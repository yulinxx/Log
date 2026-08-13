#pragma once

/**
 * @file SyPerfCounter.h
 * @brief 轻量级性能计数器与监控基础设施
 *
 * 提供：
 * - 帧耗时追踪（FrameTimer）
 * - 操作耗时 RAII 作用域计时（ScopedPerfTimer）
 * - 累计统计（PerfStats）
 * - 定期自动上报（与 SyLogger 集成）
 *
 * 设计要点：
 *  - 零虚函数，纯 POD 统计结构，跨 DLL 安全
 *  - 编译期可通过 SY_PERF_ENABLED 宏整体关闭
 *  - 所有时钟使用 std::chrono::high_resolution_clock
 */

#include "LogAPI.h"
#include "Log/SyLogger.h"
#include <chrono>
#include <cstdint>
#include <cstdio>

#ifndef SY_PERF_ENABLED
    #ifdef NDEBUG
        #define SY_PERF_ENABLED 0
    #else
        #define SY_PERF_ENABLED 1
    #endif
#endif

// ============================================================================
// 性能统计数据结构（POD，跨 DLL 安全）
// ============================================================================

struct PerfStats
{
    const char* name;    // 统计项名称
    uint64_t callCount;  // 调用次数
    double totalMs;      // 累计耗时 (ms)
    double minMs;        // 最短耗时 (ms)
    double maxMs;        // 最长耗时 (ms)
    double avgMs;        // 平均耗时 (ms)
};

// ============================================================================
// 帧耗时追踪器
// ============================================================================

class FrameTimer
{
public:
    FrameTimer()
        : m_frameCount(0)
        , m_totalMs(0.0)
        , m_minMs(1e9)
        , m_maxMs(0.0)
        , m_lastReportFrame(0)
    {
    }

    /// 开始新帧计时，应在每帧开始时调用
    void beginFrame()
    {
        m_frameStart = std::chrono::high_resolution_clock::now();
    }

    /// 结束当前帧计时，应在每帧渲染完成后调用
    void endFrame()
    {
        auto now = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(now - m_frameStart).count();

        ++m_frameCount;
        m_totalMs += ms;
        if (ms < m_minMs)
        {
            m_minMs = ms;
        }
        if (ms > m_maxMs)
        {
            m_maxMs = ms;
        }

        // 每 300 帧自动上报一次
        if (m_frameCount - m_lastReportFrame >= 300)
        {
            report();
            m_lastReportFrame = m_frameCount;
        }
    }

    /// 获取当前帧耗时 (ms)
    double currentFrameMs() const
    {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - m_frameStart).count();
    }

    /// 获取平均帧耗时 (ms)
    double avgFrameMs() const
    {
        return m_frameCount > 0 ? m_totalMs / m_frameCount : 0.0;
    }

    /// 获取帧数
    uint64_t frameCount() const
    {
        return m_frameCount;
    }

    /// 手动上报统计
    void report()
    {
        if (m_frameCount == 0)
        {
            return;
        }

        char buf[256];
        snprintf(buf,
            sizeof(buf),
            "[FrameTimer] frames=%llu avg=%.2fms min=%.2fms max=%.2fms",
            static_cast<unsigned long long>(m_frameCount),
            avgFrameMs(),
            m_minMs,
            m_maxMs);
        SyLogger::GetInstance().InfoStr(buf);
    }

    /// 重置所有累计数据
    void reset()
    {
        m_frameCount = 0;
        m_totalMs = 0.0;
        m_minMs = 1e9;
        m_maxMs = 0.0;
        m_lastReportFrame = 0;
    }

private:
    std::chrono::high_resolution_clock::time_point m_frameStart;
    uint64_t m_frameCount;
    double m_totalMs;
    double m_minMs;
    double m_maxMs;
    uint64_t m_lastReportFrame;
};

// ============================================================================
// 操作耗时 RAII 作用域计时器
// ============================================================================

#if SY_PERF_ENABLED

class ScopedPerfTimer
{
public:
    /// @param name   统计项名称（静态字符串，生命周期需长于本对象）
    /// @param stats  累计统计结构体指针
    explicit ScopedPerfTimer(const char* name, PerfStats* stats = nullptr)
        : m_name(name)
        , m_stats(stats)
        , m_start(std::chrono::high_resolution_clock::now())
    {
    }

