#include "Log/SyLogger.h"
#include <thread>

// ============================================================
//  SyLogger 使用示例
//  编译后执行 SyLogExample.exe，观察控制台和文件输出
//  日志文件位置：%LOCALAPPDATA%\SyLogExample\logs\
//
//  宏（推荐）：自动捕获 __FILE__ / __LINE__
//     SY_TRACE / SY_DEBUG / SY_INFO / SY_WARN / SY_ERROR / SY_CRITICAL
//     SY_TRACEF / SY_DEBUGF / ...（printf 风格格式化）
//
//  直接 API：不捕获源位置
//     TraceStr / DebugStr / InfoStr / ...
//     TraceF / DebugF / InfoF / ...
// ============================================================

int main()
{
    // ==================== 1. 初始化 ====================

    // 方式一：简易初始化
    SyLogger::GetInstance().Initialize("SyLogExample");

    // 方式二：完整配置
    // SyLogConfig config;
    // config.logName     = "SyLogExample";
    // config.level       = SyLogLevel::Trace;       // 最低级别，全部输出
    // config.consoleEnable = true;                   // 控制台彩色输出
    // config.fileEnable  = true;                     // 同时写文件
    // config.maxAgeDays  = 30;                       // 保留 30 天
    // config.splitErrorLog  = true;                  // 额外输出 .error.log
    // config.splitDebugLog  = false;                 // 不额外输出 .debug.log
    // config.maxFileSize = 10 * 1024 * 1024;         // 每 10MB 轮转
    // config.maxFiles    = 10;                       // 保留 10 个历史文件
    // config.rateLimit   = 0;                        // 不限速
    // SyLogger::GetInstance().Initialize(config);


    // ==================== 2. 输出日志 ====================

    // --- 纯字符串日志（宏自动捕获源位置） ---
    SY_TRACE("--- SyLog Usage Example ---");
    SY_DEBUG("This is a debug message");
    SY_INFO("This is an info message");
    SY_WARN("This is a warning");
    SY_ERROR("This is an error");
    SY_CRITICAL("This is critical");

    // --- printf 风格格式化日志（推荐） ---
    SY_INFOF("User '%s' logged in from %s", "admin", "192.168.1.1");
    SY_INFOF("Processed %d items in %.2f seconds", 1024, 3.14);
    SY_WARNF("Disk usage at %.1f%%, threshold is %.0f%%", 85.3, 80.0);
    SY_ERRORF("Failed to open file: %s (errno=%d)", "/path/to/data.bin", 2);

    // --- 使用直接 API（不捕获源位置） ---
    SyLogger::GetInstance().InfoStr("Direct API call (no source location)");
    SyLogger::GetInstance().InfoF("Direct printf: %s = %d", "count", 42);


    // ==================== 3. 运行时控制 ====================

    // 动态调整级别
    SyLogger::GetInstance().SetLevel(SyLogLevel::Warn);
    SY_INFO("This INFO will NOT appear (level now Warn)");      // 被过滤
    SY_WARN("This WARN will appear");                            // 通过

    // 动态启用/禁用
    SyLogger::GetInstance().SetEnabled(false);
    SY_ERROR("This ERROR will NOT appear (logging disabled)");   // 被过滤
    SyLogger::GetInstance().SetEnabled(true);
    SY_INFO("Logging re-enabled");

    // 恢复调试级别
    SyLogger::GetInstance().SetLevel(SyLogLevel::Trace);


    // ==================== 4. 多线程安全 ====================

    std::thread t1([] { SY_INFO("Thread 1 is working"); });
    std::thread t2([] { SY_INFO("Thread 2 is working"); });
    t1.join();
    t2.join();


    // ==================== 5. 查看日志目录 ====================

    std::string logDir = SyLogger::GetInstance().GetLogDirectory();
    SY_INFOF("Logs are stored at: %s", logDir.c_str());


    // ==================== 6. 关闭日志 ====================

    SyLogger::GetInstance().Shutdown();
    return 0;
}
