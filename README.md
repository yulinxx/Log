# SyLogger 日志库

基于 **spdlog** 的高性能 C++ 日志库，提供简洁易用的日志接口。

## 核心特性

- **printf 风格格式化**：类型安全的参数格式化（`%s`、`%d`、`%.2f` 等）
- **双通道输出**：控制台彩色输出 + 文件持久化
- **异步日志**：日志写盘在后台线程处理，不阻塞业务线程
- **大小 + 数量轮转**：按文件大小轮转，保留固定数量的历史文件，防磁盘写爆
- **多级别输出**：Trace / Debug / Info / Warn / Error / Critical
- **运行时动态调整**：级别、启用/禁用均可运行时切换
- **源位置自动捕获**：宏自动记录 `__FILE__`、`__LINE__`，便于定位问题
- **日志分流**：可单独输出 Error 日志（Warn+）和 Debug 日志（Trace/Debug）
- **速率限制**：可配置每秒最大日志条数，防止日志洪泛
- **自动清理**：超期日志文件自动删除（默认30天）
- **线程安全**：多线程环境下安全使用
- **智能路径管理**：自动使用用户目录，避免权限问题
- **C 语言 API**：提供 `extern "C"` 接口，方便跨语言调用

## 快速开始

```cpp
#include "Log/SyLogger.h"

int main() {
    // 初始化日志系统（使用默认配置）
    SyLogger::GetInstance().Initialize("MyApp");

    // 基础日志输出（自动捕获源位置）
    SY_INFO("程序启动");
    SY_WARN("这是警告");
    SY_ERROR("发生错误");

    // printf 风格格式化日志
    const char* username = "张三";
    int count = 42;
    double elapsed = 1.23;
    
    SY_INFOF("用户 %s 登录成功", username);
    SY_INFOF("处理 %d 条数据，耗时 %.2f 秒", count, elapsed);
    SY_ERRORF("错误码: %d", 500);

    // 程序退出前关闭（可选，进程退出时会自动清理）
    SyLogger::GetInstance().Shutdown();
    return 0;
}
```

## 日志存储路径

### 默认路径

| 系统 | 默认路径 | 说明 |
|------|----------|------|
| Windows | `C:\Users\<用户名>\AppData\Local\<AppName>\logs\` | 用户本地应用数据目录，无需管理员权限 |
| Linux | `~/.local/share/<AppName>/logs/` | 用户本地共享目录 |
| macOS | `~/.local/share/<AppName>/logs/` | 用户本地共享目录 |

### 如何查找日志文件

**Windows 系统**：
1. 打开文件资源管理器
2. 在地址栏输入 `%LOCALAPPDATA%\<AppName>\logs\`
3. 例如：`%LOCALAPPDATA%\MyApp\logs\`

**Linux / macOS**：
```bash
cd ~/.local/share/<AppName>/logs/
ls -la
```

### 自定义路径

可以通过 `SyLogConfig::logPath` 设置自定义路径，但**不建议使用系统根目录**（如 `C:\`），因为可能没有写入权限。推荐使用：

| 场景 | 推荐路径 |
|------|----------|
| 开发调试 | 用户临时目录（`%TEMP%`） |
| 生产环境 | 默认路径或应用安装目录下的 logs 子目录 |
| 服务器部署 | `/var/log/<AppName>/`（Linux） |

## 配置选项

### 完整配置

```cpp
#include "Log/SyLogger.h"

SyLogConfig config;
config.logName = "AppName";              // 日志名称，用于目录和文件名
config.logPath = "";                     // 留空使用默认路径，建议保持为空
config.level = SyLogLevel::Debug;        // 日志级别：Trace/Debug/Info/Warn/Error/Critical/Off
config.consoleEnable = true;             // 启用控制台彩色输出
config.fileEnable = true;                // 启用文件输出
config.maxAgeDays = 30;                  // 日志保留天数，超过自动删除
config.splitErrorLog = true;             // 额外写入 .error.log（Warn 及以上级别）
config.splitDebugLog = false;            // 额外写入 .debug.log（Trace/Debug 级别）
config.maxFileSize = 10 * 1024 * 1024;   // 单文件大小上限（默认 10MB）
config.maxFiles = 10;                    // 保留的历史文件数（轮转后）
config.rateLimit = 0;                    // 速率限制（条/秒，0=不限）
config.asyncQueueSize = 8192;            // 异步队列大小（条数）
config.asyncThreads = 1;                 // 异步写入线程数

