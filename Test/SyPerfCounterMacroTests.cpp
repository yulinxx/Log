/**
 * @file SyPerfCounterMacroTests.cpp
 * @brief SY_PERF_* 宏展开正确性 + SyTrace 守卫回归
 *
 * 覆盖三个已修复的确定性缺陷：
 *  1. SY_PERF_SCOPE / SY_PERF_SCOPE_STATS 的 `_sy_perf_timer_##__LINE__`
 *     因 ## 阻止参数展开，同函数体内两个 scope 会撞成同一变量名 ——
 *     本文件能编译即修复成立（旧宏下这是编译错误）。
 *  2. resolveTraceId(explicitId, nullptr, 0)：旧实现 bufferSize-1 下溢 SIZE_MAX，
 *     memcpy 写穿 —— 现在必须返回 0 且不碰内存。
 */

// 本测试强制走启用分支：Release 下 SY_PERF_ENABLED=0 宏是 ((void)0)，
// 测不到粘贴缺陷。ScopedPerfTimer::reportSlow 在 Release 的 Log 库里
// 未编译（同受 SY_PERF_ENABLED 门控），这里补一个空实现满足链接。
#define SY_PERF_ENABLED 1

#include <gtest/gtest.h>

#include "Log/SyPerfCounter.h"
#include "Log/SyTraceContext.h"

#include <cstring>

// Release 的 Log 库未编译此符号（受 SY_PERF_ENABLED 门控），测试自补空实现。
// 必须定义在全局命名空间（类所在的作用域），不能放进匿名命名空间。
void ScopedPerfTimer::reportSlow(const char*) {}

// 能编译即通过 —— 这正是旧 `_sy_perf_timer_##__LINE__` 下的失败用例
TEST(SyPerfCounterMacroTest, TwoScopesAndStatsInSameFunctionCompile)
{
    SY_PERF_SCOPE("scope_a");
    {
        SY_PERF_SCOPE("scope_b");  // 旧宏：与上一行撞成同名变量，编译错误
        SY_PERF_SCOPE_STATS("scope_c", nullptr);
    }
    SUCCEED();
}

TEST(SyPerfCounterMacroTest, ScopesInDistinctFunctionsRunWithoutCrash)
{
    SY_PERF_SCOPE("per_function");
    SUCCEED();
}

// ==================== resolveTraceId 入参守卫 ====================

TEST(SyTraceContextResolveTest, NullBufferOrZeroSizeReturnsZero)
{
    char buf[16] = {};
    // 显式 ID + 空缓冲：旧实现 bufferSize-1 下溢后 memcpy(nullptr, huge)
    EXPECT_EQ(SyTrace::resolveTraceId("explicit", nullptr, 0), 0u);
    EXPECT_EQ(SyTrace::resolveTraceId("explicit", buf, 0), 0u);
    // 无显式 ID 时同样受守卫保护（走 currentTraceId 的既有守卫）
    EXPECT_EQ(SyTrace::resolveTraceId(nullptr, nullptr, 0), 0u);
    EXPECT_EQ(SyTrace::resolveTraceId("", buf, 0), 0u);
}

TEST(SyTraceContextResolveTest, ExplicitIdCopiedWithTruncation)
{
    char buf[16] = {};
    EXPECT_EQ(SyTrace::resolveTraceId("abc", buf, sizeof(buf)), 3u);
    EXPECT_STREQ(buf, "abc");

    // 超长显式 ID：截断到 bufferSize-1 并保证 null 终止
    // 变量名不用 small：Windows 头（winnt.h）里有 `#define small char`，
    // Unity Build 把它和本文件拼进同一翻译单元时会展开成 `char char[4]`。
    char tiny[4] = { 'x', 'x', 'x', 'x' };
    EXPECT_EQ(SyTrace::resolveTraceId("abcdefgh", tiny, sizeof(tiny)), 3u);
    EXPECT_STREQ(tiny, "abc");
}