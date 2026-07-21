#include "minirpc/rpc/RpcServer.h"

#include "minirpc/net/Buffer.h"
#include "minirpc/net/TcpConnection.h"

#include <utility>

namespace minirpc::rpc{

RpcServer::RpcServer(
    net::EventLoop* loop,
    const net::InetAddress& addr
):tcp_server_(loop,addr){
    tcp_server_.SetMessageCallback(
        [this](net::TcpConnection* connection,net::Buffer* buffer){
            HandleMessage(connection,buffer);
        }
    );
}

void RpcServer::RegisterMethod(
    std::string service_name,
    std::string method_name,
    MethodHandler handler
){
    dispatcher_.RegisterMethod(
        std::move(service_name),
        std::move(method_name),
        std::move(handler)
    );
}

void RpcServer::Start(){
    tcp_server_.Start();
}

void RpcServer::HandleMessage(
    net::TcpConnection* connection,
    net::Buffer* buffer
){
    while(true){
        protocol::RpcMessage request;
        std::string error;

        protocol::DecodeStatus status=codec_.DecodeOne(
            buffer,
            &request,
            &error
        );

        if(status==protocol::DecodeStatus::NeedMoreData){
            return;
        }

        if(status==protocol::DecodeStatus::ProtocolError||
           request.message_type!=protocol::MessageType::Request){
            connection->Close();
            return;
        }

        protocol::RpcMessage response=dispatcher_.Dispatch(request);
        connection->Send(codec_.Encode(response));
    }
}

}
