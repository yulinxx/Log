#pragma once

#include "LogAPI.h"

#include <cstddef>
#include <string>

/**
 * @brief 跨模块 traceId 传播（thread-local 栈）
 *
 * UI OperationTraceScope 在入栈/出栈时同步到此上下文，
 * PythonHost / Network 等模块可读取当前用户操作 traceId。
 *
 * ABI 说明：导出函数不使用 std::string 参数/返回值，改为调用方缓冲区
 * （const char* + size）形式；下方 currentTraceIdString / resolveTraceIdString
 * 为 header 内联便捷包装，编译进调用方，不跨 DLL 边界。
 */
namespace SyTrace
{
    /** @brief 入栈（拷贝 traceId）；null/空串忽略 */
    LOG_API void pushTraceId(const char* traceId);

    /** @brief 出栈；栈空时忽略 */
    LOG_API void popTraceId();

    /**
     * @brief 拷贝栈顶 traceId 到 buffer
     * @param buffer 输出缓冲区
     * @param bufferSize buffer 容量
     * @return 写入的字符数（不含终止符）；无活跃 trace 时返回 0 且 buffer[0] = '\0'
     */
    LOG_API size_t currentTraceId(char* buffer, size_t bufferSize);

    /**
     * @brief 优先拷贝 explicitId，否则回退到 currentTraceId
     * @param explicitId 非空时使用之；null 或空串时回退栈顶 trace
     * @return 写入的字符数（不含终止符）；无结果时返回 0 且 buffer[0] = '\0'
     */
    LOG_API size_t resolveTraceId(const char* explicitId, char* buffer, size_t bufferSize);

    /** @brief 便捷包装（header 内联）：栈顶 traceId；无活跃 trace 时返回空串 */
    inline std::string currentTraceIdString()
    {
        char buffer[128];
        currentTraceId(buffer, sizeof(buffer));
        return std::string(buffer);
    }

    /** @brief 便捷包装（header 内联）：优先 explicitId，否则回退栈顶 trace */
    inline std::string resolveTraceIdString(const char* explicitId = nullptr)
    {
        char buffer[128];
        resolveTraceId(explicitId, buffer, sizeof(buffer));
        return std::string(buffer);
    }

    /** @brief HTTP 头名称，供 Network 模块统一使用 */
    inline constexpr const char* kTraceHeaderName = "X-Trace-Id";
}  // namespace SyTrace
