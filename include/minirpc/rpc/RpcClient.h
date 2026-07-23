#pragma once

#include "minirpc/net/TcpClient.h"
#include "minirpc/protocol/RpcCodec.h"
#include "minirpc/rpc/CallOptions.h"
#include "minirpc/rpc/PendingCalls.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
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
    void Disconnect();

    protocol::RpcMessage Call(
        std::string service_name,
        std::string method_name,
        std::string payload,
        CallOptions options={}
    );

    ResponseFuture FutureCall(
        std::string service_name,
        std::string method_name,
        std::string payload,
        CallOptions options={}
    );

    void AsyncCall(
        std::string service_name,
        std::string method_name,
        std::string payload,
        ResponseCallback callback,
        CallOptions options={}
    );

    bool IsConnected()const noexcept;

    void SetConnectionCallback(ConnectionCallback callback);
    void SetCloseCallback(CloseCallback callback);
    void SetErrorCallback(ErrorCallback callback);

private:
    struct CallState{
        std::string service_name;
        std::string method_name;
        std::string payload;
        std::uint64_t deadline_us=0;
        std::uint32_t max_retries=0;
        std::uint32_t attempts=0;
        bool idempotent=false;
        ResponseCallback completion;
    };

    std::uint64_t NextRequestId()noexcept;

    void StartCall(
        std::string service_name,
        std::string method_name,
        std::string payload,
        CallOptions options,
        ResponseCallback completion
    );

    void StartAttempt(const std::shared_ptr<CallState>& state);
    void HandleAttemptResponse(
        const std::shared_ptr<CallState>& state,
        protocol::RpcMessage response
    );

    protocol::RpcMessage MakeRequest(
        const CallState& state
    );

    void AddTimeout(
        std::uint64_t request_id,
        std::uint64_t deadline_us
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