    ~ScopedPerfTimer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - m_start).count();

        if (m_stats)
        {
            ++m_stats->callCount;
            m_stats->totalMs += ms;
            if (ms < m_stats->minMs || m_stats->callCount == 1)
            {
                m_stats->minMs = ms;
            }
            if (ms > m_stats->maxMs)
            {
                m_stats->maxMs = ms;
            }
            m_stats->avgMs = m_stats->totalMs / m_stats->callCount;
        }

        // 超过阈值时输出警告
        if (ms > 16.0)
        {
            char buf[256];
            snprintf(buf, sizeof(buf), "[PERF] %s: %.2f ms (slow frame >16ms)", m_name, ms);
            reportSlow(buf);
        }
    }

    ScopedPerfTimer(const ScopedPerfTimer&) = delete;
    ScopedPerfTimer& operator=(const ScopedPerfTimer&) = delete;

private:
    const char* m_name;
    PerfStats* m_stats;
    std::chrono::high_resolution_clock::time_point m_start;

    static void reportSlow(const char* msg);
};

    /// 便捷宏：统计指定代码块的耗时
    #define SY_PERF_SCOPE(name) ScopedPerfTimer _sy_perf_timer_##__LINE__(name)

    /// 便捷宏：统计指定代码块的耗时并写入累计统计
    #define SY_PERF_SCOPE_STATS(name, statsPtr) ScopedPerfTimer _sy_perf_timer_##__LINE__(name, statsPtr)

#else  // !SY_PERF_ENABLED

class ScopedPerfTimer
{
public:
    explicit ScopedPerfTimer(const char*, PerfStats* = nullptr) {}
};

    #define SY_PERF_SCOPE(name)              ((void)0)
    #define SY_PERF_SCOPE_STATS(name, stats) ((void)0)

#endif  // SY_PERF_ENABLED

// ============================================================================
// 操作耗时宏（简化版，直接输出日志）
// ============================================================================

#if SY_PERF_ENABLED

    /// 测量一个代码块的耗时，超过阈值时输出 SY_WARNF
    /// 用法: SY_PERF_BLOCK("render", 16.0) { ... }
    #define SY_PERF_BLOCK(name, thresholdMs)                                                                     \
        for (struct {                                                                                            \
                 const char* _n;                                                                                 \
                 double _t;                                                                                      \
                 bool _done;                                                                                     \
                 std::chrono::high_resolution_clock::time_point _s;                                              \
                 ~decltype (*this)()                                                                             \
                 {                                                                                               \
                     if (_done)                                                                                  \
                         return;                                                                                 \
                     auto _e = std::chrono::high_resolution_clock::now();                                        \
                     double _ms = std::chrono::duration<double, std::milli>(_e - _s).count();                    \
                     if (_ms > (_t))                                                                             \
                     {                                                                                           \
                         char _buf[256];                                                                         \
                         snprintf(_buf, sizeof(_buf), "[PERF] %s: %.2f ms (threshold %.1f ms)", _n, _ms, (_t));  \
                         SyLogger::GetInstance().WarnStr(_buf);                                                  \
                     }                                                                                           \
                 }                                                                                               \
             } _sy_perf_block_##__LINE__{ name, thresholdMs, false, std::chrono::high_resolution_clock::now() }; \
            !_sy_perf_block_##__LINE__._done;                                                                    \
            _sy_perf_block_##__LINE__._done = true)

#else

    #define SY_PERF_BLOCK(name, thresholdMs) \
        if (true)                            \
        {                                    \
        }                                    \
        else

#endif  // SY_PERF_ENABLED

// ============================================================================
// 性能统计上报辅助函数
// ============================================================================

/// 格式化 PerfStats 为字符串
/// @param buf  输出缓冲区
/// @param size 缓冲区大小
/// @param stats 统计数据
/// @return 写入的字符数（不含 null 终止符）
inline int perfStatsToString(char* buf, size_t size, const PerfStats& stats)
{
    return snprintf(buf,
        size,
        "[%s] calls=%llu avg=%.2fms min=%.2fms max=%.2fms total=%.2fms",
        stats.name ? stats.name : "?",
        static_cast<unsigned long long>(stats.callCount),
        stats.avgMs,
        stats.minMs,
        stats.maxMs,
        stats.totalMs);
}