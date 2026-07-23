#include "minirpc/cluster/ConnectionPool.h"
#include "minirpc/cluster/Endpoint.h"
#include "minirpc/net/EventLoop.h"
#include "minirpc/protocol/RpcMessage.h"
#include "minirpc/protocol/RpcMeta.h"
#include "minirpc/rpc/CallOptions.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace{

using Clock=std::chrono::steady_clock;

constexpr const char* kBenchService="BenchService";
constexpr const char* kEchoMethod="Echo";

struct BenchOptions{
    std::string host="127.0.0.1";
    std::uint16_t port=9000;
    std::size_t concurrency=1;
    std::size_t requests=1000;
    std::size_t payload_size=64;
};

std::size_t ParseSize(
    const std::string& text,
    const std::string& option
){
    if(text.empty()||text.front()=='-'){
        throw std::invalid_argument(
            option+" requires a non-negative integer"
        );
    }

    std::size_t parsed=0;
    unsigned long long value=0;

    try{
        value=std::stoull(text,&parsed);
    }catch(const std::exception&){
        throw std::invalid_argument(
            option+" requires a non-negative integer"
        );
    }

    if(parsed!=text.size()||
       value>std::numeric_limits<std::size_t>::max()){
        throw std::invalid_argument(
            option+" requires a valid integer"
        );
    }

    return static_cast<std::size_t>(value);
}

std::uint16_t ParsePort(const std::string& text){
    std::size_t value=ParseSize(text,"--port");
    if(value==0||value>65535){
        throw std::invalid_argument(
            "--port must be between 1 and 65535"
        );
    }
    return static_cast<std::uint16_t>(value);
}

void PrintUsage(const char* program){
    std::cout
        <<"Usage: "<<program<<" [options]\n"
        <<"  --host HOST          server host (default 127.0.0.1)\n"
        <<"  --port PORT          server port (default 9000)\n"
        <<"  --concurrency N      concurrent calls (default 1)\n"
        <<"  --requests N         total calls (default 1000)\n"
        <<"  --payload N          payload bytes (default 64)\n"
        <<"  --help               show this help\n";
}

BenchOptions ParseArguments(int argc,char* argv[]){
    BenchOptions options;

    for(int index=1;index<argc;++index){
        std::string argument=argv[index];
        if(argument=="--help"){
            PrintUsage(argv[0]);
            std::exit(0);
        }

        if(index+1>=argc){
            throw std::invalid_argument(
                "missing value for "+argument
            );
        }

        std::string value=argv[++index];
        if(argument=="--host"){
            options.host=std::move(value);
        }else if(argument=="--port"){
            options.port=ParsePort(value);
        }else if(argument=="--concurrency"){
            options.concurrency=ParseSize(value,argument);
        }else if(argument=="--requests"){
            options.requests=ParseSize(value,argument);
        }else if(argument=="--payload"){
            options.payload_size=ParseSize(value,argument);
        }else{
            throw std::invalid_argument(
                "unknown option: "+argument
            );
        }
    }

    if(options.host.empty()){
        throw std::invalid_argument("--host must not be empty");
    }
    if(options.concurrency==0){
        throw std::invalid_argument(
            "--concurrency must be positive"
        );
    }
    if(options.requests==0){
        throw std::invalid_argument("--requests must be positive");
    }
    if(options.concurrency>10000){
        throw std::invalid_argument(
            "--concurrency must not exceed 10000"
        );
    }
    if(options.payload_size>64*1024*1024){
        throw std::invalid_argument(
            "--payload must not exceed 64 MiB"
        );
    }

    return options;
}

class LoopThread{
public:
    LoopThread(){
        std::promise<minirpc::net::EventLoop*> ready;
        auto future=ready.get_future();

        thread_=std::thread([ready=std::move(ready)]()mutable{
            minirpc::net::EventLoop loop;
            ready.set_value(&loop);
            loop.Loop();
        });
        loop_=future.get();
    }

    ~LoopThread(){
        if(thread_.joinable()){
            loop_->Stop();
            thread_.join();
        }
    }

    LoopThread(const LoopThread&)=delete;
    LoopThread& operator=(const LoopThread&)=delete;

    minirpc::net::EventLoop* Loop()const noexcept{
        return loop_;
    }

    void Drain(){
        std::promise<void> done;
        auto future=done.get_future();
        loop_->RunInLoop([&done](){
            done.set_value();
        });
        future.get();
    }

private:
    minirpc::net::EventLoop* loop_=nullptr;
    std::thread thread_;
};

std::uint64_t Percentile(
    const std::vector<std::uint64_t>& sorted,
    std::size_t percent
){
    if(sorted.empty()){
        return 0;
    }

    std::size_t rank=
        (sorted.size()/100)*percent+
        ((sorted.size()%100)*percent+99)/100;
    return sorted[rank-1];
}

struct BenchResult{
    std::size_t succeeded=0;
    std::size_t failed=0;
    std::chrono::microseconds elapsed{0};
    std::uint64_t average_latency_us=0;
    std::uint64_t min_latency_us=0;
    std::uint64_t max_latency_us=0;
    std::uint64_t p50_latency_us=0;
    std::uint64_t p95_latency_us=0;
    std::uint64_t p99_latency_us=0;
    std::string first_error;
};

