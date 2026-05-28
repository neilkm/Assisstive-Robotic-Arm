#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Micros = std::chrono::microseconds;

inline int64_t usNow()
{
    return std::chrono::duration_cast<Micros>(Clock::now().time_since_epoch()).count();
}

struct TestStats {
    int sent       = 0;
    int received   = 0;
    int timeouts   = 0;
    int corrupt    = 0;
    int duplicates = 0;

    std::vector<int64_t> latencies_us;

    void recordLatency(int64_t us) { latencies_us.push_back(us); }

    int64_t minUs() const
    {
        if (latencies_us.empty()) return 0;
        return *std::min_element(latencies_us.begin(), latencies_us.end());
    }

    int64_t maxUs() const
    {
        if (latencies_us.empty()) return 0;
        return *std::max_element(latencies_us.begin(), latencies_us.end());
    }

    int64_t meanUs() const
    {
        if (latencies_us.empty()) return 0;
        const int64_t sum = std::accumulate(latencies_us.begin(), latencies_us.end(), int64_t{0});
        return sum / static_cast<int64_t>(latencies_us.size());
    }

    int64_t percentileUs(double p) const
    {
        if (latencies_us.empty()) return 0;
        std::vector<int64_t> sorted = latencies_us;
        std::sort(sorted.begin(), sorted.end());
        const size_t idx = static_cast<size_t>(p / 100.0 * static_cast<double>(sorted.size() - 1));
        return sorted[idx];
    }

    float packetLossPct() const
    {
        if (sent == 0) return 0.0f;
        return 100.0f * static_cast<float>(sent - received) / static_cast<float>(sent);
    }

    void print(const char* label) const
    {
        printf("\n");
        printf("  %-20s | %6d\n", "Sent",      sent);
        printf("  %-20s | %6d\n", "Received",  received);
        printf("  %-20s | %6d\n", "Timeouts",  timeouts);
        printf("  %-20s | %6d\n", "Corrupt",   corrupt);
        printf("  %-20s | %6d\n", "Duplicates",duplicates);
        printf("  %-20s | %5.1f%%\n", "Packet loss",  packetLossPct());
        if (!latencies_us.empty()) {
            printf("  %-20s | %6lld us\n", "Latency min",  (long long)minUs());
            printf("  %-20s | %6lld us\n", "Latency mean", (long long)meanUs());
            printf("  %-20s | %6lld us\n", "Latency p95",  (long long)percentileUs(95));
            printf("  %-20s | %6lld us\n", "Latency max",  (long long)maxUs());
        }
        (void)label;
    }
};

// ─── Print helpers ────────────────────────────────────────────────────────────

inline void printBanner(const char* title)
{
    constexpr int W = 68;
    printf("\n");
    for (int i = 0; i < W; ++i) printf("=");
    printf("\n");
    const int pad = (W - static_cast<int>(__builtin_strlen(title))) / 2;
    for (int i = 0; i < pad - 1; ++i) printf(" ");
    printf("  %s\n", title);
    for (int i = 0; i < W; ++i) printf("=");
    printf("\n");
}

inline void printSection(const char* title)
{
    constexpr int W = 68;
    printf("\n");
    for (int i = 0; i < W; ++i) printf("-");
    printf("\n  %s\n", title);
    for (int i = 0; i < W; ++i) printf("-");
    printf("\n");
}

inline void printProgress(int current, int total, const char* label = "")
{
    constexpr int BAR_W = 30;
    const int filled = (total > 0) ? (current * BAR_W / total) : 0;
    printf("\r  [");
    for (int i = 0; i < BAR_W; ++i) printf(i < filled ? "#" : ".");
    printf("] %3d/%3d  %s    ", current, total, label);
    fflush(stdout);
}

inline void printResult(const char* label, bool passed)
{
    printf("\n  [ %s ] %s\n", passed ? "PASS" : "FAIL", label);
}

inline void printSummaryHeader()
{
    printf("\n\n");
    printf("  %-40s  %s\n", "Test", "Result");
    for (int i = 0; i < 52; ++i) printf("-");
    printf("\n");
}

inline void printSummaryRow(const char* label, bool passed)
{
    printf("  %-40s  [ %s ]\n", label, passed ? "PASS" : "FAIL");
}
