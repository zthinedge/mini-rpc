#pragma once

#include "minirpc/protocol/RpcMeta.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace minirpc::metrics{

struct RpcMetricsSnapshot{
    std::uint64_t total_requests=0;
    std::uint64_t successful_requests=0;
    // 所有非 Ok 请求，包含 timeout_requests。
    std::uint64_t failed_requests=0;
    std::uint64_t timeout_requests=0;
    std::uint64_t retries=0;
    std::int64_t inflight_requests=0;
    std::int64_t active_connections=0;

    // 延迟单位为微秒，分位数是固定直方图桶的近似上界。
    std::uint64_t latency_samples=0;
    std::uint64_t total_latency_us=0;
    std::uint64_t max_latency_us=0;
    std::uint64_t p50_latency_us=0;
    std::uint64_t p95_latency_us=0;
    std::uint64_t p99_latency_us=0;

    double AverageLatencyMicros()const noexcept;
};

class RpcMetrics{
public:
    using Clock=std::chrono::steady_clock;
    using TimePoint=Clock::time_point;

    void RequestStarted()noexcept;

    void RequestFinished(
        protocol::StatusCode status,
        std::chrono::microseconds latency
    )noexcept;

    void RequestFinished(
        protocol::StatusCode status,
        TimePoint started_at
    )noexcept;

    void RetryStarted()noexcept;
    void ConnectionOpened()noexcept;
    void ConnectionClosed()noexcept;

    RpcMetricsSnapshot Snapshot()const noexcept;

private:
    static constexpr std::size_t kBucketCount=19;

    std::atomic_uint64_t total_requests_{0};
    std::atomic_uint64_t successful_requests_{0};
    std::atomic_uint64_t failed_requests_{0};
    std::atomic_uint64_t timeout_requests_{0};
    std::atomic_uint64_t retries_{0};
    std::atomic_int64_t inflight_requests_{0};
    std::atomic_int64_t active_connections_{0};

    std::atomic_uint64_t latency_samples_{0};
    std::atomic_uint64_t total_latency_us_{0};
    std::atomic_uint64_t max_latency_us_{0};
    std::array<std::atomic_uint64_t,kBucketCount> latency_buckets_{};
};

}
