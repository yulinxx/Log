# Log Library (Log.dll)

## 功能描述

日志库，基于 spdlog 封装，提供统一的日志记录接口。支持多级别日志、日志格式化、日志输出到文件/控制台等功能。

### 核心特性

- **多级别日志**：Trace / Debug / Info / Warn / Error / Critical / Off 七个级别
- **双通道输出**：控制台彩色输出 + 文件持久化
- **异步日志**：日志写盘在后台线程处理，不阻塞业务线程
- **大小 + 数量轮转**：按文件大小轮转，保留固定数量的历史文件
- **日志分流**：可单独输出 Error 日志（Warn+）和 Debug 日志（Trace/Debug）
- **速率限制**：可配置每秒最大日志条数，防止日志洪泛
- **自动清理**：超期日志文件自动删除（默认 30 天）
- **源位置自动捕获**：宏自动记录 `__FILE__`、`__LINE__`
- **线程安全**：多线程环境下安全使用
- **C 语言 API**：提供 `extern "C"` 接口，方便跨语言调用
- **TraceId 传播**：跨模块 traceId 上下文（thread-local 栈）

---

## 使用方法

### C++ 接口使用方法

#### 1. 初始化日志系统

```cpp
#include "Log/SyLogger.h"

// 方式一：简易初始化
SyLogger::GetInstance().Initialize("MyApp");

// 方式二：完整配置
SyLogConfig config;
config.logName = "MyApp";
config.level = SyLogLevel::Debug;
config.consoleEnable = true;
config.fileEnable = true;
config.maxAgeDays = 30;
config.splitErrorLog = true;
config.splitDebugLog = false;
config.maxFileSize = 10 * 1024 * 1024;
config.maxFiles = 10;
config.rateLimit = 0;
SyLogger::GetInstance().Initialize(config);
```

#### 2. 输出日志（推荐使用宏）

```cpp
// 基础字符串日志（自动捕获 __FILE__ / __LINE__）
SY_TRACE("跟踪信息");
SY_DEBUG("调试信息");
SY_INFO("一般信息");
SY_WARN("警告");
SY_ERROR("错误");
SY_CRITICAL("严重错误");

// printf 风格格式化日志
SY_INFOF("用户 %s 登录成功", username);
SY_INFOF("处理 %d 条数据，耗时 %.2f 秒", count, elapsed);
SY_ERRORF("错误码: %d, 详情: %s", code, detail);
```

#### 3. 运行时控制

```cpp
// 动态调整级别
SyLogger::GetInstance().SetLevel(SyLogLevel::Warn);

// 动态启用/禁用
SyLogger::GetInstance().SetEnabled(false);
SyLogger::GetInstance().SetEnabled(true);

// 检查状态
SyLogLevel currentLevel = SyLogger::GetInstance().GetLevel();
bool isEnabled = SyLogger::GetInstance().IsEnabled();
```

#### 4. 关闭日志系统

```cpp
SyLogger::GetInstance().Shutdown();
```

### C API 使用方法（SyLoggerDLL.h）

`SyLoggerDLL.h` 提供了独立的 C 语言接口，适用于跨编译器、跨运行时的进程间调用场景。

#### 初始化与关闭

```c
#include "Log/SyLoggerDLL.h"

// 简易初始化
SyLoggerDLL_Initialize("MyApp", 1, true, true);

// 完整配置初始化
SyLogConfig config;
config.logName = "MyApp";
config.level = 2;  // Info
SyLoggerDLL_InitializeWithConfig(&config);

// 关闭
SyLoggerDLL_Shutdown();
```

#### 日志输出

```c
// 简单日志
SyLoggerDLL_Trace("跟踪信息");
SyLoggerDLL_Debug("调试信息");
SyLoggerDLL_Info("一般信息");
SyLoggerDLL_Warn("警告");
SyLoggerDLL_Error("错误");
SyLoggerDLL_Critical("严重错误");

// 格式化日志
SyLoggerDLL_InfoF("用户 %s 登录成功", username);
SyLoggerDLL_ErrorF("错误码: %d", code);

// 带源代码位置的日志
SyLoggerDLL_LogSrc(2, __FILE__, __LINE__, "带源位置的日志");
SyLoggerDLL_LogFSrc(2, __FILE__, __LINE__, "格式化 %s", "参数");
```

#### 控制接口