SyLogger::GetInstance().Initialize(config);
```

### 场景化配置

```cpp
// 后台服务：仅文件输出，不输出到控制台
SyLogConfig serviceConfig;
serviceConfig.logName = "BackgroundService";
serviceConfig.level = SyLogLevel::Info;   // 生产环境使用 Info 级别
serviceConfig.consoleEnable = false;      // 关闭控制台输出
serviceConfig.fileEnable = true;          // 启用文件输出
serviceConfig.maxAgeDays = 7;             // 保留7天
serviceConfig.maxFileSize = 100 * 1024 * 1024;  // 100MB
serviceConfig.maxFiles = 5;

// 开发调试：控制台 + 详细级别，不写文件
SyLogConfig debugConfig;
debugConfig.logName = "DebugApp";
debugConfig.level = SyLogLevel::Trace;   // 开发环境使用 Trace 级别
debugConfig.consoleEnable = true;
debugConfig.fileEnable = false;          // 不写文件，只输出到控制台

// 高吞吐场景：启用速率限制
SyLogConfig highVolumeConfig;
highVolumeConfig.logName = "HighVolumeApp";
highVolumeConfig.level = SyLogLevel::Info;
highVolumeConfig.rateLimit = 100;        // 每秒最多 100 条日志
```

## 日志级别

| 级别 | 值 | 说明 | 典型场景 |
|------|---|------|----------|
| Trace | 0 | 最详细跟踪 | 函数调用跟踪、性能分析、调试细节 |
| Debug | 1 | 调试信息 | 开发调试、变量值输出、流程跟踪 |
| Info | 2 | 一般信息 | 正常业务流程、用户操作、系统状态 |
| Warn | 3 | 警告 | 非关键问题、可恢复错误、潜在风险 |
| Error | 4 | 错误 | 功能错误、异常情况、需要处理的问题 |
| Critical | 5 | 严重错误 | 系统级错误、崩溃前记录、紧急问题 |
| Off | 6 | 关闭日志 | 完全禁用日志输出 |

### 运行时动态调整

```cpp
// 开发阶段：详细日志
SyLogger::GetInstance().SetLevel(SyLogLevel::Debug);

// 生产环境：仅错误日志
SyLogger::GetInstance().SetLevel(SyLogLevel::Error);

// 临时禁用日志（紧急情况下）
SyLogger::GetInstance().SetEnabled(false);

// 重新启用
SyLogger::GetInstance().SetEnabled(true);

// 检查当前状态
SyLogLevel currentLevel = SyLogger::GetInstance().GetLevel();
bool isEnabled = SyLogger::GetInstance().IsEnabled();
```

## API 参考

### 日志宏（推荐方式）

日志宏会自动捕获 `__FILE__` 和 `__LINE__`，便于定位问题。

```cpp
// 基础字符串日志
SY_TRACE(msg)      // 跟踪级别，最详细
SY_DEBUG(msg)      // 调试级别
SY_INFO(msg)       // 信息级别（常用）
SY_WARN(msg)       // 警告级别
SY_ERROR(msg)      // 错误级别
SY_CRITICAL(msg)   // 严重错误级别

// printf 风格格式化日志（推荐使用）
SY_TRACEF("fmt %d", arg)
SY_DEBUGF("fmt %s", arg)
SY_INFOF("fmt %.2f", arg)
SY_WARNF("fmt %s %d", arg1, arg2)
SY_ERRORF("fmt %s", arg)
SY_CRITICALF("fmt %d", arg)
```

### SyLogger 类接口

```cpp
// 单例访问
static SyLogger& GetInstance();

// 初始化方法
void Initialize(const SyLogConfig& config);
void Initialize(const char* logName = "SanYi",
                SyLogLevel level = SyLogLevel::Debug,
                bool consoleEnable = true,
                bool fileEnable = true);

// 生命周期管理
void Shutdown();  // 关闭日志系统，刷新缓冲区

// 运行时配置
void SetLevel(SyLogLevel level);
SyLogLevel GetLevel() const;
void SetEnabled(bool enabled);
bool IsEnabled() const;

// 文件管理
void CleanOldLogs();                    // 手动清理过期日志
const char* GetLogDirectory() const;    // 获取当前日志目录路径

// 全局默认路径设置
static void SetDefaultLogPath(const char* path);  // 设置全局默认日志路径
static const char* GetDefaultLogPath();           // 获取全局默认日志路径
```

### 带源位置的直接调用（宏内部使用）

一般情况下使用宏即可，不需要直接调用这些方法。

```cpp
// 字符串日志带源位置
void LogSrc(SyLogLevel level, const char* file, int line, const char* msg);

// printf 风格日志带源位置
void LogFSrc(SyLogLevel level, const char* file, int line, const char* fmt, ...);
```

### C 语言 API

提供 C 语言接口，方便跨语言调用（如 C、Python 等）。

```c
#include "Log/SyLogger.h"

// 初始化和关闭
void SyLog_Init(const char* logName, int level, bool console, bool file);
void SyLog_Shutdown();

