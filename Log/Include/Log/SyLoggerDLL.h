#pragma once

#include "LogAPI.h"
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== DLL 接口 ====================
// 这些函数可以在不同编译器和运行时的进程间安全调用

// 日志系统初始化
LOG_API void SyLoggerDLL_Initialize(const char* logName, int level, bool console, bool file);
LOG_API void SyLoggerDLL_InitializeWithConfig(const SyLogConfig* config);

// 日志系统控制
LOG_API void SyLoggerDLL_Shutdown();
LOG_API void SyLoggerDLL_SetLevel(int level);
LOG_API void SyLoggerDLL_SetEnabled(bool enabled);
LOG_API bool SyLoggerDLL_IsEnabled();
LOG_API void SyLoggerDLL_CleanOldLogs();
LOG_API const char* SyLoggerDLL_GetLogDirectory();

// 默认路径设置
LOG_API void SyLoggerDLL_SetDefaultLogPath(const char* path);
LOG_API const char* SyLoggerDLL_GetDefaultLogPath();

// 简单日志记录
LOG_API void SyLoggerDLL_Trace(const char* msg);
LOG_API void SyLoggerDLL_Debug(const char* msg);
LOG_API void SyLoggerDLL_Info(const char* msg);
LOG_API void SyLoggerDLL_Warn(const char* msg);
LOG_API void SyLoggerDLL_Error(const char* msg);
LOG_API void SyLoggerDLL_Critical(const char* msg);

// 格式化日志记录
LOG_API void SyLoggerDLL_TraceF(const char* fmt, ...);
LOG_API void SyLoggerDLL_DebugF(const char* fmt, ...);
LOG_API void SyLoggerDLL_InfoF(const char* fmt, ...);
LOG_API void SyLoggerDLL_WarnF(const char* fmt, ...);
LOG_API void SyLoggerDLL_ErrorF(const char* fmt, ...);
LOG_API void SyLoggerDLL_CriticalF(const char* fmt, ...);

// 带源代码位置的日志记录
LOG_API void SyLoggerDLL_LogSrc(int level, const char* file, int line, const char* msg);
LOG_API void SyLoggerDLL_LogFSrc(int level, const char* file, int line, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

// ==================== C++ 包装器 ====================
#ifdef __cplusplus

#include <memory>
#include <functional>

// C++ DLL 包装器类
class LOG_API SyLoggerDLLWrapper
{
public:
    // 单例访问
    static SyLoggerDLLWrapper& GetInstance();

    // 初始化
    void Initialize(const char* logName = "SanYi",
                   int level = 1, // Debug
                   bool consoleEnable = true,
                   bool fileEnable = true);
    void Initialize(const std::string& logName,
                   int level = 1,
                   bool consoleEnable = true,
                   bool fileEnable = true);
    void Initialize(const SyLogConfig& config);

    // 控制
    void Shutdown();
    void SetLevel(int level);
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void CleanOldLogs();
    std::string GetLogDirectory() const;

    // 日志记录
    void Trace(const char* msg);
    void Debug(const char* msg);
    void Info(const char* msg);
    void Warn(const char* msg);
    void Error(const char* msg);
    void Critical(const char* msg);

    // 格式化日志
    void TraceF(const char* fmt, ...);
    void DebugF(const char* fmt, ...);
    void InfoF(const char* fmt, ...);
    void WarnF(const char* fmt, ...);
    void ErrorF(const char* fmt, ...);
    void CriticalF(const char* fmt, ...);

    // 带源位置的日志
    void LogSrc(int level, const char* file, int line, const char* msg);
    void LogFSrc(int level, const char* file, int line, const char* fmt, ...);

private:
    SyLoggerDLLWrapper();
    ~SyLoggerDLLWrapper();
    SyLoggerDLLWrapper(const SyLoggerDLLWrapper&) = delete;
    SyLoggerDLLWrapper& operator=(const SyLoggerDLLWrapper&) = delete;
};

#endif // __cplusplus