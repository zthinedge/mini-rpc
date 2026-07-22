#pragma once

#include "minirpc/net/TcpClient.h"
#include "minirpc/protocol/RpcCodec.h"
#include "minirpc/rpc/PendingCalls.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace minirpc::net{
class EventLoop;
class InetAddress;
}

namespace minirpc::rpc{

class RpcClient{
public:
    using ResponseFuture=PendingCalls::ResponseFuture;
    using ResponseCallback=PendingCalls::ResponseCallback;
    using ConnectionCallback=std::function<void()>;
    using CloseCallback=std::function<void()>;
    using ErrorCallback=std::function<void(int)>;

    RpcClient(
        net::EventLoop* loop,
        const net::InetAddress& server_addr
    );

    void Connect();

    protocol::RpcMessage Call(
        std::string service_name,
        std::string method_name,
        std::string payload
    );

    ResponseFuture FutureCall(
        std::string service_name,
        std::string method_name,
        std::string payload
    );

    void AsyncCall(
        std::string service_name,
        std::string method_name,
        std::string payload,
        ResponseCallback callback
    );

    bool IsConnected()const noexcept;

    void SetConnectionCallback(ConnectionCallback callback);
    void SetCloseCallback(CloseCallback callback);
    void SetErrorCallback(ErrorCallback callback);

private:
    std::uint64_t NextRequestId()noexcept;

    protocol::RpcMessage MakeRequest(
        std::string service_name,
        std::string method_name,
        std::string payload
    );

    void SendRequest(
        std::uint64_t request_id,
        std::string bytes
    );

    void HandleMessage(
        net::TcpConnection* connection,
        net::Buffer* buffer
    );

    net::EventLoop* loop_;
    net::TcpClient tcp_client_;
    protocol::RpcCodec codec_;
    std::atomic_uint64_t next_request_id_;
    std::atomic_bool connected_;
    PendingCalls pending_calls_;

    ConnectionCallback connection_callback_;
    CloseCallback close_callback_;
    ErrorCallback error_callback_;
};

}
