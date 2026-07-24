#include "minirpc/cluster/ChannelManager.h"
#include "minirpc/cluster/RoundRobin.h"
#include "minirpc/net/EventLoop.h"
#include "minirpc/protocol/RpcMessage.h"
#include "minirpc/registry/ZooKeeperClient.h"
#include "minirpc/registry/ZooKeeperDiscovery.h"
#include "minirpc/rpc/CallOptions.h"

#include <chrono>
#include <cstddef>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

using namespace minirpc;

namespace{

constexpr const char* kServiceName="RegistryDemoService";
constexpr const char* kMethodName="WhoAmI";

struct Arguments{
    std::string zookeeper_servers="127.0.0.1:2181";
    std::size_t requests=40;
    std::chrono::milliseconds interval{500};
};

std::size_t ParseSize(const std::string& value,const char* name){
    if(value.empty()||value.front()=='-'){
        throw std::invalid_argument(
            std::string(name)+" must be positive"
        );
    }

    std::size_t parsed=0;
    unsigned long long result=std::stoull(value,&parsed);
    if(parsed!=value.size()||result==0){
        throw std::invalid_argument(
            std::string(name)+" must be positive"
        );
    }
    return static_cast<std::size_t>(result);
}

Arguments ParseArguments(int argc,char* argv[]){
    Arguments arguments;
    if(argc>1){
        arguments.zookeeper_servers=argv[1];
    }
    if(argc>2){
        arguments.requests=ParseSize(argv[2],"requests");
    }
    if(argc>3){
        arguments.interval=std::chrono::milliseconds(
            ParseSize(argv[3],"interval")
        );
    }
    return arguments;
}

class LoopThread{
public:
    LoopThread(){
        std::promise<net::EventLoop*> ready;
        auto future=ready.get_future();

        thread_=std::thread([ready=std::move(ready)]()mutable{
            net::EventLoop loop;
            ready.set_value(&loop);
            loop.Loop();
        });
        loop_=future.get();
    }

    ~LoopThread(){
        loop_->Stop();
        thread_.join();
    }

    LoopThread(const LoopThread&)=delete;
    LoopThread& operator=(const LoopThread&)=delete;

    net::EventLoop* Loop()const noexcept{
        return loop_;
    }

private:
    net::EventLoop* loop_=nullptr;
    std::thread thread_;
};

}

int main(int argc,char* argv[]){
    try{
        Arguments arguments=ParseArguments(argc,argv);

        registry::ZooKeeperClientOptions zk_options;
        zk_options.servers=arguments.zookeeper_servers;
        zk_options.session_timeout=std::chrono::seconds(5);

        auto zk_client=
            std::make_shared<registry::ZooKeeperClient>(zk_options);
        registry::ZooKeeperDiscovery discovery(zk_client);
        discovery.SetErrorCallback([](const std::string& error){
            std::cerr<<"discovery error: "<<error<<'\n';
        });

        zk_client->Start();
        if(!zk_client->WaitUntilConnected(std::chrono::seconds(10))){
            throw std::runtime_error("ZooKeeper connect timeout");
        }
        discovery.WatchService(kServiceName);

        LoopThread loop_thread;
        cluster::ChannelManager channels(loop_thread.Loop());
        cluster::RoundRobin round_robin;

        rpc::CallOptions call_options;
        call_options.idempotent=true;
        call_options.timeout=std::chrono::milliseconds(1200);

        for(std::size_t request=1;
            request<=arguments.requests;
            ++request){
            registry::DiscoveryResult discovered=
                discovery.Resolve(kServiceName);
            std::optional<cluster::Endpoint> endpoint=
                round_robin.Select(discovered.providers);

            if(!endpoint.has_value()){
                const char* state=
                    discovered.status==
                        registry::DiscoveryStatus::NoProvider
                    ?"NoProvider":"NotReady";
                std::cout<<"request="<<request
                         <<" discovery="<<state
                         <<std::endl;
            }else{
                auto pool=channels.GetOrCreate(*endpoint);
                protocol::RpcMessage response=pool->Call(
                    kServiceName,
                    kMethodName,
                    {},
                    call_options
                );

                std::cout<<"request="<<request
                         <<" selected="<<endpoint->ToString()
                         <<" providers="
                         <<discovered.providers->size();

                if(response.meta.status_code==
                   protocol::StatusCode::Ok){
                    std::cout<<" response="<<response.payload;
                }else{
                    std::cout<<" error="
                             <<response.meta.error_text;
                }
                std::cout<<std::endl;
            }

            std::this_thread::sleep_for(arguments.interval);
        }

        zk_client->Close();
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"registry client error: "
                 <<error.what()<<'\n';
        return 1;
    }
}
