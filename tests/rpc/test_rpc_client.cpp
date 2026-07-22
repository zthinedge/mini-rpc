#include "CalculatorService.h"
#include "CalculatorStub.h"
#include "minirpc/net/Buffer.h"
#include "minirpc/net/EventLoop.h"
#include "minirpc/net/InetAddress.h"
#include "minirpc/protocol/RpcCodec.h"
#include "minirpc/protocol/RpcMessage.h"
#include "minirpc/rpc/PendingCalls.h"
#include "minirpc/rpc/RpcClient.h"
#include "minirpc/rpc/RpcServer.h"

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <chrono>
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

void SendAll(int fd,const std::string& bytes){
    std::size_t sent=0;

    while(sent<bytes.size()){
        ssize_t size=::send(
            fd,
            bytes.data()+sent,
            bytes.size()-sent,
            MSG_NOSIGNAL
        );

        if(size==-1&&errno==EINTR){
            continue;
        }

        assert(size>0);
        sent+=static_cast<std::size_t>(size);
    }
}

std::vector<protocol::RpcMessage> ReadRequests(
    int fd,
    std::size_t count,
    net::Buffer* buffer
){
    protocol::RpcCodec codec;
    std::vector<protocol::RpcMessage> requests;

    while(requests.size()<count){
        protocol::RpcMessage request;
        std::string error;
        protocol::DecodeStatus status=codec.DecodeOne(
            buffer,
            &request,
            &error
        );

        if(status==protocol::DecodeStatus::Ok){
            requests.push_back(std::move(request));
            continue;
        }

        assert(status==protocol::DecodeStatus::NeedMoreData);

        char data[4096];
        ssize_t size=::recv(fd,data,sizeof(data),0);

        if(size==-1&&errno==EINTR){
            continue;
        }

        assert(size>0);
        buffer->Append(data,static_cast<std::size_t>(size));
    }

    return requests;
}

void RunOutOfOrderServer(
    std::uint16_t port,
    std::promise<void>* ready
){
    int listen_fd=::socket(AF_INET,SOCK_STREAM,0);
    assert(listen_fd!=-1);

    int reuse=1;
    assert(::setsockopt(
        listen_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    )==0);

    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    addr.sin_port=htons(port);

    assert(::bind(
        listen_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
    )==0);
    assert(::listen(listen_fd,1)==0);
    ready->set_value();

    int fd=::accept(listen_fd,nullptr,nullptr);
    assert(fd!=-1);

    net::Buffer buffer;
    protocol::RpcCodec codec;
    auto requests=ReadRequests(fd,2,&buffer);

    protocol::RpcMessage second=MakeResponse(
        requests[1].request_id,
        requests[1].payload
    );
    protocol::RpcMessage first=MakeResponse(
        requests[0].request_id,
        requests[0].payload
    );

    SendAll(fd,codec.Encode(second));
    SendAll(fd,codec.Encode(first));

    ReadRequests(fd,2,&buffer);
    ::close(fd);
    ::close(listen_fd);
}

void TestPendingCalls(){
    rpc::PendingCalls calls;
    auto first=calls.Add(1);
    auto second=calls.Add(2);
    std::promise<protocol::RpcMessage> callback_result;

    calls.Add(3,[&callback_result](protocol::RpcMessage response){
        callback_result.set_value(std::move(response));
    });

    assert(calls.Size()==3);
    assert(calls.Complete(MakeResponse(2,"second")));
    assert(calls.Size()==2);
    assert(calls.Complete(MakeResponse(3,"callback")));
    assert(calls.Complete(MakeResponse(1,"first")));
    assert(calls.Size()==0);

    assert(first.get().payload=="first");
    assert(second.get().payload=="second");
    assert(callback_result.get_future().get().payload=="callback");
    assert(!calls.Complete(MakeResponse(3,"unknown")));

    auto failed_future=calls.Add(4);
    std::promise<protocol::RpcMessage> failed_callback;
    calls.Add(5,[&failed_callback](protocol::RpcMessage response){
        failed_callback.set_value(std::move(response));
    });

    calls.FailAll(
        protocol::StatusCode::InternalError,
        "connection closed"
    );

    assert(calls.Size()==0);
    assert(failed_future.get().meta.status_code==
           protocol::StatusCode::InternalError);
    assert(failed_callback.get_future().get().meta.status_code==
           protocol::StatusCode::InternalError);
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

void TestOutOfOrderAndDisconnect(){
    std::uint16_t port=FindFreePort();
    std::promise<void>server_ready;
    std::thread server_thread(
        RunOutOfOrderServer,
        port,
        &server_ready
    );
    server_ready.get_future().get();

    std::promise<rpc::RpcClient*>client_ready;
    std::promise<net::EventLoop*>loop_ready;
    std::thread client_thread(
        [port,&client_ready,&loop_ready](){
            net::EventLoop loop;
            net::InetAddress address("127.0.0.1",port);
            rpc::RpcClient client(&loop,address);

            client.SetConnectionCallback([&](){
                client_ready.set_value(&client);
            });

            loop_ready.set_value(&loop);
            client.Connect();
            loop.Loop();
        }
    );

    rpc::RpcClient*client=client_ready.get_future().get();
    net::EventLoop*loop=loop_ready.get_future().get();

    auto future=client->FutureCall(
        "TestService",
        "Future",
        "future response"
    );

    std::promise<protocol::RpcMessage>callback_result;
    client->AsyncCall(
        "TestService",
        "Callback",
        "callback response",
        [&callback_result](protocol::RpcMessage response){
            callback_result.set_value(std::move(response));
        }
    );

    auto callback_future=callback_result.get_future();
    assert(callback_future.wait_for(std::chrono::seconds(2))==
           std::future_status::ready);
    assert(future.wait_for(std::chrono::seconds(2))==
           std::future_status::ready);
    assert(callback_future.get().payload=="callback response");
    assert(future.get().payload=="future response");

    auto disconnected_future=client->FutureCall(
        "TestService",
        "PendingFuture",
        ""
    );

    std::promise<protocol::RpcMessage>disconnected_callback;
    client->AsyncCall(
        "TestService",
        "PendingCallback",
        "",
        [&disconnected_callback](protocol::RpcMessage response){
            disconnected_callback.set_value(std::move(response));
        }
    );

    auto disconnected_callback_future=
        disconnected_callback.get_future();

    assert(disconnected_future.wait_for(std::chrono::seconds(2))==
           std::future_status::ready);
    assert(disconnected_callback_future.wait_for(
               std::chrono::seconds(2)
           )==std::future_status::ready);

    assert(disconnected_future.get().meta.status_code==
           protocol::StatusCode::InternalError);
    assert(disconnected_callback_future.get().meta.status_code==
           protocol::StatusCode::InternalError);

    loop->Stop();
    client_thread.join();
    server_thread.join();
}

}

int main(){
    TestPendingCalls();
    TestCalculatorRpc();
    TestOutOfOrderAndDisconnect();
    return 0;
}
