#pragma once

#include <chrono>
#include <cstdint>

namespace minirpc::rpc{

struct CallOptions{
    // 相对超时；0表示不限制。
    std::chrono::microseconds timeout{0};

    // 绝对Unix时间，单位为微秒；0表示不限制。
    std::uint64_t deadline_us=0;

    // 首次调用失败后允许追加的最大尝试次数。
    std::uint32_t max_retries=0;
};

}
