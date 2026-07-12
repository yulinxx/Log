#include "Log/SyLogger.h"

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/base_sink.h>

#include <filesystem>
#include <chrono>
#include <cstdarg>
#include <mutex>
#include <atomic>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace fs = std::filesystem;

static std::string pathToUtf8(const fs::path& p)
{
#ifdef _WIN32
    if (p.empty())
        return {};
    const std::wstring w = p.native();
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), len, nullptr, nullptr);
    return s;
#else
    return p.string();
#endif
}

static std::string g_defaultLogPath;

// ==================== 日志速率限制器 ====================
class LogRateLimiter
{
public:
    explicit LogRateLimiter(int maxPerSec)
        : m_maxPerSec(maxPerSec)
    {
    }

    bool allow()
    {
        if (m_maxPerSec <= 0)
            return true;

        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(m_mutex);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastReset).count();
        if (elapsed >= 1000)
        {
            m_count = 0;
            m_lastReset = now;
        }
        return ++m_count <= m_maxPerSec;
    }

private:
    int m_maxPerSec;
    std::mutex m_mutex;
    std::chrono::steady_clock::time_point m_lastReset{ std::chrono::steady_clock::now() };
    int m_count = 0;
};

// ==================== 级别区间过滤 Sink ====================
template<typename Mutex>
class LevelRangeSink : public spdlog::sinks::base_sink<Mutex>
{
public:
    LevelRangeSink(spdlog::sink_ptr inner,
        spdlog::level::level_enum minLevel,
        spdlog::level::level_enum maxLevel)
        : inner_(std::move(inner))
        , minLevel_(minLevel)
        , maxLevel_(maxLevel)
    {
        this->set_level(spdlog::level::trace);
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        if (msg.level >= minLevel_ && msg.level <= maxLevel_)
        {
            inner_->log(msg);
        }
    }

    void flush_() override
    {
        inner_->flush();
    }

private:
    spdlog::sink_ptr inner_;
    spdlog::level::level_enum minLevel_;
    spdlog::level::level_enum maxLevel_;
};

using LevelRangeSinkMt = LevelRangeSink<std::mutex>;

// ==================== 级别映射 ====================
static spdlog::level::level_enum ToSpdlogLevel(SyLogLevel level)
{
    switch (level)
    {
        case SyLogLevel::Trace:    return spdlog::level::trace;
        case SyLogLevel::Debug:    return spdlog::level::debug;
        case SyLogLevel::Info:     return spdlog::level::info;
        case SyLogLevel::Warn:     return spdlog::level::warn;
        case SyLogLevel::Error:    return spdlog::level::err;
        case SyLogLevel::Critical: return spdlog::level::critical;
        default:                   return spdlog::level::off;
    }
}

// ==================== 文件 Sink 辅助函数 ====================
static void AddRotatingFileSink(std::vector<spdlog::sink_ptr>& sinks,
    const std::string& filePath,
    size_t maxFileSize,
    size_t maxFiles,
    spdlog::level::level_enum minLevel = spdlog::level::trace)
{
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filePath, maxFileSize, maxFiles);
    sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%L] [%t] [%s:%#] %v");
    sink->set_level(minLevel);
    sinks.push_back(sink);
}

// ==================== 格式化字符串（优化版：栈缓冲优先） ====================
static std::string FormatString(const char* fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    char buf[256];
    int size = std::vsnprintf(buf, sizeof(buf), fmt, args_copy);
    va_end(args_copy);

    if (size < 0)
        return std::string(fmt);

    if (static_cast<size_t>(size) < sizeof(buf))
        return std::string(buf, static_cast<size_t>(size));

    std::string result(static_cast<size_t>(size), '\0');
    std::vsnprintf(result.data(), result.size() + 1, fmt, args);
    return result;
}

// ==================== pimpl 实现类 ====================
class SyLoggerImpl
{
public:
    std::shared_ptr<spdlog::logger> m_logger;
    SyLogConfigInternal m_config;
    std::atomic<bool> m_enabled{ true };
    bool m_bInitialized = false;
    mutable std::mutex m_mutex;
    std::unique_ptr<LogRateLimiter> m_rateLimiter;

