#pragma once

#include "LogAPI.h"

#include <string>

/**
 * @brief 跨模块 traceId 传播（thread-local 栈）
 *
 * UI OperationTraceScope 在入栈/出栈时同步到此上下文，
 * PythonHost / Network 等模块可读取当前用户操作 traceId。
 */
namespace SyTrace
{
    LOG_API void pushTraceId(const std::string& traceId);
    LOG_API void popTraceId();

    /** @brief 栈顶 traceId；无活跃 trace 时返回空字符串 */
    LOG_API std::string currentTraceId();

    /** @brief 优先使用 explicitId，否则回退到 currentTraceId() */
    LOG_API std::string resolveTraceId(const std::string& explicitId = {});

    /** @brief HTTP 头名称，供 Network 模块统一使用 */
    inline constexpr const char* kTraceHeaderName = "X-Trace-Id";
} // namespace SyTrace
