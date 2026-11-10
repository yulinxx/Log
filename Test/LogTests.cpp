#include <gtest/gtest.h>
#include "LogAPI.h"
#include "Log/AprLogger.h"
#include "Log/AprTraceContext.h"

#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

#ifdef _WIN32
    // 本文件用 GetTempPathA / MAX_PATH，必须包含 Windows 头。
    // 但要注意：Windows 头里有 `#define small char` 这类老式兼容宏，
    // Unity Build 会把本目录的其它测试文件拼进同一个翻译单元，宏会串味 ——
    // 变量名 small 会被展开成 char，编译报「'char' 后面的 'char' 非法」。
    // 同一 unity 块里的其它文件请避开 small / near / far 这些名字。
    #include <Windows.h>
#endif

// ==================== 日志级别测试 ====================

TEST(AprLogLevelTest, LevelComparison)
{
    EXPECT_LT(static_cast<int>(AprLogLevel::Trace), static_cast<int>(AprLogLevel::Debug));
    EXPECT_LT(static_cast<int>(AprLogLevel::Debug), static_cast<int>(AprLogLevel::Info));
    EXPECT_LT(static_cast<int>(AprLogLevel::Info), static_cast<int>(AprLogLevel::Warn));
    EXPECT_LT(static_cast<int>(AprLogLevel::Warn), static_cast<int>(AprLogLevel::Error));
    EXPECT_LT(static_cast<int>(AprLogLevel::Error), static_cast<int>(AprLogLevel::Critical));
    EXPECT_LT(static_cast<int>(AprLogLevel::Critical), static_cast<int>(AprLogLevel::Off));
}

TEST(AprLogLevelTest, LevelValues)
{
    EXPECT_EQ(static_cast<int>(AprLogLevel::Trace), 0);
    EXPECT_EQ(static_cast<int>(AprLogLevel::Debug), 1);
    EXPECT_EQ(static_cast<int>(AprLogLevel::Info), 2);
    EXPECT_EQ(static_cast<int>(AprLogLevel::Warn), 3);
    EXPECT_EQ(static_cast<int>(AprLogLevel::Error), 4);
    EXPECT_EQ(static_cast<int>(AprLogLevel::Critical), 5);
    EXPECT_EQ(static_cast<int>(AprLogLevel::Off), 6);
}

// ==================== 日志配置测试 ====================

TEST(AprLogConfigTest, DefaultConfiguration)
{
    AprLogConfig config;

    EXPECT_STREQ(config.logName, "April");
    EXPECT_STREQ(config.logPath, "");
    EXPECT_EQ(config.level, AprLogLevel::Debug);
    EXPECT_TRUE(config.consoleEnable);
    EXPECT_TRUE(config.fileEnable);
    EXPECT_EQ(config.maxAgeDays, 30);
    EXPECT_EQ(config.maxFileSize, 10 * 1024 * 1024);
    EXPECT_EQ(config.maxFiles, 10);
    EXPECT_EQ(config.rateLimit, 0);
    EXPECT_EQ(config.asyncQueueSize, 8192);
    EXPECT_EQ(config.asyncThreads, 1);
}

TEST(AprLogConfigTest, CustomConfiguration)
{
    AprLogConfig config;
    config.logName = "TestApp";
    config.logPath = "";
    config.level = AprLogLevel::Info;
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
    EXPECT_EQ(config.level, AprLogLevel::Info);
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

TEST(AprLoggerTest, SingletonPattern)
{
    AprLogger& logger1 = AprLogger::GetInstance();
    AprLogger& logger2 = AprLogger::GetInstance();

    // 确保是同一个实例
    EXPECT_EQ(&logger1, &logger2);
}

TEST(AprLoggerTest, Initialization)
{
    AprLogger& logger = AprLogger::GetInstance();

    // 测试默认初始化
    logger.Initialize();

    // 测试自定义初始化
    logger.Initialize("TestLogger", AprLogLevel::Info, false, true);

    // 测试配置结构初始化
    AprLogConfig config;
    config.logName = "ConfigLogger";
    config.level = AprLogLevel::Warn;
    config.consoleEnable = true;
    config.fileEnable = false;

    logger.Initialize(config);
}

TEST(AprLoggerTest, Shutdown)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("ShutdownTest");
    logger.Shutdown();

    // 关闭后应该可以重新初始化
    logger.Initialize("ReinitTest");
}

// ==================== 日志级别控制测试 ====================