    static bool s_threadPoolInitialized;
    static std::mutex s_tpMutex;

    std::string GetDefaultLogPath()
    {
        if (!g_defaultLogPath.empty())
        {
            return g_defaultLogPath;
        }

#ifdef _WIN32
        wchar_t* path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path)))
        {
            int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
            std::string result(size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], size, nullptr, nullptr);
            CoTaskMemFree(path);
            return result + "\\" + m_config.logName + "\\logs";
        }
        return ".\\logs";
#else
        const char* home = getenv("HOME");
        if (home)
        {
            return std::string(home) + "/.local/share/" + m_config.logName + "/logs";
        }
        return "./logs";
#endif
    }

    void DoCleanOldLogs(const std::string& logDir, int maxAgeDays)
    {
        try
        {
            fs::path logPath = fs::u8path(logDir);
            if (!fs::exists(logPath))
                return;

            auto now = std::chrono::system_clock::now();
            auto maxAge = std::chrono::hours(24 * maxAgeDays);

            for (const auto& entry : fs::directory_iterator(logPath))
            {
                if (!entry.is_regular_file())
                    continue;

                auto ext = entry.path().extension().string();
                if (ext != ".log" && ext != ".txt")
                    continue;

                auto fileTime = fs::last_write_time(entry);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                );

                if (now - sctp > maxAge)
                {
                    fs::remove(entry.path());
                }
            }
        }
        catch (...)
        {
        }
    }

    void ApplyLevel(SyLogLevel level)
    {
        m_config.level = level;
        if (m_logger)
            m_logger->set_level(ToSpdlogLevel(level));
    }

    void ApplyCleanOldLogs()
    {
        if (!m_config.fileEnable || m_config.logPath.empty())
            return;
        DoCleanOldLogs(m_config.logPath, m_config.maxAgeDays);
    }

    bool PrepareLogger(std::shared_ptr<spdlog::logger>& outLogger)
    {
        if (!m_enabled.load(std::memory_order_relaxed))
            return false;
        if (m_rateLimiter && !m_rateLimiter->allow())
            return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        outLogger = m_logger;
        return outLogger != nullptr;
    }
};

bool SyLoggerImpl::s_threadPoolInitialized = false;
std::mutex SyLoggerImpl::s_tpMutex;

// ==================== 单例实现 ====================
SyLogger& SyLogger::GetInstance()
{
    static SyLogger instance;
    return instance;
}

SyLogger::SyLogger() = default;
SyLogger::~SyLogger()
{
    Shutdown();
}

