#include "minirpc/rpc/RpcClient.h"

#include "minirpc/net/Buffer.h"
#include "minirpc/net/EventLoop.h"
#include "minirpc/net/TcpConnection.h"

#include <utility>

namespace minirpc::rpc{

RpcClient::RpcClient(
    net::EventLoop* loop,
    const net::InetAddress& server_addr
):loop_(loop),
  tcp_client_(loop,server_addr),
  next_request_id_(1),
  connected_(false){
    tcp_client_.SetConnectionCallback(
        [this](net::TcpConnection*){
            connected_.store(true);

            if(connection_callback_){
                connection_callback_();
            }
        }
    );

    tcp_client_.SetMessageCallback(
        [this](net::TcpConnection* connection,net::Buffer* buffer){
            HandleMessage(connection,buffer);
        }
    );

    tcp_client_.SetCloseCallback([this](){
        connected_.store(false);
        pending_calls_.FailAll(
            protocol::StatusCode::InternalError,
            "rpc connection closed"
        );

        if(close_callback_){
            close_callback_();
        }
    });

    tcp_client_.SetErrorCallback([this](int error){
        connected_.store(false);
        pending_calls_.FailAll(
            protocol::StatusCode::InternalError,
            "rpc connection failed"
        );

        if(error_callback_){
            error_callback_(error);
        }
    });
}

void RpcClient::Connect(){
    loop_->RunInLoop([this](){
        tcp_client_.Connect();
    });
}

RpcClient::ResponseFuture RpcClient::Call(
    std::string service_name,
    std::string method_name,
    std::string payload
){
    protocol::RpcMessage request;
    request.message_type=protocol::MessageType::Request;
    request.request_id=NextRequestId();
    request.meta.service_name=std::move(service_name);
    request.meta.method_name=std::move(method_name);
    request.payload=std::move(payload);

    std::string bytes=codec_.Encode(request);
    ResponseFuture future=pending_calls_.Add(request.request_id);
    std::uint64_t request_id=request.request_id;

    loop_->RunInLoop(
        [this,request_id,bytes=std::move(bytes)](){
            if(!tcp_client_.IsConnected()){
                pending_calls_.Fail(
                    request_id,
                    protocol::StatusCode::InternalError,
                    "rpc client is not connected"
                );
                return;
            }

            tcp_client_.Send(bytes);
        }
    );

    return future;
}

bool RpcClient::IsConnected()const noexcept{
    return connected_.load();
}

void RpcClient::SetConnectionCallback(ConnectionCallback callback){
    connection_callback_=std::move(callback);
}

void RpcClient::SetCloseCallback(CloseCallback callback){
    close_callback_=std::move(callback);
}

void RpcClient::SetErrorCallback(ErrorCallback callback){
    error_callback_=std::move(callback);
}

std::uint64_t RpcClient::NextRequestId()noexcept{
    std::uint64_t request_id=next_request_id_.fetch_add(1);

    while(request_id==0){
        request_id=next_request_id_.fetch_add(1);
    }

    return request_id;
}

void RpcClient::HandleMessage(
    net::TcpConnection* connection,
    net::Buffer* buffer
){
    while(true){
        protocol::RpcMessage response;
        std::string error;

        protocol::DecodeStatus status=codec_.DecodeOne(
            buffer,
            &response,
            &error
        );

        if(status==protocol::DecodeStatus::NeedMoreData){
            return;
        }

        if(status==protocol::DecodeStatus::ProtocolError||
           response.message_type!=protocol::MessageType::Response){
            connection->Close();
            return;
        }

        pending_calls_.Complete(std::move(response));
    }
}

}
