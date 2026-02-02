# SyLogger 日志库

基于 **spdlog** 的高性能 C++ 日志库，提供简洁易用的日志接口。

## 核心特性

- **fmt 格式化**：使用 `{}` 占位符，类型安全的参数格式化
- **按天轮转**：每天自动生成新日志文件，支持自动清理过期文件（默认30天）
- **多级别输出**：Trace / Debug / Info / Warn / Error / Critical
- **双通道输出**：控制台彩色输出 + 文件持久化
- **智能路径管理**：自动使用用户目录，避免权限问题
- **高性能**：基于 spdlog，异步日志，低延迟
- **线程安全**：多线程环境下安全使用

## 快速开始

```cpp
#include "Log/SyLogger.h"

int main() {
    // 初始化日志系统
    SyLogger::GetInstance().Initialize("MyApp");
    
    // 基础日志输出
    SY_INFO("程序启动");
    SY_WARN("这是警告");
    
    // fmt 格式化日志（推荐）
    SY_INFOF("用户 {} 登录成功", username);
    SY_INFOF("处理 {} 条数据，耗时 {:.2f} 秒", count, elapsed);
    SY_ERRORF("错误码: {}, 文件: {}", code, filename);
    
    // 程序退出前关闭
    SyLogger::GetInstance().Shutdown();
    return 0;
}
```

## 日志存储路径

| 系统 | 默认路径 |
|------|----------|
| Windows | `C:\Users\<用户>\AppData\Local\<AppName>\logs\` |
| Linux | `~/.local/share/<AppName>/logs/` |
| macOS | `~/.local/share/<AppName>/logs/` |

## 配置选项

### 基础配置

```cpp
SyLogConfig config;
config.logName = "AppName";          // 日志名称
config.logPath = "";                 // 留空使用默认路径
config.level = SyLogLevel::Debug;    // 日志级别
config.consoleEnable = true;         // 启用控制台输出
config.fileEnable = true;            // 启用文件输出
config.maxAgeDays = 30;              // 日志保留天数

SyLogger::GetInstance().Initialize(config);
```

### 场景化配置

```cpp
// 后台服务：仅文件输出
SyLogConfig serviceConfig;
serviceConfig.logName = "BackgroundService";
serviceConfig.level = SyLogLevel::Info;
serviceConfig.consoleEnable = false;
serviceConfig.fileEnable = true;
serviceConfig.maxAgeDays = 7;

// 开发调试：仅控制台输出
SyLogConfig debugConfig;
debugConfig.logName = "DebugApp";
debugConfig.level = SyLogLevel::Trace;
debugConfig.consoleEnable = true;
debugConfig.fileEnable = false;
```

## 日志级别

| 级别 | 值 | 说明 | 典型场景 |
|------|---|------|----------|
| Trace | 0 | 最详细跟踪 | 函数调用跟踪、性能分析 |
| Debug | 1 | 调试信息 | 开发调试、变量值输出 |
| Info | 2 | 一般信息 | 正常业务流程、用户操作 |
| Warn | 3 | 警告 | 非关键问题、可恢复错误 |
| Error | 4 | 错误 | 功能错误、异常情况 |
| Critical | 5 | 严重错误 | 系统级错误、崩溃前记录 |
| Off | 6 | 关闭日志 | 完全禁用日志输出 |

### 运行时动态调整

```cpp
// 开发阶段：详细日志
SyLogger::GetInstance().SetLevel(SyLogLevel::Debug);

// 生产环境：仅错误日志
SyLogger::GetInstance().SetLevel(SyLogLevel::Error);

// 临时禁用日志
SyLogger::GetInstance().SetEnabled(false);
```

## API 参考

### 日志宏

```cpp
// 基础字符串日志
SY_TRACE(msg)      // 跟踪级别
SY_DEBUG(msg)      // 调试级别
SY_INFO(msg)       // 信息级别
SY_WARN(msg)       // 警告级别
SY_ERROR(msg)      // 错误级别
SY_CRITICAL(msg)   // 严重错误级别

// fmt 格式化日志（推荐）
SY_TRACEF("format {}", arg)
SY_DEBUGF("format {}", arg)
SY_INFOF("format {}", arg)
SY_WARNF("format {}", arg)
SY_ERRORF("format {}", arg)
SY_CRITICALF("format {}", arg)
```

### SyLogger 类接口

```cpp
// 单例访问
static SyLogger& GetInstance();

// 初始化方法
void Initialize(const SyLogConfig& config);
void Initialize(const std::string& logName = "SanYi",
                SyLogLevel level = SyLogLevel::Debug,
                bool consoleEnable = true, 
                bool fileEnable = true);

// 生命周期管理
void Shutdown();

// 运行时配置
void SetLevel(SyLogLevel level);
SyLogLevel GetLevel() const;
void SetEnabled(bool enabled);
bool IsEnabled() const;

