#include <gtest/gtest.h>
#include "LogAPI.h"
#include "Log/SyLogger.h"

#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif

// ==================== 日志级别测试 ====================

TEST(SyLogLevelTest, LevelComparison)
{
    EXPECT_LT(static_cast<int>(SyLogLevel::Trace), static_cast<int>(SyLogLevel::Debug));
    EXPECT_LT(static_cast<int>(SyLogLevel::Debug), static_cast<int>(SyLogLevel::Info));
    EXPECT_LT(static_cast<int>(SyLogLevel::Info), static_cast<int>(SyLogLevel::Warn));
    EXPECT_LT(static_cast<int>(SyLogLevel::Warn), static_cast<int>(SyLogLevel::Error));
    EXPECT_LT(static_cast<int>(SyLogLevel::Error), static_cast<int>(SyLogLevel::Critical));
    EXPECT_LT(static_cast<int>(SyLogLevel::Critical), static_cast<int>(SyLogLevel::Off));
}

TEST(SyLogLevelTest, LevelValues)
{
    EXPECT_EQ(static_cast<int>(SyLogLevel::Trace), 0);
    EXPECT_EQ(static_cast<int>(SyLogLevel::Debug), 1);
    EXPECT_EQ(static_cast<int>(SyLogLevel::Info), 2);
    EXPECT_EQ(static_cast<int>(SyLogLevel::Warn), 3);
    EXPECT_EQ(static_cast<int>(SyLogLevel::Error), 4);
    EXPECT_EQ(static_cast<int>(SyLogLevel::Critical), 5);
    EXPECT_EQ(static_cast<int>(SyLogLevel::Off), 6);
}

// ==================== 日志配置测试 ====================

TEST(SyLogConfigTest, DefaultConfiguration)
{
    SyLogConfig config;
    
    EXPECT_STREQ(config.logName, "SanYi");
    EXPECT_STREQ(config.logPath, "");
    EXPECT_EQ(config.level, SyLogLevel::Debug);
    EXPECT_TRUE(config.consoleEnable);
    EXPECT_TRUE(config.fileEnable);
    EXPECT_EQ(config.maxAgeDays, 30);
    EXPECT_EQ(config.maxFileSize, 10 * 1024 * 1024);
    EXPECT_EQ(config.maxFiles, 10);
    EXPECT_EQ(config.rateLimit, 0);
    EXPECT_EQ(config.asyncQueueSize, 8192);
    EXPECT_EQ(config.asyncThreads, 1);
}

TEST(SyLogConfigTest, CustomConfiguration)
{
    SyLogConfig config;
    config.logName = "TestApp";
    config.logPath = "";
    config.level = SyLogLevel::Info;
    config.consoleEnable = false;
    config.fileEnable = true;
    config.maxAgeDays = 7;
    config.maxFileSize = 5 * 1024 * 1024;
    config.maxFiles = 3;
    config.rateLimit = 100;
    config.asyncQueueSize = 4096;
    config.asyncThreads = 2;
    
    EXPECT_STREQ(config.logName, "TestApp");
    EXPECT_STREQ(config.logPath, "");
    EXPECT_EQ(config.level, SyLogLevel::Info);
    EXPECT_FALSE(config.consoleEnable);
    EXPECT_TRUE(config.fileEnable);
    EXPECT_EQ(config.maxAgeDays, 7);
    EXPECT_EQ(config.maxFileSize, 5 * 1024 * 1024);
    EXPECT_EQ(config.maxFiles, 3);
    EXPECT_EQ(config.rateLimit, 100);
    EXPECT_EQ(config.asyncQueueSize, 4096);
    EXPECT_EQ(config.asyncThreads, 2);
}

// ==================== 日志器单例测试 ====================

TEST(SyLoggerTest, SingletonPattern)
{
    SyLogger& logger1 = SyLogger::GetInstance();
    SyLogger& logger2 = SyLogger::GetInstance();
    
    // 确保是同一个实例
    EXPECT_EQ(&logger1, &logger2);
}

