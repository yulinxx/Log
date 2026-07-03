#include "Log/SyTraceContext.h"

#include <vector>

namespace SyTrace
{
    namespace
    {
        thread_local std::vector<std::string> g_traceStack;
    }

    void pushTraceId(const std::string& traceId)
    {
        if (traceId.empty())
            return;
        g_traceStack.push_back(traceId);
    }

    void popTraceId()
    {
        if (!g_traceStack.empty())
            g_traceStack.pop_back();
    }

    std::string currentTraceId()
    {
        return g_traceStack.empty() ? std::string{} : g_traceStack.back();
    }

    std::string resolveTraceId(const std::string& explicitId)
    {
        if (!explicitId.empty())
            return explicitId;
        return currentTraceId();
    }
} // namespace SyTrace