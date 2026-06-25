#include "Log/SyLogger.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/base_sink.h>

#include <filesystem>
#include <chrono>
#include <cstdarg>
#include <mutex>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace fs = std::filesystem;

static std::string g_defaultLogPath;

// 仅转发指定级别区间的日志到内层 sink（用于 Debug 分流）
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

static void AddDailyFileSink(std::vector<spdlog::sink_ptr>& sinks,
                             const std::string& filePath,
                             spdlog::level::level_enum minLevel = spdlog::level::trace)
{
    auto sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(filePath, 0, 0);
    sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%L] [%t] %v");
    sink->set_level(minLevel);
    sinks.push_back(sink);
}

// ==================== pimpl 实现类 ====================
class SyLoggerImpl
{
public:
    std::shared_ptr<spdlog::logger> m_logger;
    SyLogConfig m_config;
    bool m_enabled = true;
    bool m_bInitialized = false;

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
            if (!fs::exists(logDir))
                return;

            auto now = std::chrono::system_clock::now();
            auto maxAge = std::chrono::hours(24 * maxAgeDays);

            for (const auto& entry : fs::directory_iterator(logDir))
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
                    if (m_logger)
                    {
                        m_logger->debug("Deleted old log: {}", entry.path().string());
                    }
                }
            }
        }
        catch (...)
        {
            // 忽略清理错误
        }
    }
};

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
    if (m_impl->m_bInitialized)
    {
        Shutdown();
    }

    m_impl->m_config = config;

    // 日志目录
    std::string logDir = config.logPath.empty() ? m_impl->GetDefaultLogPath() : config.logPath;
    m_impl->m_config.logPath = logDir;

    try
    {
        std::vector<spdlog::sink_ptr> sinks;

        // 控制台输出（彩色）
        if (config.consoleEnable)
        {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
            sinks.push_back(consoleSink);
        }

        // 文件输出（按天生成）
        if (config.fileEnable)
        {
            fs::create_directories(logDir);

            // 主日志：全部级别，按时间线完整记录
            const std::string logFile = (fs::path(logDir) / (config.logName + ".log")).string();
            AddDailyFileSink(sinks, logFile);

            // 错误分流：Warn / Error / Critical
            if (config.splitErrorLog)
            {
                const std::string errorFile =
                    (fs::path(logDir) / (config.logName + ".error.log")).string();
                AddDailyFileSink(sinks, errorFile, spdlog::level::warn);
            }

            // Debug 分流：Trace / Debug（可选，避免主文件被大量调试日志淹没）
            if (config.splitDebugLog)
            {
                const std::string debugFile =
                    (fs::path(logDir) / (config.logName + ".debug.log")).string();
                auto debugInner = std::make_shared<spdlog::sinks::daily_file_sink_mt>(debugFile, 0, 0);
                debugInner->set_pattern("[%Y-%m-%d %H:%M:%S] [%L] [%t] %v");
                debugInner->set_level(spdlog::level::trace);
                auto debugSink = std::make_shared<LevelRangeSinkMt>(
                    debugInner, spdlog::level::trace, spdlog::level::debug);
                sinks.push_back(debugSink);
            }
        }

        // 创建 logger
        m_impl->m_logger = std::make_shared<spdlog::logger>(config.logName, sinks.begin(), sinks.end());

        // 设置日志级别
        SetLevel(config.level);

        // warn 及以上自动 flush
        m_impl->m_logger->flush_on(spdlog::level::warn);

        // 注册为默认 logger
        spdlog::set_default_logger(m_impl->m_logger);

        m_impl->m_bInitialized = true;
        m_impl->m_enabled = true;

        // 清理过期日志
        CleanOldLogs();

        m_impl->m_logger->info("=== SyLogger Initialized ===");
        m_impl->m_logger->info("Log Directory: {}", logDir);
        m_impl->m_logger->info("Main log: {}/{}.log", logDir, config.logName);
        if (config.splitErrorLog)
        {
            m_impl->m_logger->info("Error log: {}/{}.error.log (WARN+)", logDir, config.logName);
        }
        if (config.splitDebugLog)
        {
            m_impl->m_logger->info("Debug log: {}/{}.debug.log (TRACE/DEBUG)", logDir, config.logName);
        }
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        // 失败时创建控制台 logger
        m_impl->m_logger = spdlog::stdout_color_mt(config.logName);
        m_impl->m_logger->error("Log init failed: {}", ex.what());
    }
}

void SyLogger::Initialize(const std::string& logName, SyLogLevel level, bool consoleEnable, bool fileEnable)
{
    SyLogConfig config;
    config.logName = logName;
    config.level = level;
    config.consoleEnable = consoleEnable;
    config.fileEnable = fileEnable;
    Initialize(config);
}

// ==================== 关闭 ====================
void SyLogger::Shutdown()
{
    if (m_impl->m_logger)
    {
        m_impl->m_logger->info("=== SyLogger Shutdown ===\n\n");
        m_impl->m_logger->flush();
        spdlog::drop(m_impl->m_config.logName);
        m_impl->m_logger.reset();
    }
    m_impl->m_bInitialized = false;
}