// 文件管理
void CleanOldLogs();
std::string GetLogDirectory() const;
```

## 日志格式

### 默认格式

**控制台输出**（带颜色）：
```
[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v
```
示例：`[2026-01-24 14:31:00.973] [debug] 用户操作完成`

**文件输出**（带线程ID）：
```
[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v
```
示例：`[2026-01-24 14:31:00.973] [debug] [14068] 用户操作完成`

### 格式占位符

| 占位符 | 说明 | 示例 |
|--------|------|------|
| `%Y-%m-%d %H:%M:%S.%e` | 完整时间戳（毫秒） | 2026-01-24 14:31:00.973 |
| `%l` / `%L` | 日志级别（完整/简短） | debug / D |
| `%^...%$` | 颜色控制（仅控制台） | - |
| `%v` | 日志消息内容 | 用户操作完成 |
| `%t` | 线程ID | 14068 |
| `%P` | 进程ID | 12345 |
| `%s:%#` | 源文件:行号 | main.cpp:25 |
| `%!` | 函数名 | Initialize |

### 自定义格式示例

```cpp
// 完整调试格式
"[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%#] %v"
// 输出：[2026-01-24 14:31:00.973] [debug] [14068] [main.cpp:25] 函数调用

// 简洁格式
"%Y-%m-%d %H:%M:%S | %-8l | %v"
// 输出：2026-01-24 14:31:00 | debug    | 用户操作

// JSON格式（便于日志分析工具）
R"({"time":"%Y-%m-%dT%H:%M:%S.%eZ","level":"%l","thread":%t,"message":"%v"})"
```

## 最佳实践

### 性能优化

```cpp
// 1. 生产环境使用合适的日志级别
SyLogger::GetInstance().SetLevel(SyLogLevel::Info);

// 2. 避免在循环中频繁输出低级别日志
for (auto& item : items) {
    // ❌ 错误：频繁的调试日志
    // SY_DEBUGF("处理项目: {}", item.id);
    
    // ✅ 正确：仅记录关键信息
    if (item.isSpecial) {
        SY_INFOF("特殊项目处理: {}", item.id);
    }
}

// 3. 使用格式化而非字符串拼接
// ❌ 错误：性能差
SY_INFO("用户 " + username + " 登录成功");

// ✅ 正确：高性能
SY_INFOF("用户 {} 登录成功", username);
```

### 多线程使用

```cpp
// 日志库是线程安全的，无需额外同步
std::thread t1([]() {
    SY_INFO("线程1开始工作");
});

std::thread t2([]() {
    SY_INFO("线程2开始工作");
});

t1.join();
t2.join();
```

### 异常处理

```cpp
try {
    processData(data);
    SY_INFOF("数据处理成功，数量: {}", data.size());
} 
catch (const std::exception& e) {
    // 记录详细错误信息和上下文
    SY_ERRORF("数据处理失败: {}, 文件: {}, 行号: {}", 
              e.what(), __FILE__, __LINE__);
    
    SY_DEBUGF("失败时的数据状态: size={}, valid={}", 
              data.size(), data.isValid());
}
```

## 日志文件管理

### 文件结构

```
C:\Users\<用户>\AppData\Local\AppName\logs\
├── AppName.log              # 当前日志文件
├── AppName_2026-01-24.log   # 历史日志（按日期）
├── AppName_2026-01-23.log
└── ...
```

### 管理策略

- **自动轮转**：每天 00:00 自动创建新文件
- **命名规则**：`{AppName}_{YYYY-MM-DD}.log`
- **自动清理**：超过配置天数的文件自动删除（默认30天）
- **大小控制**：由 spdlog 自动管理，无需手动干预

## 项目集成

### CMake 配置

```cmake
# 链接日志库
target_link_libraries(YourTarget PRIVATE Log)

# 包含头文件路径
target_include_directories(YourTarget PRIVATE 
    ${CMAKE_SOURCE_DIR}/Log/Log/Include
)
```

### 依赖安装

**必需依赖：spdlog**

```powershell
# 使用 vcpkg 安装
vcpkg install spdlog:x64-windows

# 或使用 Conan
conan install spdlog/1.11.0@
```

## 故障排除

### 常见问题

**日志文件未生成**
- 检查目录权限是否正确
- 确认 `fileEnable = true`
- 查看控制台是否有错误输出

**性能问题**
- 降低日志级别（生产环境使用 Info 或更高）
- 避免在性能关键路径输出 Trace/Debug 日志
- 减少循环中的日志输出频率

**格式显示异常**
- 控制台颜色不支持：检查终端类型
- 中文乱码：确保使用 UTF-8 编码

### 调试技巧

```cpp
// 启用详细日志
SyLogger::GetInstance().SetLevel(SyLogLevel::Trace);

// 查看日志目录
std::string logDir = SyLogger::GetInstance().GetLogDirectory();
SY_INFOF("日志目录: {}", logDir);

// 手动清理旧日志
SyLogger::GetInstance().CleanOldLogs();
```

## 技术规格

- **C++ 标准**：C++17 或更高
- **依赖库**：spdlog 1.9.0+
- **支持平台**：Windows / Linux / macOS
- **支持编译器**：MSVC / GCC / Clang