TEST(AprLoggerTest, LevelControl)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("LevelTest", AprLogLevel::Info);

    // 测试设置和获取级别
    logger.SetLevel(AprLogLevel::Warn);
    EXPECT_EQ(logger.GetLevel(), AprLogLevel::Warn);

    logger.SetLevel(AprLogLevel::Error);
    EXPECT_EQ(logger.GetLevel(), AprLogLevel::Error);

    logger.SetLevel(AprLogLevel::Trace);
    EXPECT_EQ(logger.GetLevel(), AprLogLevel::Trace);
}

TEST(AprLoggerTest, EnableDisable)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("EnableTest");

    // 测试启用/禁用
    logger.SetEnabled(false);
    EXPECT_FALSE(logger.IsEnabled());

    logger.SetEnabled(true);
    EXPECT_TRUE(logger.IsEnabled());
}

// ==================== 字符串日志测试 ====================

TEST(AprLoggerTest, StringLogging)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("StringTest", AprLogLevel::Trace);

    // 测试所有级别的字符串日志
    logger.TraceStr("This is a trace message");
    logger.DebugStr("This is a debug message");
    logger.InfoStr("This is an info message");
    logger.WarnStr("This is a warning message");
    logger.ErrorStr("This is an error message");
    logger.CriticalStr("This is a critical message");

    // 测试不同级别的过滤
    logger.SetLevel(AprLogLevel::Warn);

    // 这些消息应该被过滤掉
    logger.TraceStr("This trace should be filtered");
    logger.DebugStr("This debug should be filtered");
    logger.InfoStr("This info should be filtered");

    // 这些消息应该被记录
    logger.WarnStr("This warning should be logged");
    logger.ErrorStr("This error should be logged");
    logger.CriticalStr("This critical should be logged");
}

TEST(AprLoggerTest, StringLoggingDisabled)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("DisabledTest");
    logger.SetEnabled(false);

    // 禁用状态下不应该记录日志
    logger.InfoStr("This message should not be logged when disabled");
    logger.ErrorStr("This error should not be logged when disabled");

    logger.SetEnabled(true);
    logger.InfoStr("This message should be logged when enabled");
}

// ==================== 格式化日志测试 ====================

TEST(AprLoggerTest, FormattedLogging)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("FormatTest", AprLogLevel::Debug);

    // 测试格式化日志
    logger.DebugF("Formatted debug: %s %d", "test", 123);
    logger.InfoF("Formatted info: %.2f", 3.14159);
    logger.WarnF("Formatted warning: %c", 'A');
    logger.ErrorF("Formatted error: %s", "error message");

    // 测试复杂格式化
    const char* name = "April";
    int version = 1;
    double dValue = 99.99;

    logger.InfoF("Application %s v%d started with value %.2f", name, version, dValue);
}

TEST(AprLoggerTest, FormattedLoggingEdgeCases)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("FormatEdgeTest");

    // 测试边界情况
    logger.InfoF("Empty format string");
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat"
    #pragma GCC diagnostic ignored "-Wformat-security"
    #pragma GCC diagnostic ignored "-Wformat-zero-length"
    // 故意喂空串，验证 logger 不崩溃（编译期已知为边界用例，规避 format 字面量检查）
    logger.InfoF("");
    #pragma GCC diagnostic pop
#else
    logger.InfoF("");
#endif
    logger.InfoF(
        "Very long format string with many parameters: %d %d %d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

    // 测试特殊字符
    logger.InfoF("Special chars: \t\n\r\\");
}

// ==================== 日志文件管理测试 ====================

TEST(AprLoggerTest, LogDirectory)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("DirectoryTest");

    const char* logDir = logger.GetLogDirectory();

    // 日志目录应该不为空
    EXPECT_TRUE(logDir != nullptr);
    EXPECT_STRNE(logDir, "");

    // logName 只作为日志文件名（<logName>.log），不进目录路径；
    // 目录是 main 里 SetDefaultLogPath 设的默认目录。
    EXPECT_TRUE(std::string(logDir).find("AprLogTests") != std::string::npos);
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::u8path(logDir) / "DirectoryTest.log"));
}

TEST(AprLoggerTest, CustomLogPath)
{
    AprLogger& logger = AprLogger::GetInstance();

    // 使用用户临时目录，避免权限问题
    // 非 Windows 无 MAX_PATH（本测试此前从未在非 Win 平台编译过）；
    // GetTempPathA 仅在 Win 下填充，其余平台 tempPath 保持空串。
    char tempPath[260] = { 0 };
#ifdef _WIN32
    GetTempPathA(MAX_PATH, tempPath);
#endif
    std::string customPath = std::string(tempPath) + "AprLogs";

    AprLogConfig config;
    config.logName = "CustomPathTest";
    config.logPath = customPath.c_str();

    logger.Initialize(config);

    const char* logDir = logger.GetLogDirectory();

    // 应该使用自定义路径
    EXPECT_TRUE(logDir != nullptr);
    EXPECT_TRUE(std::string(logDir).find("AprLogs") != std::string::npos);
}

