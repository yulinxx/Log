#include "Log/SyLogger.h"

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/base_sink.h>

#include <filesystem>
#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <mutex>
#include <atomic>
#include <thread>

#ifdef _WIN32
    #include <Windows.h>
    #include <ShlObj.h>
#endif

namespace fs = std::filesystem;

constexpr uint32_t kSyLogVersion = 0x010000;
constexpr const char* kSyLogVersionString = "1.0.0";

extern "C" LOG_API uint32_t SyLog_GetVersion(void)
{
    return kSyLogVersion;
}

extern "C" LOG_API const char* SyLog_GetVersionString(void)
{
    return kSyLogVersionString;
}

// Log 是基础模块，不依赖 Utility DLL；pathToUtf8 与 Ut::FileUtils::pathToUtf8 实现一致。
static std::string pathToUtf8(const fs::path& p)
{
#ifdef _WIN32
    if (p.empty())
    {
        return {};
    }
    const std::wstring w = p.native();
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
    {
        return {};
    }
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), len, nullptr, nullptr);
    return s;
#else
    return p.string();
#endif
}

static std::string g_defaultLogPath;
static SyLogger::LogPathCallback g_logPathCallback = nullptr;
static void* g_logPathCallbackCtx = nullptr;

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
        {
            return true;
        }

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
    LevelRangeSink(spdlog::sink_ptr inner, spdlog::level::level_enum minLevel, spdlog::level::level_enum maxLevel)
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
    case SyLogLevel::Trace:
        return spdlog::level::trace;
    case SyLogLevel::Debug:
        return spdlog::level::debug;
    case SyLogLevel::Info:
        return spdlog::level::info;
    case SyLogLevel::Warn:
        return spdlog::level::warn;
    case SyLogLevel::Error:
        return spdlog::level::err;
    case SyLogLevel::Critical:
        return spdlog::level::critical;
    default:
        return spdlog::level::off;
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
    {
        return std::string(fmt);
    }

    if (static_cast<size_t>(size) < sizeof(buf))
    {
        return std::string(buf, static_cast<size_t>(size));
    }

    // 超过栈缓冲：重新拷贝 va_list 做第二次格式化
    // 注意：不能复用已被消费的原始 args，必须重新 va_copy
    va_copy(args_copy, args);
    std::string result(static_cast<size_t>(size), '\0');
    std::vsnprintf(result.data(), result.size() + 1, fmt, args_copy);
    va_end(args_copy);
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

    // 轻量析构：仅在未显式 Shutdown() 的异常退出路径兜底释放 logger。
    // 不调用 spdlog::shutdown()/drop 与 logger->flush() —— 前者访问 registry
    // 全局静态对象（进程静态销毁阶段访问可能触发 terminate），后者 async 路径
    // 会向 thread_pool 投递（若线程池已先析构则落入 error handler）。
    // 仅 reset() 让 async_logger 析构：它不访问 registry，只释放自身成员，
    // 文件 sink 析构时自行 flush 关闭文件，全程不碰 spdlog 静态对象，安全。
    ~SyLoggerImpl()
    {
        if (m_logger && m_bInitialized)
        {
            m_logger.reset();
        }
    }

    static bool s_threadPoolInitialized;
    static std::mutex s_tpMutex;

    std::string GetDefaultLogPath()
    {
        if (g_logPathCallback)
        {
            const char* path = g_logPathCallback(g_logPathCallbackCtx);
            if (path && *path)
            {
                return std::string(path);
            }
        }

        if (!g_defaultLogPath.empty())
        {
            return g_defaultLogPath;
        }

        const std::string appName = m_config.logName.empty() ? "SanYiCAD" : m_config.logName;

#ifdef _WIN32
        wchar_t* path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path)))
        {
            int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
            std::string result(size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], size, nullptr, nullptr);
            CoTaskMemFree(path);
            return result + "\\" + appName + "\\logs";
        }
        return ".\\logs";
