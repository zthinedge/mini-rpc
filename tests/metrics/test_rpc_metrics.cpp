#include "minirpc/metrics/RpcMetrics.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace{

using minirpc::metrics::RpcMetrics;
using minirpc::protocol::StatusCode;

void TestCounters(){
    RpcMetrics metrics;

    metrics.RequestStarted();
    metrics.RequestStarted();
    metrics.RequestStarted();

    auto inflight=metrics.Snapshot();
    assert(inflight.total_requests==3);
    assert(inflight.inflight_requests==3);

    metrics.RequestFinished(
        StatusCode::Ok,
        std::chrono::microseconds(100)
    );
    metrics.RequestFinished(
        StatusCode::InternalError,
        std::chrono::microseconds(200)
    );
    metrics.RequestFinished(
        StatusCode::Timeout,
        std::chrono::microseconds(300)
    );
    metrics.RetryStarted();
    metrics.RetryStarted();

    auto snapshot=metrics.Snapshot();
    assert(snapshot.total_requests==3);
    assert(snapshot.successful_requests==1);
    assert(snapshot.failed_requests==2);
    assert(snapshot.timeout_requests==1);
    assert(snapshot.retries==2);
    assert(snapshot.inflight_requests==0);
    assert(snapshot.latency_samples==3);
    assert(snapshot.total_latency_us==600);
    assert(snapshot.max_latency_us==300);
    assert(snapshot.AverageLatencyMicros()==200.0);
}

void TestConnections(){
    RpcMetrics metrics;

    metrics.ConnectionOpened();
    metrics.ConnectionOpened();
    assert(metrics.Snapshot().active_connections==2);

    metrics.ConnectionClosed();
    assert(metrics.Snapshot().active_connections==1);

    metrics.ConnectionClosed();
    metrics.ConnectionClosed();
    assert(metrics.Snapshot().active_connections==0);
}

void TestPercentiles(){
    RpcMetrics metrics;
    const std::vector<std::uint64_t> latencies{
        50,
        200,
        400,
        800,
        2000,
        4000,
        8000,
        20000,
        40000,
        80000,
        200000,
        400000,
        800000,
        2000000,
        4000000,
        8000000,
        20000000,
        40000000,
        70000000
    };

    for(std::uint64_t latency:latencies){
        metrics.RequestStarted();
        metrics.RequestFinished(
            StatusCode::Ok,
            std::chrono::microseconds(latency)
        );
    }

    auto snapshot=metrics.Snapshot();
    assert(snapshot.latency_samples==latencies.size());
    assert(snapshot.p50_latency_us==100000);
    assert(snapshot.p95_latency_us==70000000);
    assert(snapshot.p99_latency_us==70000000);
    assert(snapshot.p50_latency_us<=snapshot.p95_latency_us);
    assert(snapshot.p95_latency_us<=snapshot.p99_latency_us);
}

void TestConcurrentUpdates(){
    RpcMetrics metrics;
    constexpr int kThreadCount=8;
    constexpr int kRequestsPerThread=1000;
    std::vector<std::thread> threads;

    for(int i=0;i<kThreadCount;++i){
        threads.emplace_back([&metrics](){
            for(int request=0;request<kRequestsPerThread;++request){
                metrics.RequestStarted();
                metrics.RequestFinished(
                    StatusCode::Ok,
                    std::chrono::microseconds(10)
                );
            }
        });
    }

    for(auto& thread:threads){
        thread.join();
    }

    auto snapshot=metrics.Snapshot();
    std::uint64_t expected=
        static_cast<std::uint64_t>(kThreadCount)*
        kRequestsPerThread;
    assert(snapshot.total_requests==expected);
    assert(snapshot.successful_requests==expected);
    assert(snapshot.failed_requests==0);
    assert(snapshot.inflight_requests==0);
    assert(snapshot.latency_samples==expected);
}

}

int main(){
    TestCounters();
    TestConnections();
    TestPercentiles();
    TestConcurrentUpdates();

    std::cout<<"rpc metrics tests passed\n";
}