TEST(AprLoggerTest, CleanOldLogs)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("CleanTest");

    // 清理过期日志（应该不会抛出异常）
    EXPECT_NO_THROW(logger.CleanOldLogs());
}

// ==================== 并发日志测试 ====================

TEST(AprLoggerTest, ConcurrentLogging)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("ConcurrentTest", AprLogLevel::Info);

    const int NUM_THREADS = 4;
    const int MESSAGES_PER_THREAD = 10;

    auto logWorker = [&logger, MESSAGES_PER_THREAD](int threadId) {
        for (int i = 0; i < MESSAGES_PER_THREAD; ++i)
        {
            logger.InfoF("Thread %d: Message %d", threadId, i);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    std::vector<std::thread> threads;

    // 启动多个线程同时记录日志
    for (int i = 0; i < NUM_THREADS; ++i)
    {
        threads.emplace_back(logWorker, i);
    }

    // 等待所有线程完成
    for (auto& thread : threads)
    {
        thread.join();
    }

    // 测试应该完成而不崩溃
    EXPECT_TRUE(true);
}

// ==================== 性能测试 ====================

TEST(AprLoggerTest, Performance)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("PerformanceTest", AprLogLevel::Off);  // 关闭日志以提高性能

    const int NUM_ITERATIONS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; ++i)
    {
        logger.InfoF("Performance test message %d", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 记录日志操作应该快速完成
    EXPECT_LT(duration.count(), 1000);  // 应该在1秒内完成

    std::cout << "Performance test completed in " << duration.count() << " ms" << std::endl;
}

// ==================== 错误处理测试 ====================

TEST(AprLoggerTest, ErrorHandling)
{
    AprLogger& logger = AprLogger::GetInstance();

    // 测试无效的日志级别设置
    EXPECT_NO_THROW(logger.SetLevel(static_cast<AprLogLevel>(100)));

    // 测试空字符串日志
    EXPECT_NO_THROW(logger.InfoStr(""));

    // 测试非常长的消息
    std::string longMessage(10000, 'X');
    EXPECT_NO_THROW(logger.InfoStr(longMessage.c_str()));

    // 测试格式化字符串中的无效格式
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat"
    // 故意喂带孤立 % 的非法格式串，验证 logger 不抛异常
    EXPECT_NO_THROW(logger.InfoF("Invalid format: %"));
    #pragma GCC diagnostic pop
#else
    EXPECT_NO_THROW(logger.InfoF("Invalid format: %"));
#endif
}

// ==================== 便捷宏测试 ====================

TEST(AprLoggerMacroTest, StringMacros)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("MacroTest", AprLogLevel::Trace);

    // 测试字符串宏
    APR_TRACE("Trace macro message");
    APR_DEBUG("Debug macro message");
    APR_INFO("Info macro message");
    APR_WARN("Warning macro message");
    APR_ERROR("Error macro message");
    APR_CRITICAL("Critical macro message");
}

// ==================== 源位置日志测试 ====================

TEST(AprLoggerTest, SourceLocationLogging)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("SrcLocTest", AprLogLevel::Trace);

    // 使用 LogSrc / LogFSrc
    logger.LogSrc(AprLogLevel::Info, __FILE__, __LINE__, "LogSrc message");
    logger.LogFSrc(AprLogLevel::Warn, __FILE__, __LINE__, "LogFSrc %s %d", "test", 42);

    // 使用宏（自动捕获源位置）
    APR_TRACE("Source location via macro");
    APR_INFO("Source location via macro");
    APR_WARN("Source location via macro");

    APR_TRACEF("Formatted %s via macro", "trace");
    APR_INFOF("Formatted %s via macro", "info");
    APR_ERRORF("Formatted %s=%d", "code", 500);
}

TEST(AprLoggerTest, LogSrcLevelFiltering)
{
    AprLogger& logger = AprLogger::GetInstance();

    logger.Initialize("SrcFilterTest", AprLogLevel::Warn);

    // 这些应该被过滤掉（级别低于 Warn）
    logger.LogSrc(AprLogLevel::Trace, __FILE__, __LINE__, "Should be filtered");
    logger.LogSrc(AprLogLevel::Debug, __FILE__, __LINE__, "Should be filtered");
    logger.LogSrc(AprLogLevel::Info, __FILE__, __LINE__, "Should be filtered");

    // 这些应该被记录
    logger.LogSrc(AprLogLevel::Warn, __FILE__, __LINE__, "Should be logged");
    logger.LogSrc(AprLogLevel::Error, __FILE__, __LINE__, "Should be logged");
    logger.LogSrc(AprLogLevel::Critical, __FILE__, __LINE__, "Should be logged");
}