// ==================== 级别控制 ====================
void SyLogger::SetLevel(SyLogLevel level)
{
    m_impl->m_config.level = level;
    if (m_impl->m_logger)
    {
        spdlog::level::level_enum spdLevel;
        switch (level)
        {
            case SyLogLevel::Trace:    spdLevel = spdlog::level::trace; break;
            case SyLogLevel::Debug:    spdLevel = spdlog::level::debug; break;
            case SyLogLevel::Info:     spdLevel = spdlog::level::info; break;
            case SyLogLevel::Warn:     spdLevel = spdlog::level::warn; break;
            case SyLogLevel::Error:    spdLevel = spdlog::level::err; break;
            case SyLogLevel::Critical: spdLevel = spdlog::level::critical; break;
            default:                   spdLevel = spdlog::level::off; break;
        }
        m_impl->m_logger->set_level(spdLevel);
    }
}

SyLogLevel SyLogger::GetLevel() const
{
    return m_impl->m_config.level;
}

void SyLogger::SetEnabled(bool enabled)
{
    m_impl->m_enabled = enabled;
}

bool SyLogger::IsEnabled() const
{
    return m_impl->m_enabled;
}

std::string SyLogger::GetLogDirectory() const
{
    return m_impl->m_config.logPath;
}

void SyLogger::SetDefaultLogPath(const std::string& path)
{
    g_defaultLogPath = path;
}

std::string SyLogger::GetDefaultLogPath()
{
    return g_defaultLogPath;
}

// ==================== 清理过期日志 ====================
void SyLogger::CleanOldLogs()
{
    if (!m_impl->m_config.fileEnable || m_impl->m_config.logPath.empty())
        return;

    m_impl->DoCleanOldLogs(m_impl->m_config.logPath, m_impl->m_config.maxAgeDays);
}

// ==================== 字符串日志 ====================
void SyLogger::TraceStr(const std::string& msg)
{
    if (m_impl->m_enabled && m_impl->m_logger)
        m_impl->m_logger->trace(msg);
}
void SyLogger::DebugStr(const std::string& msg)
{
    if (m_impl->m_enabled && m_impl->m_logger)
        m_impl->m_logger->debug(msg);
}
void SyLogger::InfoStr(const std::string& msg)
{
    if (m_impl->m_enabled && m_impl->m_logger)
        m_impl->m_logger->info(msg);
}
void SyLogger::WarnStr(const std::string& msg)
{
    if (m_impl->m_enabled && m_impl->m_logger)
        m_impl->m_logger->warn(msg);
}
void SyLogger::ErrorStr(const std::string& msg)
{
    if (m_impl->m_enabled && m_impl->m_logger)
        m_impl->m_logger->error(msg);
}
void SyLogger::CriticalStr(const std::string& msg)
{
    if (m_impl->m_enabled && m_impl->m_logger)
        m_impl->m_logger->critical(msg);
}

// ==================== 格式化日志 (printf风格) ====================
static std::string FormatString(const char* fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0)
        return std::string(fmt);

    std::string result(size + 1, '\0');
    std::vsnprintf(&result[0], result.size(), fmt, args);
    result.resize(size);
    return result;
}

void SyLogger::TraceF(const char* fmt, ...)
{
    if (!m_impl->m_enabled || !m_impl->m_logger)
        return;
    va_list args;
    va_start(args, fmt);
    m_impl->m_logger->trace(FormatString(fmt, args));
    va_end(args);
}

void SyLogger::DebugF(const char* fmt, ...)
{
    if (!m_impl->m_enabled || !m_impl->m_logger)
        return;
    va_list args;
    va_start(args, fmt);
    m_impl->m_logger->debug(FormatString(fmt, args));
    va_end(args);
}

void SyLogger::InfoF(const char* fmt, ...)
{
    if (!m_impl->m_enabled || !m_impl->m_logger)
        return;
    va_list args;
    va_start(args, fmt);
    m_impl->m_logger->info(FormatString(fmt, args));
    va_end(args);
}

void SyLogger::WarnF(const char* fmt, ...)
{
    if (!m_impl->m_enabled || !m_impl->m_logger)
        return;
    va_list args;
    va_start(args, fmt);
    m_impl->m_logger->warn(FormatString(fmt, args));
    va_end(args);
}

void SyLogger::ErrorF(const char* fmt, ...)
{
    if (!m_impl->m_enabled || !m_impl->m_logger)
        return;
    va_list args;
    va_start(args, fmt);
    m_impl->m_logger->error(FormatString(fmt, args));
    va_end(args);
}

void SyLogger::CriticalF(const char* fmt, ...)
{
    if (!m_impl->m_enabled || !m_impl->m_logger)
        return;
    va_list args;
    va_start(args, fmt);
    m_impl->m_logger->critical(FormatString(fmt, args));
    va_end(args);
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