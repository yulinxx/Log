#pragma once

#include "LogAPI.h"
#include <string>
#include <memory>

// ==================== 日志级别枚举 ====================
/**
 * @brief 日志级别枚举，定义日志消息的严重程度
 *
 * 日志级别从低到高排列，级别越高表示问题越严重。
 * 设置某个级别后，只有该级别及更高级别的日志会被记录。
 */
enum class SyLogLevel
{
    Trace = 0,    ///< 跟踪级别：最详细的调试信息，用于跟踪程序执行流程
    Debug = 1,    ///< 调试级别：开发调试信息，用于问题排查
    Info = 2,     ///< 信息级别：常规运行信息，用于了解程序运行状态
    Warn = 3,     ///< 警告级别：潜在问题警告，不影响程序正常运行
    Error = 4,    ///< 错误级别：错误信息，影响部分功能但程序仍可运行
    Critical = 5, ///< 严重级别：严重错误，可能导致程序崩溃
    Off = 6       ///< 关闭级别：关闭所有日志输出
};

///////////////////////////////////////////////////////
// ==================== 日志配置结构 ====================
struct LOG_API SyLogConfig
{
    std::string logName = "SanYi";              // 日志名称
    std::string logPath = "";                   // 日志目录（空则使用默认路径）
    SyLogLevel level = SyLogLevel::Debug;       // 日志级别
    bool consoleEnable = true;                  // 控制台输出
    bool fileEnable = true;                     // 文件输出
    int maxAgeDays = 30;                        // 日志保留天数
    bool splitErrorLog = true;                  // 额外写入 {logName}.error.log（Warn 及以上）
    bool splitDebugLog = false;                 // 额外写入 {logName}.debug.log（Trace/Debug）
};

class SyLoggerImpl;

///////////////////////////////////////////////////////
// ==================== 日志库核心类 ====================
class LOG_API SyLogger
{
public:
    static SyLogger& GetInstance();

    // 使用配置结构初始化
    void Initialize(const SyLogConfig& config);

    // 初始化
    void Initialize(const std::string& logName = "SanYi",
        SyLogLevel level = SyLogLevel::Debug,
        bool consoleEnable = true,
        bool fileEnable = true);

    // 关闭日志系统
    void Shutdown();

    // 设置/获取日志级别
    void SetLevel(SyLogLevel level);
    SyLogLevel GetLevel() const;

    // 启用/禁用日志
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // 清理过期日志
    void CleanOldLogs();

    // 获取日志文件目录
    std::string GetLogDirectory() const;

    // 设置默认日志路径（用于统一路径管理）
    static void SetDefaultLogPath(const std::string& path);
    static std::string GetDefaultLogPath();

    // ==================== 字符串日志 ====================
    void TraceStr(const std::string& msg);
    void DebugStr(const std::string& msg);
    void InfoStr(const std::string& msg);
    void WarnStr(const std::string& msg);
    void ErrorStr(const std::string& msg);
    void CriticalStr(const std::string& msg);

    // ==================== 格式化日志 (printf风格) ====================
    void TraceF(const char* fmt, ...);
    void DebugF(const char* fmt, ...);
    void InfoF(const char* fmt, ...);
    void WarnF(const char* fmt, ...);
    void ErrorF(const char* fmt, ...);
    void CriticalF(const char* fmt, ...);

private:
    SyLogger();
    ~SyLogger();
    SyLogger(const SyLogger&) = delete;
    SyLogger& operator=(const SyLogger&) = delete;

    std::unique_ptr<SyLoggerImpl> m_impl = std::make_unique<SyLoggerImpl>();  // pimpl 模式
};

//////////////////////////////////////////////////////
// ==================== 便捷宏定义 ====================
// 字符串日志
#define SY_TRACE(msg)    SyLogger::GetInstance().TraceStr(msg)
#define SY_DEBUG(msg)    SyLogger::GetInstance().DebugStr(msg)
#define SY_INFO(msg)     SyLogger::GetInstance().InfoStr(msg)
#define SY_WARN(msg)     SyLogger::GetInstance().WarnStr(msg)
#define SY_ERROR(msg)    SyLogger::GetInstance().ErrorStr(msg)
#define SY_CRITICAL(msg) SyLogger::GetInstance().CriticalStr(msg)

// 格式化日志 (printf 风格)
// 用法: SY_INFOF("Hello %s, count=%d", name, 123);
#define SY_TRACEF(...)    SyLogger::GetInstance().TraceF(__VA_ARGS__)
#define SY_DEBUGF(...)    SyLogger::GetInstance().DebugF(__VA_ARGS__)
#define SY_INFOF(...)     SyLogger::GetInstance().InfoF(__VA_ARGS__)
#define SY_WARNF(...)     SyLogger::GetInstance().WarnF(__VA_ARGS__)
#define SY_ERRORF(...)    SyLogger::GetInstance().ErrorF(__VA_ARGS__)
#define SY_CRITICALF(...) SyLogger::GetInstance().CriticalF(__VA_ARGS__)

///////////////////////////////////////////////////////////////////////
// ==================== C API ====================
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
