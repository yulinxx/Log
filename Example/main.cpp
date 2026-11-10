#include "Log/AprLogger.h"
#include <thread>

// ============================================================
//  AprLogger 使用示例
//  编译后执行 AprLogExample.exe，观察控制台和文件输出
//  日志文件位置：%LOCALAPPDATA%\AprLogExample\logs\
//
//  宏（推荐）：自动捕获 __FILE__ / __LINE__
//     APR_TRACE / APR_DEBUG / APR_INFO / APR_WARN / APR_ERROR / APR_CRITICAL
//     APR_TRACEF / APR_DEBUGF / ...（printf 风格格式化）
//
//  直接 API：不捕获源位置
//     TraceStr / DebugStr / InfoStr / ...
//     TraceF / DebugF / InfoF / ...
// ============================================================

int main()
{
    // ==================== 1. 初始化 ====================

    // 方式一：简易初始化
    AprLogger::GetInstance().Initialize("AprLogExample");

    // 方式二：完整配置
    // AprLogConfig config;
    // config.logName     = "AprLogExample";
    // config.level       = AprLogLevel::Trace;       // 最低级别，全部输出
    // config.consoleEnable = true;                   // 控制台彩色输出
    // config.fileEnable  = true;                     // 同时写文件
    // config.maxAgeDays  = 30;                       // 保留 30 天
    // config.splitErrorLog  = true;                  // 额外输出 .error.log
    // config.splitDebugLog  = false;                 // 不额外输出 .debug.log
    // config.maxFileSize = 10 * 1024 * 1024;         // 每 10MB 轮转
    // config.maxFiles    = 10;                       // 保留 10 个历史文件
    // config.rateLimit   = 0;                        // 不限速
    // AprLogger::GetInstance().Initialize(config);

    // ==================== 2. 输出日志 ====================

    // --- 纯字符串日志（宏自动捕获源位置） ---
    APR_TRACE("--- AprLog Usage Example ---");
    APR_DEBUG("This is a debug message");
    APR_INFO("This is an info message");
    APR_WARN("This is a warning");
    APR_ERROR("This is an error");
    APR_CRITICAL("This is critical");

    // --- printf 风格格式化日志（推荐） ---
    APR_INFOF("User '%s' logged in from %s", "admin", "192.168.1.1");
    APR_INFOF("Processed %d items in %.2f seconds", 1024, 3.14);
    APR_WARNF("Disk usage at %.1f%%, threshold is %.0f%%", 85.3, 80.0);
    APR_ERRORF("Failed to open file: %s (errno=%d)", "/path/to/data.bin", 2);

    // --- 使用直接 API（不捕获源位置） ---
    AprLogger::GetInstance().InfoStr("Direct API call (no source location)");
    AprLogger::GetInstance().InfoF("Direct printf: %s = %d", "count", 42);

    // ==================== 3. 运行时控制 ====================

    // 动态调整级别
    AprLogger::GetInstance().SetLevel(AprLogLevel::Warn);
    APR_INFO("This INFO will NOT appear (level now Warn)");  // 被过滤
    APR_WARN("This WARN will appear");                       // 通过

    // 动态启用/禁用
    AprLogger::GetInstance().SetEnabled(false);
    APR_ERROR("This ERROR will NOT appear (logging disabled)");  // 被过滤
    AprLogger::GetInstance().SetEnabled(true);
    APR_INFO("Logging re-enabled");

    // 恢复调试级别
    AprLogger::GetInstance().SetLevel(AprLogLevel::Trace);

    // ==================== 4. 多线程安全 ====================

    std::thread t1([] {
        APR_INFO("Thread 1 is working");
    });
    std::thread t2([] {
        APR_INFO("Thread 2 is working");
    });
    t1.join();
    t2.join();

    // ==================== 5. 查看日志目录 ====================

    std::string logDir = AprLogger::GetInstance().GetLogDirectory();
    APR_INFOF("Logs are stored at: %s", logDir.c_str());

    // ==================== 6. 关闭日志 ====================

    AprLogger::GetInstance().Shutdown();
    return 0;
}
