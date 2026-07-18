#pragma once

#include "minirpc/protocol/RpcHeader.h"
#include "minirpc/protocol/RpcMeta.h"

#include <cstdint>
#include <string>

namespace minirpc::protocol{

struct RpcMessage{
    MessageType message_type=MessageType::Unknown;
    CodecType codec=CodecType::MiniProtobuf;
    std::uint8_t flags=0;
    std::uint64_t request_id=0;

    RpcMeta meta;
    std::string payload;
};

}
