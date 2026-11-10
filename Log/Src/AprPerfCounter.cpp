/**
 * @file AprPerfCounter.cpp
 * @brief 性能计数器实现（ScopedPerfTimer::reportSlow）
 */

#include "Log/AprPerfCounter.h"
#include "Log/AprLogger.h"

#include <cstdio>

#if APR_PERF_ENABLED
void ScopedPerfTimer::reportSlow(const char* msg)
{
    AprLogger::GetInstance().WarnStr(msg);
}
#endif