```c
SyLoggerDLL_SetLevel(3);       // 设置级别: 0=Trace, 1=Debug, 2=Info, 3=Warn, 4=Error, 5=Critical
SyLoggerDLL_SetEnabled(true);
bool enabled = SyLoggerDLL_IsEnabled();
SyLoggerDLL_CleanOldLogs();
const char* logDir = SyLoggerDLL_GetLogDirectory();
```

#### C++ 包装器

C++ 项目中推荐使用 `SyLoggerDLLWrapper` 类，它封装了 C API 调用并提供更便捷的使用方式：

```cpp
#include "Log/SyLoggerDLL.h"

SyLoggerDLLWrapper::GetInstance().Initialize("MyApp");
SyLoggerDLLWrapper::GetInstance().Info("日志消息");
SyLoggerDLLWrapper::GetInstance().InfoF("格式化 %s", "参数");
SyLoggerDLLWrapper::GetInstance().Shutdown();
```

---

## 设计框架

### 单例模式

`SyLogger` 采用单例模式，全局只有一个日志器实例：

```cpp
class LOG_API SyLogger
{
public:
    static SyLogger& GetInstance();
    // ...
private:
    SyLogger();
    ~SyLogger();
    SyLogger(const SyLogger&) = delete;
    SyLogger& operator=(const SyLogger&) = delete;
};
```

通过 `static SyLogger instance` 实现线程安全的局部静态变量单例。

### 日志配置 (SyLogConfig)

`SyLogConfig` 采用两层结构分离设计：

- **导出层**：`SyLogConfig` 使用 `const char*` 字符串，确保 ABI 兼容性
- **内部层**：`SyLogConfigInternal` 使用 `std::string`，提供更好的内存管理

```cpp
struct SyLogConfig
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

    operator SyLogConfigInternal() const;
};
```

通过 `operator SyLogConfigInternal()` 转换运算符，在初始化时将导出层配置转换为内部使用的配置结构。

### 日志级别 (SyLogLevel)

```cpp
enum class SyLogLevel
{
    Trace = 0,      // 最详细跟踪
    Debug = 1,      // 调试信息
    Info = 2,       // 一般信息
    Warn = 3,       // 警告
    Error = 4,      // 错误
    Critical = 5,   // 严重错误
    Off = 6         // 关闭日志
};
```

日志级别映射到 spdlog 级别枚举，支持运行时动态调整。

### 日志上下文 (SyTraceContext)

跨模块 TraceId 传播机制，使用 thread-local 栈实现：

```cpp
namespace SyTrace
{
    LOG_API void pushTraceId(const char* traceId);
    LOG_API void popTraceId();
    LOG_API size_t currentTraceId(char* buffer, size_t bufferSize);
    LOG_API size_t resolveTraceId(const char* explicitId, char* buffer, size_t bufferSize);
    inline std::string currentTraceIdString();
    inline std::string resolveTraceIdString(const char* explicitId = nullptr);
    inline constexpr const char* kTraceHeaderName = "X-Trace-Id";
}
```

- `pushTraceId` / `popTraceId`：操作 traceId 栈（`const char*`，DLL 内拷贝）
- `currentTraceId`：拷贝栈顶 traceId 到调用方缓冲区，返回写入长度（ABI 安全）
- `resolveTraceId`：优先使用显式 ID，否则回退到当前栈顶
- `currentTraceIdString` / `resolveTraceIdString`：header 内联便捷包装，编译进调用方，不跨 DLL 边界
- `kTraceHeaderName`：HTTP 头名称，供 Network 模块统一使用

### 两层结构分离

日志库的核心设计原则，确保 DLL 跨边界 ABI 兼容性：

| 层次 | 用途 | 字符串类型 | 说明 |
|------|------|-----------|------|
| 内部层 | DLL 内部使用 | `std::string` / `std::shared_ptr` | 享受 STL 的内存管理便利 |
| 导出层 | 跨 DLL 边界导出 | `const char*` / 原始指针 | 确保 ABI 稳定 |

**具体实现：**

1. **pImpl 模式**：`SyLogger` 使用 `SyLoggerImpl*`（原始指针）而非 `std::unique_ptr`，避免 MSVC C4251 警告（导出类中的 `std::unique_ptr<前置声明类型>` 模板实例无法跨 DLL 边界安全销毁）
2. **配置转换**：`SyLogConfig`（导出层）通过转换运算符转换为 `SyLogConfigInternal`（内部层）
3. **内存管理**：`GetLogDirectory()` 和 `GetDefaultLogPath()` 使用 `static std::string` 缓存返回值，确保返回的 `const char*` 指针在调用期间有效