// ==================== 初始化 ====================
void SyLogger::Initialize(const SyLogConfig& config)
{
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);

    if (m_impl->m_bInitialized)
    {
        if (m_impl->m_logger)
        {
            m_impl->m_logger->flush();
            spdlog::drop(m_impl->m_config.logName);
            m_impl->m_logger.reset();
        }
        m_impl->m_bInitialized = false;
    }

    m_impl->m_config = static_cast<SyLogConfigInternal>(config);

    std::string logDirStr = (!config.logPath || *config.logPath == '\0') ? m_impl->GetDefaultLogPath() : config.logPath;
    m_impl->m_config.logPath = logDirStr;
    fs::path logDir = fs::u8path(logDirStr);

    try
    {
        std::vector<spdlog::sink_ptr> sinks;

        if (config.consoleEnable)
        {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
            sinks.push_back(consoleSink);
        }

        if (config.fileEnable)
        {
            fs::create_directories(logDir);

            std::string logNameStr = config.logName ? config.logName : "";
            AddRotatingFileSink(sinks, pathToUtf8(logDir / (logNameStr + ".log")), config.maxFileSize, config.maxFiles);

            if (config.splitErrorLog)
            {
                AddRotatingFileSink(sinks, pathToUtf8(logDir / (logNameStr + ".error.log")), config.maxFileSize, config.maxFiles, spdlog::level::warn);
            }

            if (config.splitDebugLog)
            {
                auto debugInner = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    pathToUtf8(logDir / (logNameStr + ".debug.log")), config.maxFileSize, config.maxFiles);
                debugInner->set_pattern("[%Y-%m-%d %H:%M:%S] [%L] [%t] [%s:%#] %v");
                debugInner->set_level(spdlog::level::trace);
                auto debugSink = std::make_shared<LevelRangeSinkMt>(
                    debugInner, spdlog::level::trace, spdlog::level::debug);
                sinks.push_back(debugSink);
            }
        }

        if (!SyLoggerImpl::s_threadPoolInitialized)
        {
            std::lock_guard<std::mutex> tpLock(SyLoggerImpl::s_tpMutex);
            if (!SyLoggerImpl::s_threadPoolInitialized)
            {
                spdlog::init_thread_pool(config.asyncQueueSize, config.asyncThreads);
                SyLoggerImpl::s_threadPoolInitialized = true;
            }
        }

        m_impl->m_logger = std::make_shared<spdlog::async_logger>(
            config.logName, sinks.begin(), sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        m_impl->ApplyLevel(config.level);

        m_impl->m_logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(m_impl->m_logger);

        m_impl->m_rateLimiter = (config.rateLimit > 0)
            ? std::make_unique<LogRateLimiter>(config.rateLimit)
            : nullptr;

        m_impl->m_bInitialized = true;
        m_impl->m_enabled.store(true, std::memory_order_relaxed);

        m_impl->ApplyCleanOldLogs();
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        m_impl->m_logger = spdlog::stdout_color_mt(config.logName);
        m_impl->m_logger->error("Log init failed: {}", ex.what());
    }
}

void SyLogger::Initialize(const char* logName, SyLogLevel level, bool consoleEnable, bool fileEnable)
{
    SyLogConfig config;
    config.logName = logName ? logName : "SanYi";
    config.level = level;
    config.consoleEnable = consoleEnable;
    config.fileEnable = fileEnable;
    Initialize(config);
}

// ==================== 关闭 ====================
void SyLogger::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    if (m_impl->m_logger)
    {
        m_impl->m_logger->flush();
        spdlog::drop(m_impl->m_config.logName);
        m_impl->m_logger.reset();
    }
    m_impl->m_bInitialized = false;

    spdlog::shutdown();
    SyLoggerImpl::s_threadPoolInitialized = false;
}

// ==================== 级别控制 ====================
void SyLogger::SetLevel(SyLogLevel level)
{
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->ApplyLevel(level);
}

SyLogLevel SyLogger::GetLevel() const
{
    return m_impl->m_config.level;
}

void SyLogger::SetEnabled(bool enabled)
{
    m_impl->m_enabled.store(enabled, std::memory_order_relaxed);
}

bool SyLogger::IsEnabled() const
{
    return m_impl->m_enabled.load(std::memory_order_relaxed);
}

const char* SyLogger::GetLogDirectory() const
{
    static std::string cachedPath;
    cachedPath = m_impl->m_config.logPath;
    return cachedPath.c_str();
}

void SyLogger::SetDefaultLogPath(const char* path)
{
    g_defaultLogPath = path ? path : "";
}

const char* SyLogger::GetDefaultLogPath()
{
    static std::string cachedPath;
    cachedPath = g_defaultLogPath;
    return cachedPath.c_str();
}

// ==================== 清理过期日志 ====================
void SyLogger::CleanOldLogs()
{
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->ApplyCleanOldLogs();
}

// ==================== 字符串日志（向后兼容） ====================
void SyLogger::TraceStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
        return;
    logger->log(spdlog::level::trace, msg ? msg : "");
}

void SyLogger::DebugStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
        return;
    logger->log(spdlog::level::debug, msg ? msg : "");
}

void SyLogger::InfoStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
        return;
    logger->log(spdlog::level::info, msg ? msg : "");
}

void SyLogger::WarnStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
        return;
    logger->log(spdlog::level::warn, msg ? msg : "");
}

void SyLogger::ErrorStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
        return;
    logger->log(spdlog::level::err, msg ? msg : "");
}

