#pragma once

#include "minirpc/protocol/RpcMessage.h"

#include <string>

namespace minirpc::net{
class Buffer;
}

namespace minirpc::protocol{

enum class DecodeStatus{
    Ok,
    NeedMoreData,
    ProtocolError
};

class RpcCodec{
public:
    std::string Encode(
        const RpcMessage& message
    )const;

    DecodeStatus DecodeOne(
        net::Buffer* buffer,
        RpcMessage* message,
        std::string* error
    )const;
};

}