#else
        const char* home = getenv("HOME");
        if (home)
        {
            return std::string(home) + "/.local/share/" + appName + "/logs";
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
            {
                return;
            }

            auto now = std::chrono::system_clock::now();
            auto maxAge = std::chrono::hours(24 * maxAgeDays);

            for (const auto& entry : fs::directory_iterator(logPath))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                auto ext = entry.path().extension().string();
                if (ext != ".log" && ext != ".txt")
                {
                    continue;
                }

                auto fileTime = fs::last_write_time(entry);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());

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
        {
            m_logger->set_level(ToSpdlogLevel(level));
        }
    }

    void ApplyCleanOldLogs()
    {
        if (!m_config.fileEnable || m_config.logPath.empty())
        {
            return;
        }
        DoCleanOldLogs(m_config.logPath, m_config.maxAgeDays);
    }

    bool PrepareLogger(std::shared_ptr<spdlog::logger>& outLogger)
    {
        if (!m_enabled.load(std::memory_order_relaxed))
        {
            return false;
        }
        if (m_rateLimiter && !m_rateLimiter->allow())
        {
            return false;
        }
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

SyLogger::SyLogger()
    : m_impl(new SyLoggerImpl())
{
}

SyLogger::~SyLogger()
{
    // 注意：此处不能调用 Shutdown()。
    // Shutdown() 内部会访问 spdlog 的全局静态对象（registry / thread pool），
    // 而单例析构发生在进程静态对象销毁阶段（exit() -> __cxa_finalize），
    // 此时 spdlog 内部的静态对象可能已被先销毁，访问其 mutex 会触发
    // “mutex lock failed: Invalid argument” 并导致 std::terminate。
    // 真正的关闭流程由 AppInitializer::shutdown() 显式调用 Shutdown() 完成。
    delete m_impl;
    m_impl = nullptr;
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
                AddRotatingFileSink(sinks,
                    pathToUtf8(logDir / (logNameStr + ".error.log")),
                    config.maxFileSize,
                    config.maxFiles,
                    spdlog::level::warn);
            }

            if (config.splitDebugLog)
            {
                auto debugInner = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    pathToUtf8(logDir / (logNameStr + ".debug.log")), config.maxFileSize, config.maxFiles);
                debugInner->set_pattern("[%Y-%m-%d %H:%M:%S] [%L] [%t] [%s:%#] %v");
                debugInner->set_level(spdlog::level::trace);
                auto debugSink =
                    std::make_shared<LevelRangeSinkMt>(debugInner, spdlog::level::trace, spdlog::level::debug);
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
            config.logName, sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);

        m_impl->ApplyLevel(config.level);

        m_impl->m_logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(m_impl->m_logger);

        m_impl->m_rateLimiter = (config.rateLimit > 0) ? std::make_unique<LogRateLimiter>(config.rateLimit) : nullptr;

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
    config.logName = logName ? logName : "SanYiCAD";
    config.level = level;
    config.consoleEnable = consoleEnable;
    config.fileEnable = fileEnable;
    Initialize(config);
}

// ==================== 关闭 ====================
void SyLogger::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    if (!m_impl->m_bInitialized)
    {
        return;  // 幂等：已关闭则直接返回，避免重复调用 spdlog::shutdown()
    }
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

void SyLogger::Flush()
{
    // 不加锁：本函数的主要调用者是崩溃处理器，此时其他线程可能正持着 m_mutex
    // 且永远不会再释放（比如崩在临界区里），加锁等于把崩溃现场变成死锁。
    // spdlog 的 logger::flush() 自身是线程安全的，最坏情况是与并发写入交错，
    // 而这远好过丢掉整段日志。
    if (m_impl && m_impl->m_logger)
    {
        m_impl->m_logger->flush();
    }
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
    // 返回指向内部数据的指针：调用方应在锁外尽快拷贝。
    // 不使用 static 局部变量——多线程并发写同一 static 是 data race。
    return m_impl->m_config.logPath.c_str();
}

void SyLogger::SetLogPathCallback(LogPathCallback callback, void* ctx)
{
    g_logPathCallback = callback;
    g_logPathCallbackCtx = ctx;
}

void SyLogger::SetDefaultLogPath(const char* path)
{
    g_defaultLogPath = path ? path : "";
}

const char* SyLogger::GetDefaultLogPath()
{
    // 返回指向全局变量的指针：调用方应在使用后尽快拷贝，不要长期持有。
    return g_defaultLogPath.c_str();
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
    {
        return;
    }
    logger->log(spdlog::level::trace, msg ? msg : "");
}

void SyLogger::DebugStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
    {
        return;
    }
    logger->log(spdlog::level::debug, msg ? msg : "");
}

void SyLogger::InfoStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
    {
        return;
    }
    logger->log(spdlog::level::info, msg ? msg : "");
}

void SyLogger::WarnStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
    {
        return;
    }
    logger->log(spdlog::level::warn, msg ? msg : "");
}

void SyLogger::ErrorStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
    {
        return;
    }
    logger->log(spdlog::level::err, msg ? msg : "");
}

void SyLogger::CriticalStr(const char* msg)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
    {
        return;
    }
    logger->log(spdlog::level::critical, msg ? msg : "");
}

// ==================== 格式化日志（向后兼容） ====================
#define DEFINE_LEGACY_PRINTF_METHOD(name, spdLevel)      \
    void SyLogger::name(const char* fmt, ...)            \
    {                                                    \
        std::shared_ptr<spdlog::logger> logger;          \
        if (!m_impl->PrepareLogger(logger))              \
            return;                                      \
        if (!logger->should_log(spdLevel))               \
            return;                                      \
        va_list args;                                    \
        va_start(args, fmt);                             \
        std::string formatted = FormatString(fmt, args); \
        va_end(args);                                    \
        logger->log(spdLevel, formatted);                \
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
    {
        return;
    }
    auto spdLevel = ToSpdlogLevel(level);
    if (!logger->should_log(spdLevel))
    {
        return;
    }
    if (file)
    {
        logger->log(spdlog::source_loc{ file, line, "" }, spdLevel, msg ? msg : "");
    }
    else
    {
        logger->log(spdLevel, msg ? msg : "");
    }
}

// ==================== 带源位置的格式化日志（由宏使用） ====================
void SyLogger::LogFSrc(SyLogLevel level, const char* file, int line, const char* fmt, ...)
{
    std::shared_ptr<spdlog::logger> logger;
    if (!m_impl->PrepareLogger(logger))
    {
        return;
    }
    auto spdLevel = ToSpdlogLevel(level);
    if (!logger->should_log(spdLevel))
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    std::string formatted = FormatString(fmt, args);
    va_end(args);
    if (file)
    {
        logger->log(spdlog::source_loc{ file, line, "" }, spdLevel, formatted);
    }
    else
    {
        logger->log(spdLevel, formatted);
    }
}