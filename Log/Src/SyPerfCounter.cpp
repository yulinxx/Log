/**
 * @file SyPerfCounter.cpp
 * @brief 性能计数器实现（ScopedPerfTimer::reportSlow）
 */

#include "Log/SyPerfCounter.h"
#include "Log/SyLogger.h"

#include <cstdio>

#if SY_PERF_ENABLED
void ScopedPerfTimer::reportSlow(const char* msg)
{
    SyLogger::GetInstance().WarnStr(msg);
}
#endif