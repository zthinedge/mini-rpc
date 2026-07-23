#pragma once

#include "minirpc/log/AsyncLogger.h"

#define MINIRPC_LOG(logger,level,message)                              \
    do{                                                               \
        auto& minirpc_logger_=(logger);                               \
        if(minirpc_logger_.ShouldLog(level)){                         \
            minirpc_logger_.Log(                                     \
                level,__FILE__,__LINE__,__func__,message             \
            );                                                        \
        }                                                             \
    }while(false)

#define MINIRPC_LOG_TRACE(logger,message) \
    MINIRPC_LOG(logger,::minirpc::log::LogLevel::Trace,message)

#define MINIRPC_LOG_DEBUG(logger,message) \
    MINIRPC_LOG(logger,::minirpc::log::LogLevel::Debug,message)

#define MINIRPC_LOG_INFO(logger,message) \
    MINIRPC_LOG(logger,::minirpc::log::LogLevel::Info,message)

#define MINIRPC_LOG_WARN(logger,message) \
    MINIRPC_LOG(logger,::minirpc::log::LogLevel::Warn,message)

#define MINIRPC_LOG_ERROR(logger,message) \
    MINIRPC_LOG(logger,::minirpc::log::LogLevel::Error,message)

#define MINIRPC_LOG_FATAL(logger,message) \
    MINIRPC_LOG(logger,::minirpc::log::LogLevel::Fatal,message)
