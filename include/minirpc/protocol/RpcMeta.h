#pragma once

#include <cstdint>
#include <string>

namespace minirpc::protocol{

enum class StatusCode : std::int32_t{
    Ok=0,
    DecodeError=1,
    ServiceNotFound=2,
    MethodNotFound=3,
    InvokeError=4,
    InternalError=5
};

struct RpcMeta{
    std::string service_name;
    std::string method_name;

    StatusCode status_code=StatusCode::Ok;
    std::string error_text;
};

}
