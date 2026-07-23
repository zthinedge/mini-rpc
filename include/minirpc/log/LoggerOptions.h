#pragma once

#include "minirpc/log/LogLevel.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace minirpc::log{

struct LoggerOptions{
    std::filesystem::path file_path="logs/minirpc.log";
    LogLevel min_level=LogLevel::Info;
    std::size_t queue_capacity=8192;
    std::size_t batch_size=256;
    std::size_t roll_size_bytes=64*1024*1024;
    std::size_t max_rolled_files=10;
    std::chrono::milliseconds flush_interval{1000};
};

struct LoggerStats{
    std::uint64_t enqueued=0;
    std::uint64_t written=0;
    std::uint64_t dropped=0;
    std::uint64_t write_errors=0;
    std::size_t queue_peak=0;
};

}
