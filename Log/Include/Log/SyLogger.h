#pragma once

#include "LogAPI.h"
#include <string>
#include <memory>

#if defined(_MSC_VER)
#    include <sal.h>
#endif

// ---------------------------------------------------------------------------
// printf 格式串校验宏
//
// *F 系列（LogFSrc / InfoF 等）是 C 可变参数，编译器默认不做任何检查，
// 于是 "%d 打 int64_t"、"%s 传 std::string"、"%p 传 double*" 这类错误会一直
// 潜伏到运行期（打出错误数据，32 位目标上还会错位后续实参）。
//
// GCC/Clang：__attribute__((format(printf, N, M)))，-Wformat 默认开启即生效。
//            注意成员函数的隐式 this 计为第 1 个参数，所以索引要整体 +1。
// MSVC：SAL 的 _Printf_format_string_，在 /analyze 静态分析下生效。
// ---------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
#    define SYLOG_PRINTF_FMT(fmtIndex, firstArgIndex) __attribute__((format(printf, fmtIndex, firstArgIndex)))
#else
#    define SYLOG_PRINTF_FMT(fmtIndex, firstArgIndex)
#endif

#if defined(_MSC_VER)
#    define SYLOG_FMT_STR _Printf_format_string_
#else
#    define SYLOG_FMT_STR
#endif


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

// DLL 版本查询：用于启动时检查 Log.dll 与调用方是否匹配
// 声明为 C 链接（extern "C"），与 SyLogger.cpp 中的定义保持一致
extern "C" LOG_API uint32_t SyLog_GetVersion(void);
extern "C" LOG_API const char* SyLog_GetVersionString(void);

struct SyLogConfigInternal
{
    std::string logName = "";
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
    const char* logName = "SanYiCAD";
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

// pImpl：使用原始指针而非 std::unique_ptr，避免 MSVC C4251（导出类中的
// std::unique_ptr<前置声明类型> 模板实例无法跨 DLL 边界安全销毁）
class LOG_API SyLogger
{
public:
    static SyLogger& GetInstance();
    SyLogger(const SyLogger&) = delete;
    SyLogger& operator=(const SyLogger&) = delete;

public:
    void Initialize(const SyLogConfig& config);
    void Initialize(const char* logName = "",
        SyLogLevel level = SyLogLevel::Debug,
        bool consoleEnable = true,
        bool fileEnable = true);

    void Shutdown();

    /// 立刻把异步队列里的记录落盘。
    ///
    /// 日志走 spdlog async_logger + flush_on(warn)，硬崩溃时队列中尚未写出的记录
    /// 会随进程一起消失 —— 这正是"出了问题却找不到日志"的头号来源。
    /// 崩溃处理器、致命错误分支、以及任何"接下来可能就没有下一行日志了"的位置
    /// 都应当先调一次本函数。
    /// 可重入且对未初始化的 logger 安全（此时为空操作）。
    void Flush();


    void SetLevel(SyLogLevel level);
    SyLogLevel GetLevel() const;

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    void CleanOldLogs();
    const char* GetLogDirectory() const;

    // 日志目录回调：C 函数指针 + void* ctx（避免 std::function 跨 DLL 传递）
    using LogPathCallback = const char* (*)(void* ctx);
    static void SetLogPathCallback(LogPathCallback callback, void* ctx = nullptr);
    static void SetDefaultLogPath(const char* path);
    static const char* GetDefaultLogPath();

    void TraceStr(const char* msg);
    void DebugStr(const char* msg);
    void InfoStr(const char* msg);
    void WarnStr(const char* msg);
    void ErrorStr(const char* msg);
    void CriticalStr(const char* msg);

    void TraceF(SYLOG_FMT_STR const char* fmt, ...) SYLOG_PRINTF_FMT(2, 3);
    void DebugF(SYLOG_FMT_STR const char* fmt, ...) SYLOG_PRINTF_FMT(2, 3);
    void InfoF(SYLOG_FMT_STR const char* fmt, ...) SYLOG_PRINTF_FMT(2, 3);
    void WarnF(SYLOG_FMT_STR const char* fmt, ...) SYLOG_PRINTF_FMT(2, 3);
    void ErrorF(SYLOG_FMT_STR const char* fmt, ...) SYLOG_PRINTF_FMT(2, 3);
    void CriticalF(SYLOG_FMT_STR const char* fmt, ...) SYLOG_PRINTF_FMT(2, 3);

    void LogSrc(SyLogLevel level, const char* file, int line, const char* msg);

    inline void LogSrc(SyLogLevel level, const char* file, int line, const std::string& msg)
    {
        LogSrc(level, file, line, msg.c_str());
    }

    void LogFSrc(SyLogLevel level, const char* file, int line, SYLOG_FMT_STR const char* fmt, ...)
        SYLOG_PRINTF_FMT(5, 6);

private:
    SyLogger();
    ~SyLogger();

private:
    SyLoggerImpl* m_impl;
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
