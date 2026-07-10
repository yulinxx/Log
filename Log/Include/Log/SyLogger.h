#pragma once

#include "LogAPI.h"
#include <string>
#include <memory>

enum class SyLogLevel
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};

struct SyLogConfigInternal
{
    std::string logName = "SanYi";
    std::string logPath = "";
    SyLogLevel level = SyLogLevel::Debug;
    bool consoleEnable = true;
    bool fileEnable = true;
    int maxAgeDays = 30;
    bool splitErrorLog = true;
    bool splitDebugLog = false;

    size_t maxFileSize = 10 * 1024 * 1024;
    size_t maxFiles = 10;

    int rateLimit = 0;

    size_t asyncQueueSize = 8192;
    size_t asyncThreads = 1;
};

struct LOG_API SyLogConfig
{
    const char* logName = "SanYi";
    const char* logPath = "";
    SyLogLevel level = SyLogLevel::Debug;
    bool consoleEnable = true;
    bool fileEnable = true;
    int maxAgeDays = 30;
    bool splitErrorLog = true;
    bool splitDebugLog = false;

    size_t maxFileSize = 10 * 1024 * 1024;
    size_t maxFiles = 10;

    int rateLimit = 0;

    size_t asyncQueueSize = 8192;
    size_t asyncThreads = 1;

    operator SyLogConfigInternal() const
    {
        SyLogConfigInternal internal;
        internal.logName = logName ? logName : "";
        internal.logPath = logPath ? logPath : "";
        internal.level = level;
        internal.consoleEnable = consoleEnable;
        internal.fileEnable = fileEnable;
        internal.maxAgeDays = maxAgeDays;
        internal.splitErrorLog = splitErrorLog;
        internal.splitDebugLog = splitDebugLog;
        internal.maxFileSize = maxFileSize;
        internal.maxFiles = maxFiles;
        internal.rateLimit = rateLimit;
        internal.asyncQueueSize = asyncQueueSize;
        internal.asyncThreads = asyncThreads;
        return internal;
    }
};

class SyLoggerImpl;

class LOG_API SyLogger
{
public:
    static SyLogger& GetInstance();

    void Initialize(const SyLogConfig& config);
    void Initialize(const char* logName = "SanYi",
        SyLogLevel level = SyLogLevel::Debug,
        bool consoleEnable = true,
        bool fileEnable = true);
    void Initialize(const std::string& logName,
        SyLogLevel level = SyLogLevel::Debug,
        bool consoleEnable = true,
        bool fileEnable = true);

    void Shutdown();

    void SetLevel(SyLogLevel level);
    SyLogLevel GetLevel() const;

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    void CleanOldLogs();
    const char* GetLogDirectory() const;

    static void SetDefaultLogPath(const char* path);
    static const char* GetDefaultLogPath();

    void TraceStr(const char* msg);
    void DebugStr(const char* msg);
    void InfoStr(const char* msg);
    void WarnStr(const char* msg);
    void ErrorStr(const char* msg);
    void CriticalStr(const char* msg);

    void TraceF(const char* fmt, ...);
    void DebugF(const char* fmt, ...);
    void InfoF(const char* fmt, ...);
    void WarnF(const char* fmt, ...);
    void ErrorF(const char* fmt, ...);
    void CriticalF(const char* fmt, ...);

    void LogSrc(SyLogLevel level, const char* file, int line, const char* msg);
    void LogFSrc(SyLogLevel level, const char* file, int line, const char* fmt, ...);

private:
    SyLogger();
    ~SyLogger();
    SyLogger(const SyLogger&) = delete;
    SyLogger& operator=(const SyLogger&) = delete;

    std::unique_ptr<SyLoggerImpl> m_impl = std::make_unique<SyLoggerImpl>();
};

#define SY_TRACE(msg)    SyLogger::GetInstance().LogSrc(SyLogLevel::Trace, __FILE__, __LINE__, msg)
#define SY_DEBUG(msg)    SyLogger::GetInstance().LogSrc(SyLogLevel::Debug, __FILE__, __LINE__, msg)
#define SY_INFO(msg)     SyLogger::GetInstance().LogSrc(SyLogLevel::Info, __FILE__, __LINE__, msg)
#define SY_WARN(msg)     SyLogger::GetInstance().LogSrc(SyLogLevel::Warn, __FILE__, __LINE__, msg)
#define SY_ERROR(msg)    SyLogger::GetInstance().LogSrc(SyLogLevel::Error, __FILE__, __LINE__, msg)
#define SY_CRITICAL(msg) SyLogger::GetInstance().LogSrc(SyLogLevel::Critical, __FILE__, __LINE__, msg)

#define SY_TRACEF(...)    SyLogger::GetInstance().LogFSrc(SyLogLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define SY_DEBUGF(...)    SyLogger::GetInstance().LogFSrc(SyLogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define SY_INFOF(...)     SyLogger::GetInstance().LogFSrc(SyLogLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
#define SY_WARNF(...)     SyLogger::GetInstance().LogFSrc(SyLogLevel::Warn, __FILE__, __LINE__, __VA_ARGS__)
#define SY_ERRORF(...)    SyLogger::GetInstance().LogFSrc(SyLogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#define SY_CRITICALF(...) SyLogger::GetInstance().LogFSrc(SyLogLevel::Critical, __FILE__, __LINE__, __VA_ARGS__)

extern "C" {
    LOG_API void SyLog_Init(const char* logName, int level, bool console, bool file);
    LOG_API void SyLog_Shutdown();
    LOG_API void SyLog_SetLevel(int level);
    LOG_API void SyLog_SetEnabled(bool enabled);

    LOG_API void SyLog_Trace(const char* msg);
    LOG_API void SyLog_Debug(const char* msg);
    LOG_API void SyLog_Info(const char* msg);
    LOG_API void SyLog_Warn(const char* msg);
    LOG_API void SyLog_Error(const char* msg);
    LOG_API void SyLog_Critical(const char* msg);
}