// 配置
void SyLog_SetLevel(int level);       // level: 0=Trace, 1=Debug, ..., 5=Critical
void SyLog_SetEnabled(bool enabled);

// 日志输出
void SyLog_Trace(const char* msg);
void SyLog_Debug(const char* msg);
void SyLog_Info(const char* msg);
void SyLog_Warn(const char* msg);
void SyLog_Error(const char* msg);
void SyLog_Critical(const char* msg);
```

## 日志格式

### 默认格式

**控制台输出**（带颜色）：
```
[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v
```
示例：`[2026-01-24 14:31:00.973] [debug] 用户操作完成`

**文件输出**（带源位置和线程ID）：
```
[%Y-%m-%d %H:%M:%S] [%L] [%t] [%s:%#] %v
```
示例：`[2026-01-24 14:31:00] [I] [14068] [main.cpp:42] 用户操作完成`

### 格式占位符

| 占位符 | 说明 | 示例 |
|--------|------|------|
| `%Y-%m-%d %H:%M:%S.%e` | 完整时间戳（毫秒） | 2026-01-24 14:31:00.973 |
| `%l` / `%L` | 日志级别（完整/简短） | debug / D |
| `%^...%$` | 颜色控制（仅控制台） | 包裹的内容显示颜色 |
| `%v` | 日志消息内容 | 用户操作完成 |
| `%t` | 线程ID | 14068 |
| `%P` | 进程ID | 12345 |
| `%s:%#` | 源文件:行号（宏自动填充） | main.cpp:42 |
| `%!` | 函数名 | Initialize |

## 日志文件管理

### 文件结构

```
C:\Users\<用户名>\AppData\Local\AppName\logs\
├── AppName.log              # 当前日志文件
├── AppName.log.1            # 最近的历史日志（轮转一次）
├── AppName.log.2            # 较早的历史日志（轮转两次）
├── ...
├── AppName.log.9            # 最旧的历史日志（轮转九次）
├── AppName.error.log        # 错误日志（当 splitErrorLog=true，包含 Warn+）
├── AppName.error.log.1
└── ...
```

### 管理策略

- **轮转触发**：当前文件达到 `maxFileSize`（默认 10MB）时自动轮转
- **保留数量**：最多保留 `maxFiles`（默认 10）个历史文件，超过自动删除最旧的
- **自动清理**：超过 `maxAgeDays` 天的 `.log`/`.txt` 文件自动删除
- **错误分流**：`splitErrorLog=true` 时，Warn 及以上级别日志额外写入 `.error.log`
- **调试分流**：`splitDebugLog=true` 时，Trace/Debug 级别日志额外写入 `.debug.log`

## 最佳实践

### 性能优化

```cpp
// 1. 生产环境使用 Info 或更高级别，减少日志量
SyLogger::GetInstance().SetLevel(SyLogLevel::Info);

// 2. 日志写盘在异步线程，不影响业务逻辑性能
//    但仍需避免在热点路径中频繁输出大量日志

// 3. 使用格式化宏而非字符串拼接
// ❌ 错误：产生临时 string 对象，影响性能
SY_INFO("用户 " + std::string(username) + " 登录成功");

// ✅ 正确：直接传递格式化参数，零拷贝
SY_INFOF("用户 %s 登录成功", username);

// 4. 高吞吐场景启用速率限制
SyLogConfig config;
config.rateLimit = 100;  // 每秒最多 100 条
```

### 多线程使用

日志库是线程安全的，初始化后各线程直接调用宏，无需额外同步。

```cpp
SyLogger::GetInstance().Initialize("MyApp");

std::thread t1([]() {
    for (int i = 0; i < 100; ++i) {
        SY_INFOF("Thread 1: message %d", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
});

std::thread t2([]() {
    for (int i = 0; i < 100; ++i) {
        SY_INFOF("Thread 2: message %d", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
});

t1.join();
t2.join();
```

**写入顺序（FIFO）**

异步模式下，每次 `SY_INFO()` 调用会把消息推入一个线程安全的队列，后台 worker 线程按入队顺序依次处理。消息按入队时间排序，哪个线程先调用，那条日志就先写。

### 异常处理

```cpp
try {
    processData(data);
    SY_INFOF("数据处理成功，数量: %zu", data.size());
}
catch (const std::exception& e) {
    // 宏自动记录源位置，无需手动加 __FILE__/__LINE__
    SY_ERRORF("数据处理失败: %s", e.what());
}
catch (...) {
    SY_CRITICAL("未知异常");
}
```

### 日志目录查找

```cpp
// 获取当前日志目录
const char* logDir = SyLogger::GetInstance().GetLogDirectory();
SY_INFOF("日志目录: %s", logDir);

// 在控制台打印，方便开发调试
printf("日志目录: %s\n", logDir);
```

## 项目集成

### CMake 配置

```cmake
find_package(spdlog CONFIG REQUIRED)

target_link_libraries(YourTarget PRIVATE Log)

target_include_directories(YourTarget PRIVATE
    ${CMAKE_SOURCE_DIR}/Log/Log/Include
)
```

### 依赖安装

**必需依赖：spdlog**

```powershell
# 使用 vcpkg 安装（推荐）
vcpkg install spdlog:x64-windows

# 或使用 Conan
conan install spdlog/1.9.0@
```

### 示例程序

示例程序位于 `Log/Example/main.cpp`，演示了基本用法：

```cpp
#include "Log/SyLogger.h"
#include <thread>

int main() {
    // 初始化日志系统
    SyLogger::GetInstance().Initialize("SyLogExample");
    
    // 输出不同级别的日志
    SY_TRACE("这是一条 Trace 日志");
    SY_DEBUG("这是一条 Debug 日志");
    SY_INFO("这是一条 Info 日志");
    SY_WARN("这是一条 Warn 日志");
    SY_ERROR("这是一条 Error 日志");
    SY_CRITICAL("这是一条 Critical 日志");
    
    // 格式化日志
    SY_INFOF("格式化示例：整数=%d, 浮点数=%.2f, 字符串=%s", 42, 3.14, "hello");
    
    // 获取日志目录并输出
    const char* logDir = SyLogger::GetInstance().GetLogDirectory();
    SY_INFOF("日志文件位于: %s", logDir);
    
    // 多线程日志
    std::thread t([]() {
        for (int i = 0; i < 5; ++i) {
            SY_INFOF("子线程消息 %d", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    t.join();
    
    // 关闭日志系统
    SyLogger::GetInstance().Shutdown();
    
    printf("\n日志已写入，请查看目录: %s\n", logDir);
    return 0;
}
```

## 故障排除

### 常见问题

**日志文件未生成**

1. 检查目录权限是否正确
   - 默认路径（`%LOCALAPPDATA%`）应该有写入权限
   - 自定义路径需要确保程序有写入权限
2. 确认 `fileEnable = true`
3. 查看控制台是否有错误输出
4. 调用 `GetLogDirectory()` 确认实际使用的路径

**性能问题**

1. 降低日志级别（生产环境使用 Info 或更高）
2. 启用速率限制（`rateLimit`）
3. 避免在性能关键路径输出 Trace/Debug 日志
4. 检查 `asyncQueueSize` 是否足够大

**格式显示异常**

1. 控制台颜色不支持：检查终端类型（Windows 终端需要支持 VT100 颜色）
2. 中文乱码：确保使用 UTF-8 编码（MSVC 编译时添加 `/utf-8` 选项）
3. `%s:%#` 显示空：确认使用了宏（而非直接调用字符串方法）

**权限问题**

1. 不要使用系统根目录（如 `C:\`、`C:\Windows\`）
2. 不要使用 Program Files 目录（需要管理员权限）
3. 推荐使用默认路径或用户临时目录

### 调试技巧

```cpp
// 启用详细日志
SyLogger::GetInstance().SetLevel(SyLogLevel::Trace);

// 查看日志目录（关键！）
const char* logDir = SyLogger::GetInstance().GetLogDirectory();
printf("日志目录: %s\n", logDir);  // 在控制台打印
SY_INFOF("日志目录: %s", logDir);  // 写入日志文件

// 手动清理旧日志
SyLogger::GetInstance().CleanOldLogs();

// 检查日志是否启用
if (SyLogger::GetInstance().IsEnabled()) {
    SY_INFO("日志系统已启用");
}
```

## 技术规格

- **C++ 标准**：C++17 或更高
- **依赖库**：spdlog 1.9.0+
- **支持平台**：Windows / Linux / macOS
- **支持编译器**：MSVC 2019+ / GCC 9+ / Clang 10+
- **线程模型**：异步写入，线程安全
- **默认编码**：UTF-8

## 测试

测试代码位于 `Log/Test/LogTests.cpp`，包含以下测试用例：

| 测试类别 | 测试内容 |
|----------|----------|
| SyLogLevelTest | 日志级别枚举值和比较 |
| SyLogConfigTest | 配置结构体默认值和自定义配置 |
| SyLoggerTest | 单例模式、初始化、关闭、级别控制、字符串日志、格式化日志、日志目录、并发日志、性能测试、错误处理 |
| SyLoggerMacroTest | 日志宏功能验证 |

运行测试：

```powershell
# 构建测试
cmake --build "Log/build" --target LogTests --config Debug

# 运行测试
cd "Log/build"
ctest --build-config Debug --output-on-failure
```

测试日志文件会输出到默认路径（`%LOCALAPPDATA%\TestApp\logs\` 等），不会使用需要管理员权限的路径。