---

## 依赖库

### spdlog

高性能 C++ 日志库，提供异步日志、文件轮转、多 Sink 支持等核心能力。

- **版本要求**：spdlog 1.9.0+
- **官方仓库**：https://github.com/gabime/spdlog

### 依赖库安装方法

#### 方式一：vcpkg（推荐）

```powershell
# 安装 spdlog
vcpkg install spdlog:x64-windows

# 集成到 CMake
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]\scripts\buildsystems\vcpkg.cmake ..
```

#### 方式二：Conan

```powershell
conan install spdlog/1.9.0@
```

#### 方式三：CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.9.0
)
FetchContent_MakeAvailable(spdlog)
```

---

## 构建配置

### CMake 配置说明

#### 模块 CMakeLists.txt 关键配置

```cmake
cmake_minimum_required(VERSION 4.3)

set(LIB_NAME "Log")
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

find_package(spdlog CONFIG REQUIRED)

add_library(${LIB_NAME} SHARED
    ${LIB_SOURCES}
    ${LIB_HEADERS}
)

target_include_directories(${LIB_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_compile_features(${LIB_NAME} PUBLIC cxx_std_17)

target_link_libraries(${LIB_NAME}
    PRIVATE
        spdlog::spdlog
)

target_compile_definitions(${LIB_NAME} PRIVATE LOG_EXPORTS)

if(MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /utf-8 /FS)
endif()
```

#### 项目集成

在项目的 CMakeLists.txt 中集成 Log 模块：

```cmake
# 添加 Log 子目录
add_subdirectory(Log)

# 链接 Log 库到你的目标
target_link_libraries(YourTarget PRIVATE Log)

# 或直接引入头文件目录
target_include_directories(YourTarget PRIVATE
    ${CMAKE_SOURCE_DIR}/Log/Log/Include
)
```

#### 构建选项

```cmake
option(BUILD_LOG_TESTS "Build Log module tests" ON)
option(BUILD_LOG_EXAMPLE "Build Log usage example" ON)
```

- `BUILD_LOG_TESTS`：构建单元测试
- `BUILD_LOG_EXAMPLE`：构建示例程序

---

## API 概要

### SyLogger 类主要方法

#### 生命周期

| 方法 | 说明 |
|------|------|
| `static SyLogger& GetInstance()` | 获取单例实例 |
| `void Initialize(const SyLogConfig& config)` | 使用配置结构体初始化 |
| `void Initialize(const char* logName, SyLogLevel level, bool console, bool file)` | 使用简化参数初始化 |
| `void Shutdown()` | 关闭日志系统，刷新缓冲区 |

#### 级别控制

| 方法 | 说明 |
|------|------|
| `void SetLevel(SyLogLevel level)` | 设置日志级别 |
| `SyLogLevel GetLevel() const` | 获取当前日志级别 |
| `void SetEnabled(bool enabled)` | 启用/禁用日志 |
| `bool IsEnabled() const` | 检查日志是否启用 |

#### 日志输出（带源位置）

| 方法 | 说明 |
|------|------|
| `void LogSrc(SyLogLevel level, const char* file, int line, const char* msg)` | 带源位置的字符串日志 |
| `void LogSrc(SyLogLevel level, const char* file, int line, const std::string& msg)` | 带源位置的 std::string 日志（内联） |
| `void LogFSrc(SyLogLevel level, const char* file, int line, const char* fmt, ...)` | 带源位置的格式化日志 |

#### 日志输出（不带源位置）

| 方法 | 说明 |
|------|------|
| `void TraceStr(const char* msg)` | Trace 级别字符串日志 |
| `void DebugStr(const char* msg)` | Debug 级别字符串日志 |
| `void InfoStr(const char* msg)` | Info 级别字符串日志 |
| `void WarnStr(const char* msg)` | Warn 级别字符串日志 |
| `void ErrorStr(const char* msg)` | Error 级别字符串日志 |
| `void CriticalStr(const char* msg)` | Critical 级别字符串日志 |
| `void TraceF(const char* fmt, ...)` | Trace 级别格式化日志 |
| `void DebugF(const char* fmt, ...)` | Debug 级别格式化日志 |
| `void InfoF(const char* fmt, ...)` | Info 级别格式化日志 |
| `void WarnF(const char* fmt, ...)` | Warn 级别格式化日志 |
| `void ErrorF(const char* fmt, ...)` | Error 级别格式化日志 |
| `void CriticalF(const char* fmt, ...)` | Critical 级别格式化日志 |

#### 文件管理

| 方法 | 说明 |
|------|------|
| `void CleanOldLogs()` | 手动清理过期日志文件 |
| `const char* GetLogDirectory() const` | 获取当前日志目录路径 |
| `static void SetLogPathCallback(LogPathCallback cb, void* ctx)` | 设置日志目录回调（C 函数指针 + ctx，避免跨 DLL std::function） |
| `static void SetDefaultLogPath(const char* path)` | 设置全局默认日志路径 |
| `static const char* GetDefaultLogPath()` | 获取全局默认日志路径 |

### C API 函数

#### SyLogger.h 中的 C API

```c
void SyLog_Init(const char* logName, int level, bool console, bool file);
void SyLog_Shutdown();
void SyLog_SetLevel(int level);
void SyLog_SetEnabled(bool enabled);

void SyLog_Trace(const char* msg);
void SyLog_Debug(const char* msg);
void SyLog_Info(const char* msg);
void SyLog_Warn(const char* msg);
void SyLog_Error(const char* msg);
void SyLog_Critical(const char* msg);
```

#### SyLoggerDLL.h 中的 C API（完整版本）

##### 初始化与配置

```c
void SyLoggerDLL_Initialize(const char* logName, int level, bool console, bool file);
void SyLoggerDLL_InitializeWithConfig(const SyLogConfig* config);
void SyLoggerDLL_Shutdown();
void SyLoggerDLL_SetLevel(int level);
void SyLoggerDLL_SetEnabled(bool enabled);
bool SyLoggerDLL_IsEnabled();
```

##### 文件管理

```c
void SyLoggerDLL_CleanOldLogs();
const char* SyLoggerDLL_GetLogDirectory();
void SyLoggerDLL_SetDefaultLogPath(const char* path);
const char* SyLoggerDLL_GetDefaultLogPath();
```

##### 日志输出

```c
void SyLoggerDLL_Trace(const char* msg);
void SyLoggerDLL_Debug(const char* msg);
void SyLoggerDLL_Info(const char* msg);
void SyLoggerDLL_Warn(const char* msg);
void SyLoggerDLL_Error(const char* msg);
void SyLoggerDLL_Critical(const char* msg);

void SyLoggerDLL_TraceF(const char* fmt, ...);
void SyLoggerDLL_DebugF(const char* fmt, ...);
void SyLoggerDLL_InfoF(const char* fmt, ...);
void SyLoggerDLL_WarnF(const char* fmt, ...);
void SyLoggerDLL_ErrorF(const char* fmt, ...);
void SyLoggerDLL_CriticalF(const char* fmt, ...);

void SyLoggerDLL_LogSrc(int level, const char* file, int line, const char* msg);
void SyLoggerDLL_LogFSrc(int level, const char* file, int line, const char* fmt, ...);
```

### 日志宏

```cpp
// 字符串日志宏（自动捕获源位置）
SY_TRACE(msg)
SY_DEBUG(msg)
SY_INFO(msg)
SY_WARN(msg)
SY_ERROR(msg)
SY_CRITICAL(msg)

// 格式化日志宏（自动捕获源位置）
SY_TRACEF(...)
SY_DEBUGF(...)
SY_INFOF(...)
SY_WARNF(...)
SY_ERRORF(...)
SY_CRITICALF(...)
```

### SyTrace 命名空间 API

```cpp
void pushTraceId(const char* traceId);
void popTraceId();
size_t currentTraceId(char* buffer, size_t bufferSize);
size_t resolveTraceId(const char* explicitId, char* buffer, size_t bufferSize);
// 便捷包装（header 内联，编译进调用方，不跨 DLL 边界）
std::string currentTraceIdString();
std::string resolveTraceIdString(const char* explicitId = nullptr);
```

---

## ABI 说明

### 两层结构分离模式

日志库采用两层结构分离模式设计，确保 DLL 跨边界的 ABI 稳定性：

**问题背景**
- C++ STL 类型（如 `std::string`、`std::shared_ptr`）在不同编译器、不同运行时版本间可能存在 ABI 不兼容
- 直接在 DLL 导出接口中使用 STL 类型，会导致跨模块/跨编译器调用时出现内存管理问题

**解决方案**

| 层次 | 数据类型 | 使用场景 |
|------|---------|---------|
| 导出层（DLL 边界） | `const char*`、原始指针、基础类型 | 确保 ABI 稳定 |
| 内部层（DLL 内部） | `std::string`、`std::shared_ptr`、`std::mutex` | 享受 STL 便利 |

**关键实现：**

1. **SyLogConfig 导出层**：使用 `const char*` 字符串
   ```cpp
   struct LOG_API SyLogConfig
   {
       const char* logName;  // 导出层使用 const char*
       operator SyLogConfigInternal() const;  // 转换到内部层
   };
   ```

2. **SyLogConfigInternal 内部层**：使用 `std::string`
   ```cpp
   struct SyLogConfigInternal
   {
       std::string logName;  // 内部使用 std::string
       // ...
   };
   ```

3. **SyLogger 类**：使用 pImpl 模式，导出类只包含原始指针
   ```cpp
   class LOG_API SyLogger
   {
   private:
       SyLoggerImpl* m_impl;  // 原始指针，避免 unique_ptr 的 C4251 警告
   };
   ```

### 内联函数兼容模式

对于需要跨 DLL 边界的轻量级接口，使用内联函数模式：

```cpp
// SyLogConfig 中的转换运算符在 DLL 边界内联执行
operator SyLogConfigInternal() const
{
    SyLogConfigInternal internal;
    internal.logName = logName ? logName : "";  // 在调用方进程内完成转换
    // ...
    return internal;
}
```

### 固定缓冲区模式

对于返回字符串指针的接口，使用静态缓冲区确保指针有效性：

```cpp
const char* SyLogger::GetLogDirectory() const
{
    static std::string cachedPath;  // 静态局部变量，生命周期与程序一致
    cachedPath = m_impl->m_config.logPath;
    return cachedPath.c_str();
}
```

**注意事项：**
- 返回的 `const char*` 指针在下次调用同一方法前保持有效
- 调用方不应长期持有该指针，应立即复制需要的内容
- `GetDefaultLogPath()` 同样使用此模式

### 导出宏

```cpp
// LogAPI.h
#if defined(_WIN32) || defined(_WIN64)
#ifdef LOG_EXPORTS
#define LOG_API __declspec(dllexport)
#else
#define LOG_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#ifdef LOG_EXPORTS
#define LOG_API __attribute__((visibility("default")))
#else
#define LOG_API
#endif
#elif defined(__APPLE__)
#ifdef LOG_EXPORTS
#define LOG_API __attribute__((visibility("default")))
#else
#define LOG_API
#endif
#else
#define LOG_API
#endif
```

---

## 日志文件管理

### 文件结构

```
%LOCALAPPDATA%\<AppName>\logs\
├── <AppName>.log              # 主日志文件
├── <AppName>.log.1            # 历史日志（轮转）
├── <AppName>.log.2
├── ...
├── <AppName>.error.log        # 错误日志（Warn+，当 splitErrorLog=true）
├── <AppName>.error.log.1
└── ...
```

### 管理策略

- **轮转触发**：当前文件达到 `maxFileSize`（默认 10MB）时自动轮转
- **保留数量**：最多保留 `maxFiles`（默认 10）个历史文件
- **自动清理**：超过 `maxAgeDays` 天的 `.log`/`.txt` 文件自动删除
- **错误分流**：`splitErrorLog=true` 时，Warn 及以上级别日志额外写入 `.error.log`
- **调试分流**：`splitDebugLog=true` 时，Trace/Debug 级别日志额外写入 `.debug.log`

### 默认存储路径

| 系统 | 默认路径 |
|------|----------|
| Windows | `C:\Users\<用户名>\AppData\Local\<AppName>\logs\` |
| Linux | `~/.local/share/<AppName>/logs/` |
| macOS | `~/.local/share/<AppName>/logs/` |

---

## 日志格式

### 控制台输出

```
[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v
```
示例：`[2026-01-24 14:31:00.973] [debug] 用户操作完成`

### 文件输出

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
| `%s:%#` | 源文件:行号 | main.cpp:42 |

---

## 技术规格

- **C++ 标准**：C++17 或更高
- **依赖库**：spdlog 1.9.0+
- **支持平台**：Windows / Linux / macOS
- **支持编译器**：MSVC 2019+ / GCC 9+ / Clang 10+
- **线程模型**：异步写入，线程安全
- **默认编码**：UTF-8

---

## 版本信息

**当前版本：1.0.0**

- `VERSION_MAJOR = 1`
- `VERSION_MINOR = 0`
- `VERSION_PATCH = 0`
- `PROJECT_VERSION = "1.0.0"`