BenchResult RunBenchmark(
    const BenchOptions& options,
    minirpc::cluster::ConnectionPool* pool
){
    std::string payload(options.payload_size,'x');
    std::vector<std::uint64_t> latencies(options.requests);
    std::atomic_size_t next_request{0};
    std::atomic_size_t succeeded{0};
    std::atomic_size_t failed{0};

    std::mutex start_mutex;
    std::condition_variable start_ready;
    bool start=false;

    std::mutex error_mutex;
    std::string first_error;

    std::size_t worker_count=std::min(
        options.concurrency,
        options.requests
    );
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    minirpc::rpc::CallOptions call_options;
    call_options.idempotent=true;
    call_options.timeout=std::chrono::seconds(5);

    for(std::size_t worker=0;worker<worker_count;++worker){
        workers.emplace_back([&](){
            {
                std::unique_lock<std::mutex> lock(start_mutex);
                start_ready.wait(lock,[&start](){
                    return start;
                });
            }

            while(true){
                std::size_t request_index=
                    next_request.fetch_add(1,std::memory_order_relaxed);
                if(request_index>=options.requests){
                    return;
                }

                auto request_started=Clock::now();

                try{
                    minirpc::protocol::RpcMessage response=pool->Call(
                        kBenchService,
                        kEchoMethod,
                        payload,
                        call_options
                    );

                    latencies[request_index]=
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::microseconds
                            >(Clock::now()-request_started).count()
                        );

                    if(response.meta.status_code==
                           minirpc::protocol::StatusCode::Ok&&
                       response.payload==payload){
                        succeeded.fetch_add(1,std::memory_order_relaxed);
                    }else{
                        failed.fetch_add(1,std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if(first_error.empty()){
                            first_error=response.meta.error_text.empty()
                                ?"invalid echo response"
                                :response.meta.error_text;
                        }
                    }
                }catch(const std::exception& error){
                    latencies[request_index]=
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::microseconds
                            >(Clock::now()-request_started).count()
                        );
                    failed.fetch_add(1,std::memory_order_relaxed);

                    std::lock_guard<std::mutex> lock(error_mutex);
                    if(first_error.empty()){
                        first_error=error.what();
                    }
                }
            }
        });
    }

    auto benchmark_started=Clock::now();
    {
        std::lock_guard<std::mutex> lock(start_mutex);
        start=true;
    }
    start_ready.notify_all();

    for(auto& worker:workers){
        worker.join();
    }
    auto benchmark_finished=Clock::now();

    std::sort(latencies.begin(),latencies.end());
    std::uint64_t total_latency=0;
    for(std::uint64_t latency:latencies){
        total_latency+=latency;
    }

    BenchResult result;
    result.succeeded=succeeded.load(std::memory_order_relaxed);
    result.failed=failed.load(std::memory_order_relaxed);
    result.elapsed=std::chrono::duration_cast<std::chrono::microseconds>(
        benchmark_finished-benchmark_started
    );
    result.average_latency_us=
        total_latency/static_cast<std::uint64_t>(latencies.size());
    result.min_latency_us=latencies.front();
    result.max_latency_us=latencies.back();
    result.p50_latency_us=Percentile(latencies,50);
    result.p95_latency_us=Percentile(latencies,95);
    result.p99_latency_us=Percentile(latencies,99);
    result.first_error=std::move(first_error);
    return result;
}

void PrintResult(
    const BenchOptions& options,
    std::size_t connections,
    const BenchResult& result
){
    double elapsed_seconds=
        static_cast<double>(result.elapsed.count())/1000000.0;
    double qps=elapsed_seconds>0.0
        ?static_cast<double>(options.requests)/elapsed_seconds
        :0.0;

    std::cout<<std::fixed<<std::setprecision(2)
             <<"\nrpc_bench result\n"
             <<"endpoint: "<<options.host<<':'<<options.port<<'\n'
             <<"concurrency: "<<options.concurrency<<'\n'
             <<"connections: "<<connections<<'\n'
             <<"requests: "<<options.requests<<'\n'
             <<"payload bytes: "<<options.payload_size<<'\n'
             <<"succeeded: "<<result.succeeded<<'\n'
             <<"failed: "<<result.failed<<'\n'
             <<"elapsed(s): "<<elapsed_seconds<<'\n'
             <<"QPS: "<<qps<<'\n'
             <<"latency(us) avg/min/max: "
             <<result.average_latency_us<<'/'
             <<result.min_latency_us<<'/'
             <<result.max_latency_us<<'\n'
             <<"latency(us) P50/P95/P99: "
             <<result.p50_latency_us<<'/'
             <<result.p95_latency_us<<'/'
             <<result.p99_latency_us<<'\n';

    if(!result.first_error.empty()){
        std::cout<<"first error: "<<result.first_error<<'\n';
    }
}

}

int main(int argc,char* argv[]){
    try{
        BenchOptions options=ParseArguments(argc,argv);
        LoopThread loop_thread;

        minirpc::cluster::ConnectionPoolOptions pool_options;
        pool_options.max_connections=std::min<std::size_t>(
            options.concurrency,
            4
        );
        pool_options.idle_timeout=std::chrono::milliseconds::zero();

        auto pool=std::make_unique<minirpc::cluster::ConnectionPool>(
            loop_thread.Loop(),
            minirpc::cluster::Endpoint(options.host,options.port),
            pool_options
        );

        BenchResult result=RunBenchmark(options,pool.get());
        PrintResult(
            options,
            pool_options.max_connections,
            result
        );

        pool.reset();
        loop_thread.Drain();
        return result.failed==0?0:1;
    }catch(const std::exception& error){
        std::cerr<<"rpc_bench error: "<<error.what()<<'\n';
        PrintUsage(argv[0]);
        return 1;
    }
}