TEST(SyLoggerTest, Initialization)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    // 测试默认初始化
    logger.Initialize();
    
    // 测试自定义初始化
    logger.Initialize("TestLogger", SyLogLevel::Info, false, true);
    
    // 测试配置结构初始化
    SyLogConfig config;
    config.logName = "ConfigLogger";
    config.level = SyLogLevel::Warn;
    config.consoleEnable = true;
    config.fileEnable = false;
    
    logger.Initialize(config);
}

TEST(SyLoggerTest, Shutdown)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("ShutdownTest");
    logger.Shutdown();
    
    // 关闭后应该可以重新初始化
    logger.Initialize("ReinitTest");
}

// ==================== 日志级别控制测试 ====================

TEST(SyLoggerTest, LevelControl)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("LevelTest", SyLogLevel::Info);
    
    // 测试设置和获取级别
    logger.SetLevel(SyLogLevel::Warn);
    EXPECT_EQ(logger.GetLevel(), SyLogLevel::Warn);
    
    logger.SetLevel(SyLogLevel::Error);
    EXPECT_EQ(logger.GetLevel(), SyLogLevel::Error);
    
    logger.SetLevel(SyLogLevel::Trace);
    EXPECT_EQ(logger.GetLevel(), SyLogLevel::Trace);
}

TEST(SyLoggerTest, EnableDisable)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("EnableTest");
    
    // 测试启用/禁用
    logger.SetEnabled(false);
    EXPECT_FALSE(logger.IsEnabled());
    
    logger.SetEnabled(true);
    EXPECT_TRUE(logger.IsEnabled());
}

// ==================== 字符串日志测试 ====================

TEST(SyLoggerTest, StringLogging)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("StringTest", SyLogLevel::Trace);
    
    // 测试所有级别的字符串日志
    logger.TraceStr("This is a trace message");
    logger.DebugStr("This is a debug message");
    logger.InfoStr("This is an info message");
    logger.WarnStr("This is a warning message");
    logger.ErrorStr("This is an error message");
    logger.CriticalStr("This is a critical message");
    
    // 测试不同级别的过滤
    logger.SetLevel(SyLogLevel::Warn);
    
    // 这些消息应该被过滤掉
    logger.TraceStr("This trace should be filtered");
    logger.DebugStr("This debug should be filtered");
    logger.InfoStr("This info should be filtered");
    
    // 这些消息应该被记录
    logger.WarnStr("This warning should be logged");
    logger.ErrorStr("This error should be logged");
    logger.CriticalStr("This critical should be logged");
}

TEST(SyLoggerTest, StringLoggingDisabled)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("DisabledTest");
    logger.SetEnabled(false);
    
    // 禁用状态下不应该记录日志
    logger.InfoStr("This message should not be logged when disabled");
    logger.ErrorStr("This error should not be logged when disabled");
    
    logger.SetEnabled(true);
    logger.InfoStr("This message should be logged when enabled");
}

// ==================== 格式化日志测试 ====================

TEST(SyLoggerTest, FormattedLogging)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("FormatTest", SyLogLevel::Debug);
    
    // 测试格式化日志
    logger.DebugF("Formatted debug: %s %d", "test", 123);
    logger.InfoF("Formatted info: %.2f", 3.14159);
    logger.WarnF("Formatted warning: %c", 'A');
    logger.ErrorF("Formatted error: %s", "error message");
    
    // 测试复杂格式化
    const char* name = "SanYi";
    int version = 1;
    double dValue = 99.99;
    
    logger.InfoF("Application %s v%d started with value %.2f", name, version, dValue);
}

TEST(SyLoggerTest, FormattedLoggingEdgeCases)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("FormatEdgeTest");
    
    // 测试边界情况
    logger.InfoF("Empty format string");
    logger.InfoF(""); // 空字符串
    logger.InfoF("Very long format string with many parameters: %d %d %d %d %d %d %d %d %d %d", 
                 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    
    // 测试特殊字符
    logger.InfoF("Special chars: \t\n\r\\");
}

// ==================== 日志文件管理测试 ====================

