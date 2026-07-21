#include "CalculatorService.h"
#include "CalculatorStub.h"
#include "minirpc/net/EventLoop.h"
#include "minirpc/net/InetAddress.h"
#include "minirpc/protocol/RpcMessage.h"
#include "minirpc/rpc/PendingCalls.h"
#include "minirpc/rpc/RpcClient.h"
#include "minirpc/rpc/RpcServer.h"

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <future>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace minirpc;
using namespace minirpc::example::calculator;

namespace{

class CalculatorServiceImpl:public CalculatorService{
public:
    void Add(
        const AddRequest& request,
        AddResponse* response
    )override{
        response->set_result(request.a()+request.b());
    }
};

std::uint16_t FindFreePort(){
    int fd=::socket(AF_INET,SOCK_STREAM,0);
    assert(fd!=-1);

    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    addr.sin_port=0;

    assert(::bind(
        fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
    )==0);

    socklen_t len=sizeof(addr);
    assert(::getsockname(
        fd,
        reinterpret_cast<sockaddr*>(&addr),
        &len
    )==0);

    std::uint16_t port=ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

protocol::RpcMessage MakeResponse(
    std::uint64_t request_id,
    const std::string& payload
){
    protocol::RpcMessage response;
    response.message_type=protocol::MessageType::Response;
    response.request_id=request_id;
    response.payload=payload;
    return response;
}

void TestPendingCalls(){
    rpc::PendingCalls calls;
    auto first=calls.Add(1);
    auto second=calls.Add(2);

    assert(calls.Size()==2);
    assert(calls.Complete(MakeResponse(2,"second")));
    assert(calls.Size()==1);
    assert(calls.Complete(MakeResponse(1,"first")));
    assert(calls.Size()==0);

    assert(first.get().payload=="first");
    assert(second.get().payload=="second");
    assert(!calls.Complete(MakeResponse(3,"unknown")));
}

void TestCalculatorRpc(){
    std::uint16_t port=FindFreePort();
    std::promise<net::EventLoop*>server_ready;

    std::thread server_thread([port,&server_ready](){
        net::EventLoop loop;
        net::InetAddress address("127.0.0.1",port);
        rpc::RpcServer server(&loop,address);
        CalculatorServiceImpl service;
        CalculatorServiceAdapter adapter(&service);

        adapter.RegisterTo(&server);
        server.Start();
        server_ready.set_value(&loop);
        loop.Loop();
    });

    net::EventLoop*server_loop=server_ready.get_future().get();
    std::promise<rpc::RpcClient*>client_ready;
    std::promise<net::EventLoop*>client_loop_ready;

    std::thread client_thread(
        [port,&client_ready,&client_loop_ready](){
            net::EventLoop loop;
            net::InetAddress address("127.0.0.1",port);
            rpc::RpcClient client(&loop,address);

            client.SetConnectionCallback([&](){
                client_ready.set_value(&client);
            });

            client_loop_ready.set_value(&loop);
            client.Connect();
            loop.Loop();
        }
    );

    rpc::RpcClient*client=client_ready.get_future().get();
    net::EventLoop*client_loop=client_loop_ready.get_future().get();
    CalculatorStub stub(client);

    AddRequest request;
    request.set_a(20);
    request.set_b(22);
    AddResponse response=stub.Add(request);
    assert(response.result()==42);

    constexpr int call_count=16;
    std::vector<std::thread>callers;
    std::vector<int>results(call_count);

    for(int i=0;i<call_count;++i){
        callers.emplace_back([i,&stub,&results](){
            AddRequest request;
            request.set_a(i);
            request.set_b(i*2);
            results[i]=stub.Add(request).result();
        });
    }

    for(auto&caller:callers){
        caller.join();
    }

    for(int i=0;i<call_count;++i){
        assert(results[i]==i*3);
    }

    client_loop->Stop();
    client_thread.join();
    server_loop->Stop();
    server_thread.join();
}

}

int main(){
    TestPendingCalls();
    TestCalculatorRpc();
    return 0;
}
