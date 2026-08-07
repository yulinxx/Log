#include "Log/SyTraceContext.h"

#include <cstring>
#include <vector>

namespace SyTrace
{
    namespace
    {
        thread_local std::vector<std::string> g_traceStack;
    }

    void pushTraceId(const char* traceId)
    {
        if (!traceId || !*traceId)
            return;
        g_traceStack.emplace_back(traceId);
    }

    void popTraceId()
    {
        if (!g_traceStack.empty())
            g_traceStack.pop_back();
    }

    size_t currentTraceId(char* buffer, size_t bufferSize)
    {
        if (!buffer || bufferSize == 0)
            return 0;
        if (g_traceStack.empty())
        {
            buffer[0] = '\0';
            return 0;
        }

        const std::string& id = g_traceStack.back();
        const size_t copyLen = (id.size() < bufferSize) ? id.size() : bufferSize - 1;
        std::memcpy(buffer, id.data(), copyLen);
        buffer[copyLen] = '\0';
        return copyLen;
    }

    size_t resolveTraceId(const char* explicitId, char* buffer, size_t bufferSize)
    {
        if (explicitId && *explicitId)
        {
            const size_t len = std::strlen(explicitId);
            const size_t copyLen = (len < bufferSize) ? len : bufferSize - 1;
            std::memcpy(buffer, explicitId, copyLen);
            buffer[copyLen] = '\0';
            return copyLen;
        }
        return currentTraceId(buffer, bufferSize);
    }
} // namespace SyTrace