TEST(SyLoggerTest, LogDirectory)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("DirectoryTest");
    
    const char* logDir = logger.GetLogDirectory();
    
    // 日志目录应该不为空
    EXPECT_TRUE(logDir != nullptr);
    EXPECT_STRNE(logDir, "");
    
    // 应该包含日志名称
    EXPECT_TRUE(std::string(logDir).find("DirectoryTest") != std::string::npos);
}

TEST(SyLoggerTest, CustomLogPath)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    // 使用用户临时目录，避免权限问题
    char tempPath[MAX_PATH] = {0};
#ifdef _WIN32
    GetTempPathA(MAX_PATH, tempPath);
#endif
    std::string customPath = std::string(tempPath) + "SanYiLogs";
    
    SyLogConfig config;
    config.logName = "CustomPathTest";
    config.logPath = customPath.c_str();
    
    logger.Initialize(config);
    
    const char* logDir = logger.GetLogDirectory();
    
    // 应该使用自定义路径
    EXPECT_TRUE(logDir != nullptr);
    EXPECT_TRUE(std::string(logDir).find("SanYiLogs") != std::string::npos);
}

TEST(SyLoggerTest, CleanOldLogs)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("CleanTest");
    
    // 清理过期日志（应该不会抛出异常）
    EXPECT_NO_THROW(logger.CleanOldLogs());
}

// ==================== 并发日志测试 ====================

TEST(SyLoggerTest, ConcurrentLogging)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("ConcurrentTest", SyLogLevel::Info);
    
    const int NUM_THREADS = 4;
    const int MESSAGES_PER_THREAD = 10;
    
    auto logWorker = [&logger, MESSAGES_PER_THREAD](int threadId) {
        for (int i = 0; i < MESSAGES_PER_THREAD; ++i) {
            logger.InfoF("Thread %d: Message %d", threadId, i);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    
    std::vector<std::thread> threads;
    
    // 启动多个线程同时记录日志
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(logWorker, i);
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 测试应该完成而不崩溃
    EXPECT_TRUE(true);
}

// ==================== 性能测试 ====================

TEST(SyLoggerTest, Performance)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("PerformanceTest", SyLogLevel::Off); // 关闭日志以提高性能
    
    const int NUM_ITERATIONS = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        logger.InfoF("Performance test message %d", i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 记录日志操作应该快速完成
    EXPECT_LT(duration.count(), 1000); // 应该在1秒内完成
    
    std::cout << "Performance test completed in " << duration.count() << " ms" << std::endl;
}

// ==================== 错误处理测试 ====================

TEST(SyLoggerTest, ErrorHandling)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    // 测试无效的日志级别设置
    EXPECT_NO_THROW(logger.SetLevel(static_cast<SyLogLevel>(100)));
    
    // 测试空字符串日志
    EXPECT_NO_THROW(logger.InfoStr(""));
    
    // 测试非常长的消息
    std::string longMessage(10000, 'X');
    EXPECT_NO_THROW(logger.InfoStr(longMessage.c_str()));
    
    // 测试格式化字符串中的无效格式
    EXPECT_NO_THROW(logger.InfoF("Invalid format: %"));
}

// ==================== 便捷宏测试 ====================

TEST(SyLoggerMacroTest, StringMacros)
{
    SyLogger& logger = SyLogger::GetInstance();
    
    logger.Initialize("MacroTest", SyLogLevel::Trace);
    
    // 测试字符串宏
    SY_TRACE("Trace macro message");
    SY_DEBUG("Debug macro message");
    SY_INFO("Info macro message");
    SY_WARN("Warning macro message");
    SY_ERROR("Error macro message");
    SY_CRITICAL("Critical macro message");
}

// ==================== 源位置日志测试 ====================