void SyLogger::CriticalStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
        return;
    logger->log(spdlog::level::critical, msg ? msg : "");
}

// ==================== 格式化日志（向后兼容） ====================
#define DEFINE_LEGACY_PRINTF_METHOD(name, spdLevel) \
void SyLogger::name(const char* fmt, ...) \
{ \
    std::shared_ptr<spdlog::logger> logger; \
    if (!m_impl->PrepareLogger(logger)) \
        return; \
    if (!logger->should_log(spdLevel)) \
        return; \
    va_list args; \
    va_start(args, fmt); \
    std::string formatted = FormatString(fmt, args); \
    va_end(args); \
    logger->log(spdLevel, formatted); \
}

DEFINE_LEGACY_PRINTF_METHOD(TraceF, spdlog::level::trace)
DEFINE_LEGACY_PRINTF_METHOD(DebugF, spdlog::level::debug)
DEFINE_LEGACY_PRINTF_METHOD(InfoF, spdlog::level::info)
DEFINE_LEGACY_PRINTF_METHOD(WarnF, spdlog::level::warn)
DEFINE_LEGACY_PRINTF_METHOD(ErrorF, spdlog::level::err)
DEFINE_LEGACY_PRINTF_METHOD(CriticalF, spdlog::level::critical)

// ==================== 带源位置的字符串日志（由宏使用） ====================
void SyLogger::LogSrc(SyLogLevel level, const char* file, int line, const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
        return;
    auto spdLevel = ToSpdlogLevel(level);
    if (!logger->should_log(spdLevel))
        return;
    if (file)
        logger->log(spdlog::source_loc{ file, line, "" }, spdLevel, msg ? msg : "");
    else
        logger->log(spdLevel, msg ? msg : "");
}

// ==================== 带源位置的格式化日志（由宏使用） ====================
void SyLogger::LogFSrc(SyLogLevel level, const char* file, int line, const char* fmt, ...)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
        return;
    auto spdLevel = ToSpdlogLevel(level);
    if (!logger->should_log(spdLevel))
        return;
    va_list args;
    va_start(args, fmt);
    std::string formatted = FormatString(fmt, args);
    va_end(args);
    if (file)
        logger->log(spdlog::source_loc{ file, line, "" }, spdLevel, formatted);
    else
        logger->log(spdLevel, formatted);
}

// ==================== C API ====================
extern "C" {
    void SyLog_Init(const char* logName, int level, bool console, bool file)
    {
        SyLogConfig config;
        config.logName = logName ? logName : "SanYi";
        config.level = static_cast<SyLogLevel>(level);
        config.consoleEnable = console;
        config.fileEnable = file;
        SyLogger::GetInstance().Initialize(config);
    }

    void SyLog_Shutdown()
    {
        SyLogger::GetInstance().Shutdown();
    }

    void SyLog_SetLevel(int level)
    {
        SyLogger::GetInstance().SetLevel(static_cast<SyLogLevel>(level));
    }

    void SyLog_SetEnabled(bool enabled)
    {
        SyLogger::GetInstance().SetEnabled(enabled);
    }

    void SyLog_Trace(const char* msg)
    {
        SyLogger::GetInstance().TraceStr(msg ? msg : "");
    }

    void SyLog_Debug(const char* msg)
    {
        SyLogger::GetInstance().DebugStr(msg ? msg : "");
    }

    void SyLog_Info(const char* msg)
    {
        SyLogger::GetInstance().InfoStr(msg ? msg : "");
    }

    void SyLog_Warn(const char* msg)
    {
        SyLogger::GetInstance().WarnStr(msg ? msg : "");
    }

    void SyLog_Error(const char* msg)
    {
        SyLogger::GetInstance().ErrorStr(msg ? msg : "");
    }

    void SyLog_Critical(const char* msg)
    {
        SyLogger::GetInstance().CriticalStr(msg ? msg : "");
    }
} // extern "C"

void SyLogger::Initialize(const std::string& logName, SyLogLevel level, bool consoleEnable, bool fileEnable)
{
    Initialize(logName.c_str(), level, consoleEnable, fileEnable);
}