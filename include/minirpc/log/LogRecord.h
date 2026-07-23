#pragma once

#include "minirpc/log/LogLevel.h"

#include <chrono>
#include <string>
#include <thread>

namespace minirpc::log{

struct LogRecord{
    LogLevel level=LogLevel::Info;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id thread_id;
    std::string file;
    int line=0;
    std::string function;
    std::string message;
};

}