// ==================== 速率限制测试 ====================

TEST(AprLoggerTest, RateLimiting)
{
    AprLogConfig config;
    config.logName = "RateLimitTest";
    config.level = AprLogLevel::Trace;
    config.rateLimit = 5;

    AprLogger& logger = AprLogger::GetInstance();
    logger.Initialize(config);

    // 连续写入 20 条，应该只有 5 条通过
    for (int i = 0; i < 20; ++i)
    {
        std::string msg = "Rate limited message " + std::to_string(i);
        logger.TraceStr(msg.c_str());
    }
}

// ==================== 内存管理测试 ====================

TEST(AprLoggerTest, MemoryManagement)
{
    // 测试多次初始化和关闭
    for (int i = 0; i < 10; ++i)
    {
        AprLogger& logger = AprLogger::GetInstance();

        std::string logName = "MemoryTest" + std::to_string(i);
        logger.Initialize(logName.c_str());

        // 记录一些日志
        for (int j = 0; j < 10; ++j)
        {
            logger.InfoF("Memory test iteration %d-%d", i, j);
        }

        logger.Shutdown();
    }

    // 如果没有内存泄漏，测试应该通过
    EXPECT_TRUE(true);
}

// ==================== 集成测试 ====================

TEST(AprLoggerIntegrationTest, CompleteWorkflow)
{
    // 完整的日志系统工作流测试
    AprLogger& logger = AprLogger::GetInstance();

    // 1. 初始化
    AprLogConfig config;
    config.logName = "IntegrationTest";
    config.level = AprLogLevel::Info;
    config.consoleEnable = true;
    config.fileEnable = true;

    logger.Initialize(config);

    // 2. 记录各种级别的日志
    logger.TraceStr("This trace should be filtered");
    logger.InfoStr("Application started");
    logger.WarnStr("Low disk space");
    logger.ErrorStr("File not found");

    // 3. 更改配置
    logger.SetLevel(AprLogLevel::Warn);
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

// ==================== TraceContext ABI 测试 ====================

TEST(AprTraceContextTest, EmptyStack)
{
    AprTrace::popTraceId();
    char buffer[128];
    const size_t len = AprTrace::currentTraceId(buffer, sizeof(buffer));
    EXPECT_EQ(len, 0u);
    EXPECT_STREQ(buffer, "");
    EXPECT_TRUE(AprTrace::currentTraceIdString().empty());
}

TEST(AprTraceContextTest, PushPop)
{
    AprTrace::pushTraceId("trace-1");
    EXPECT_EQ(AprTrace::currentTraceIdString(), "trace-1");
    AprTrace::pushTraceId("trace-2");
    EXPECT_EQ(AprTrace::currentTraceIdString(), "trace-2");
    AprTrace::popTraceId();
    EXPECT_EQ(AprTrace::currentTraceIdString(), "trace-1");
    AprTrace::popTraceId();
    EXPECT_TRUE(AprTrace::currentTraceIdString().empty());
}

TEST(AprTraceContextTest, ResolveTraceId)
{
    char buffer[128];
    // explicitId 优先
    const size_t len = AprTrace::resolveTraceId("explicit-id", buffer, sizeof(buffer));
    EXPECT_EQ(len, 11u);
    EXPECT_STREQ(buffer, "explicit-id");
    // null/空串回退
    EXPECT_STREQ(AprTrace::resolveTraceIdString(nullptr).c_str(), "");
    AprTrace::pushTraceId("stack-id");
    EXPECT_EQ(AprTrace::resolveTraceIdString(nullptr), "stack-id");
    EXPECT_EQ(AprTrace::resolveTraceIdString(""), "stack-id");
    AprTrace::popTraceId();
}

TEST(AprTraceContextTest, PushIgnoresEmpty)
{
    AprTrace::pushTraceId("");
    AprTrace::pushTraceId(nullptr);
    EXPECT_TRUE(AprTrace::currentTraceIdString().empty());
}

// ==================== 主测试入口 ====================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    // 库设计：logPath 为空时 GetDefaultLogPath 兜底返回空串，
    // 调用方必须先 SetDefaultLogPath/SetLogPathCallback，否则
    // create_directories("") 抛 filesystem_error。测试统一落到临时目录。
    namespace fs = std::filesystem;
    static const fs::path defaultLogDir = fs::temp_directory_path() / "AprLogTests";
    std::error_code ec;
    fs::create_directories(defaultLogDir, ec);
    AprLogger::SetDefaultLogPath(defaultLogDir.string().c_str());

    return RUN_ALL_TESTS();
}