#include "minirpc/metrics/RpcMetrics.h"

#include <algorithm>
#include <limits>

namespace minirpc::metrics{
namespace{

constexpr std::array<std::uint64_t,19> kLatencyUpperBoundsUs{
    100,
    250,
    500,
    1000,
    2500,
    5000,
    10000,
    25000,
    50000,
    100000,
    250000,
    500000,
    1000000,
    2500000,
    5000000,
    10000000,
    30000000,
    60000000,
    std::numeric_limits<std::uint64_t>::max()
};

void DecrementGauge(std::atomic_int64_t* gauge)noexcept{
    std::int64_t current=gauge->load(std::memory_order_relaxed);

    while(current>0&&!gauge->compare_exchange_weak(
        current,
        current-1,
        std::memory_order_relaxed
    )){}
}

std::uint64_t Percentile(
    const std::array<std::uint64_t,19>& buckets,
    std::uint64_t sample_count,
    std::uint64_t max_latency,
    std::uint64_t numerator
)noexcept{
    if(sample_count==0){
        return 0;
    }

    std::uint64_t rank=
        (sample_count/100)*numerator+
        ((sample_count%100)*numerator+99)/100;
    std::uint64_t cumulative=0;

    for(std::size_t i=0;i<buckets.size();++i){
        cumulative+=buckets[i];

        if(cumulative>=rank){
            if(kLatencyUpperBoundsUs[i]==
               std::numeric_limits<std::uint64_t>::max()){
                return max_latency;
            }

            return kLatencyUpperBoundsUs[i];
        }
    }

    return max_latency;
}

}

double RpcMetricsSnapshot::AverageLatencyMicros()const noexcept{
    if(latency_samples==0){
        return 0.0;
    }

    return static_cast<double>(total_latency_us)/
           static_cast<double>(latency_samples);
}

void RpcMetrics::RequestStarted()noexcept{
    total_requests_.fetch_add(1,std::memory_order_relaxed);
    inflight_requests_.fetch_add(1,std::memory_order_relaxed);
}

void RpcMetrics::RequestFinished(
    protocol::StatusCode status,
    std::chrono::microseconds latency
)noexcept{
    DecrementGauge(&inflight_requests_);

    if(status==protocol::StatusCode::Ok){
        successful_requests_.fetch_add(1,std::memory_order_relaxed);
    }else{
        failed_requests_.fetch_add(1,std::memory_order_relaxed);

        if(status==protocol::StatusCode::Timeout){
            timeout_requests_.fetch_add(1,std::memory_order_relaxed);
        }
    }

    std::uint64_t latency_us=latency.count()>0
        ?static_cast<std::uint64_t>(latency.count())
        :0;

    latency_samples_.fetch_add(1,std::memory_order_relaxed);
    total_latency_us_.fetch_add(latency_us,std::memory_order_relaxed);

    std::uint64_t current=max_latency_us_.load(std::memory_order_relaxed);
    while(current<latency_us&&!max_latency_us_.compare_exchange_weak(
        current,
        latency_us,
        std::memory_order_relaxed
    )){}

    auto bucket=std::lower_bound(
        kLatencyUpperBoundsUs.begin(),
        kLatencyUpperBoundsUs.end(),
        latency_us
    );
    std::size_t index=static_cast<std::size_t>(
        bucket-kLatencyUpperBoundsUs.begin()
    );
    latency_buckets_[index].fetch_add(1,std::memory_order_relaxed);
}

void RpcMetrics::RequestFinished(
    protocol::StatusCode status,
    TimePoint started_at
)noexcept{
    RequestFinished(
        status,
        std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now()-started_at
        )
    );
}

void RpcMetrics::RetryStarted()noexcept{
    retries_.fetch_add(1,std::memory_order_relaxed);
}

void RpcMetrics::ConnectionOpened()noexcept{
    active_connections_.fetch_add(1,std::memory_order_relaxed);
}

void RpcMetrics::ConnectionClosed()noexcept{
    DecrementGauge(&active_connections_);
}

RpcMetricsSnapshot RpcMetrics::Snapshot()const noexcept{
    RpcMetricsSnapshot snapshot;
    snapshot.total_requests=
        total_requests_.load(std::memory_order_relaxed);
    snapshot.successful_requests=
        successful_requests_.load(std::memory_order_relaxed);
    snapshot.failed_requests=
        failed_requests_.load(std::memory_order_relaxed);
    snapshot.timeout_requests=
        timeout_requests_.load(std::memory_order_relaxed);
    snapshot.retries=retries_.load(std::memory_order_relaxed);
    snapshot.inflight_requests=
        inflight_requests_.load(std::memory_order_relaxed);
    snapshot.active_connections=
        active_connections_.load(std::memory_order_relaxed);
    snapshot.latency_samples=
        latency_samples_.load(std::memory_order_relaxed);
    snapshot.total_latency_us=
        total_latency_us_.load(std::memory_order_relaxed);
    snapshot.max_latency_us=
        max_latency_us_.load(std::memory_order_relaxed);

    std::array<std::uint64_t,kBucketCount> buckets;
    for(std::size_t i=0;i<kBucketCount;++i){
        buckets[i]=latency_buckets_[i].load(std::memory_order_relaxed);
    }

    snapshot.p50_latency_us=Percentile(
        buckets,
        snapshot.latency_samples,
        snapshot.max_latency_us,
        50
    );
    snapshot.p95_latency_us=Percentile(
        buckets,
        snapshot.latency_samples,
        snapshot.max_latency_us,
        95
    );
    snapshot.p99_latency_us=Percentile(
        buckets,
        snapshot.latency_samples,
        snapshot.max_latency_us,
        99
    );

    return snapshot;
}

}
