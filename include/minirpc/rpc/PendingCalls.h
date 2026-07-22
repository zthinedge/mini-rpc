#pragma once

#include "minirpc/protocol/RpcMessage.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>

namespace minirpc::rpc{

class PendingCalls{
public:
    using ResponseFuture=std::future<protocol::RpcMessage>;
    using ResponseCallback=
        std::function<void(protocol::RpcMessage)>;

    ResponseFuture Add(std::uint64_t request_id);
    void Add(
        std::uint64_t request_id,
        ResponseCallback callback
    );

    bool Complete(protocol::RpcMessage response);

    bool Fail(
        std::uint64_t request_id,
        protocol::StatusCode status_code,
        const std::string& error_text
    );

    void FailAll(
        protocol::StatusCode status_code,
        const std::string& error_text
    );

    std::size_t Size()const;

private:
    struct PendingCall{
        std::promise<protocol::RpcMessage> promise;
        ResponseCallback callback;
    };

    using CallMap=
        std::unordered_map<
            std::uint64_t,
            PendingCall
        >;

    void Insert(std::uint64_t request_id,PendingCall call);
    static void Finish(
        PendingCall call,
        protocol::RpcMessage response
    )noexcept;

    mutable std::mutex mutex_;
    CallMap calls_;
};

}
