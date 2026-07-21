#pragma once

#include "minirpc/protocol/RpcMessage.h"

#include <cstddef>
#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>

namespace minirpc::rpc{

class PendingCalls{
public:
    using ResponseFuture=std::future<protocol::RpcMessage>;

    ResponseFuture Add(std::uint64_t request_id);
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
    using CallMap=
        std::unordered_map<
            std::uint64_t,
            std::promise<protocol::RpcMessage>
        >;

    mutable std::mutex mutex_;
    CallMap calls_;
};

}