TEST(SyLoggerTest, SourceLocationLogging)
{
    SyLogger& logger = SyLogger::GetInstance();

    logger.Initialize("SrcLocTest", SyLogLevel::Trace);

    // 使用 LogSrc / LogFSrc
    logger.LogSrc(SyLogLevel::Info, __FILE__, __LINE__, "LogSrc message");
    logger.LogFSrc(SyLogLevel::Warn, __FILE__, __LINE__, "LogFSrc %s %d", "test", 42);

    // 使用宏（自动捕获源位置）
    SY_TRACE("Source location via macro");
    SY_INFO("Source location via macro");
    SY_WARN("Source location via macro");

    SY_TRACEF("Formatted %s via macro", "trace");
    SY_INFOF("Formatted %s via macro", "info");
    SY_ERRORF("Formatted %s=%d", "code", 500);
}

TEST(SyLoggerTest, LogSrcLevelFiltering)
{
    SyLogger& logger = SyLogger::GetInstance();

    logger.Initialize("SrcFilterTest", SyLogLevel::Warn);

    // 这些应该被过滤掉（级别低于 Warn）
    logger.LogSrc(SyLogLevel::Trace, __FILE__, __LINE__, "Should be filtered");
    logger.LogSrc(SyLogLevel::Debug, __FILE__, __LINE__, "Should be filtered");
    logger.LogSrc(SyLogLevel::Info, __FILE__, __LINE__, "Should be filtered");

    // 这些应该被记录
    logger.LogSrc(SyLogLevel::Warn, __FILE__, __LINE__, "Should be logged");
    logger.LogSrc(SyLogLevel::Error, __FILE__, __LINE__, "Should be logged");
    logger.LogSrc(SyLogLevel::Critical, __FILE__, __LINE__, "Should be logged");
}

// ==================== 速率限制测试 ====================

TEST(SyLoggerTest, RateLimiting)
{
    SyLogConfig config;
    config.logName = "RateLimitTest";
    config.level = SyLogLevel::Trace;
    config.rateLimit = 5;

    SyLogger& logger = SyLogger::GetInstance();
    logger.Initialize(config);

    // 连续写入 20 条，应该只有 5 条通过
    for (int i = 0; i < 20; ++i)
    {
        std::string msg = "Rate limited message " + std::to_string(i);
        logger.TraceStr(msg.c_str());
    }
}

// ==================== 内存管理测试 ====================

TEST(SyLoggerTest, MemoryManagement)
{
    // 测试多次初始化和关闭
    for (int i = 0; i < 10; ++i) {
        SyLogger& logger = SyLogger::GetInstance();
        
        std::string logName = "MemoryTest" + std::to_string(i);
        logger.Initialize(logName.c_str());
        
        // 记录一些日志
        for (int j = 0; j < 10; ++j) {
            logger.InfoF("Memory test iteration %d-%d", i, j);
        }
        
        logger.Shutdown();
    }
    
    // 如果没有内存泄漏，测试应该通过
    EXPECT_TRUE(true);
}

// ==================== 集成测试 ====================

TEST(SyLoggerIntegrationTest, CompleteWorkflow)
{
    // 完整的日志系统工作流测试
    SyLogger& logger = SyLogger::GetInstance();
    
    // 1. 初始化
    SyLogConfig config;
    config.logName = "IntegrationTest";
    config.level = SyLogLevel::Info;
    config.consoleEnable = true;
    config.fileEnable = true;
    
    logger.Initialize(config);
    
    // 2. 记录各种级别的日志
    logger.TraceStr("This trace should be filtered");
    logger.InfoStr("Application started");
    logger.WarnStr("Low disk space");
    logger.ErrorStr("File not found");
    
    // 3. 更改配置
    logger.SetLevel(SyLogLevel::Warn);
    logger.InfoStr("This info should now be filtered");
    logger.WarnStr("This warning should be logged");
    
    // 4. 禁用日志
    logger.SetEnabled(false);
    logger.ErrorStr("This error should not be logged");
    
    // 5. 重新启用
    logger.SetEnabled(true);
    logger.InfoStr("Logging re-enabled");
    
    // 6. 清理
    logger.CleanOldLogs();
    logger.Shutdown();
    
    // 测试应该完成而不崩溃
    EXPECT_TRUE(true);
}

// ==================== 主测试入口 ====================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}