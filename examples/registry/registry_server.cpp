#include "minirpc/cluster/Endpoint.h"
#include "minirpc/net/EventLoop.h"
#include "minirpc/net/InetAddress.h"
#include "minirpc/registry/ZooKeeperClient.h"
#include "minirpc/registry/ZooKeeperProvider.h"
#include "minirpc/rpc/RpcServer.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <thread>

using namespace minirpc;

namespace{

constexpr const char* kServiceName="RegistryDemoService";
constexpr const char* kMethodName="WhoAmI";

struct Arguments{
    std::uint16_t port=0;
    std::string zookeeper_servers="127.0.0.1:2181";
    std::string advertise_ip="127.0.0.1";
};

Arguments ParseArguments(int argc,char* argv[]){
    if(argc<2){
        throw std::invalid_argument(
            "usage: registry_server PORT [ZOOKEEPER] [ADVERTISE_IP]"
        );
    }

    int port=std::stoi(argv[1]);
    if(port<1||port>65535){
        throw std::invalid_argument(
            "port must be between 1 and 65535"
        );
    }

    Arguments arguments;
    arguments.port=static_cast<std::uint16_t>(port);
    if(argc>2){
        arguments.zookeeper_servers=argv[2];
    }
    if(argc>3){
        arguments.advertise_ip=argv[3];
    }
    return arguments;
}

sigset_t BlockStopSignals(){
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals,SIGINT);
    sigaddset(&signals,SIGTERM);

    if(pthread_sigmask(SIG_BLOCK,&signals,nullptr)!=0){
        throw std::runtime_error("failed to block stop signals");
    }
    return signals;
}

}

int main(int argc,char* argv[]){
    try{
        Arguments arguments=ParseArguments(argc,argv);
        sigset_t stop_signals=BlockStopSignals();

        registry::ZooKeeperClientOptions zk_options;
        zk_options.servers=arguments.zookeeper_servers;
        zk_options.session_timeout=std::chrono::seconds(5);

        auto zk_client=
            std::make_shared<registry::ZooKeeperClient>(zk_options);
        zk_client->Start();
        if(!zk_client->WaitUntilConnected(std::chrono::seconds(10))){
            throw std::runtime_error("ZooKeeper connect timeout");
        }

        registry::ZooKeeperProvider provider(zk_client);
        provider.Register(
            kServiceName,
            cluster::Endpoint(
                arguments.advertise_ip,
                arguments.port
            )
        );

        net::EventLoop loop;
        rpc::RpcServer server(
            &loop,
            net::InetAddress("0.0.0.0",arguments.port)
        );
        server.RegisterMethod(
            kServiceName,
            kMethodName,
            [port=arguments.port](const std::string&){
                return std::to_string(port);
            }
        );
        server.Start();

        std::cout
            <<"registry demo server ready endpoint="
            <<arguments.advertise_ip<<':'<<arguments.port
            <<std::endl;

        std::thread stop_thread([&loop,stop_signals]()mutable{
            int signal=0;
            if(sigwait(&stop_signals,&signal)==0){
                loop.Stop();
            }
        });

        loop.Loop();
        zk_client->Close();
        stop_thread.join();
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"registry server error: "
                 <<error.what()<<'\n';
        return 1;
    }
}
