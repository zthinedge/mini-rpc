#include "minirpc/rpc/PendingCalls.h"

#include <stdexcept>
#include <utility>

namespace minirpc::rpc{
namespace{

protocol::RpcMessage MakeErrorResponse(
    std::uint64_t request_id,
    protocol::StatusCode status_code,
    const std::string& error_text
){
    protocol::RpcMessage response;
    response.message_type=protocol::MessageType::Response;
    response.request_id=request_id;
    response.meta.status_code=status_code;
    response.meta.error_text=error_text;
    return response;
}

}

PendingCalls::ResponseFuture PendingCalls::Add(
    std::uint64_t request_id
){
    if(request_id==0){
        throw std::invalid_argument("rpc request id must not be zero");
    }

    std::promise<protocol::RpcMessage> call;
    ResponseFuture future=call.get_future();

    std::lock_guard<std::mutex>lock(mutex_);
    auto result=calls_.emplace(request_id,std::move(call));

    if(!result.second){
        throw std::logic_error("rpc request id already pending");
    }

    return future;
}

bool PendingCalls::Complete(protocol::RpcMessage response){
    std::promise<protocol::RpcMessage> call;

    {
        std::lock_guard<std::mutex>lock(mutex_);
        auto pending=calls_.find(response.request_id);

        if(pending==calls_.end()){
            return false;
        }

        call=std::move(pending->second);
        calls_.erase(pending);
    }

    call.set_value(std::move(response));
    return true;
}

bool PendingCalls::Fail(
    std::uint64_t request_id,
    protocol::StatusCode status_code,
    const std::string& error_text
){
    return Complete(
        MakeErrorResponse(request_id,status_code,error_text)
    );
}

void PendingCalls::FailAll(
    protocol::StatusCode status_code,
    const std::string& error_text
){
    CallMap calls;

    {
        std::lock_guard<std::mutex>lock(mutex_);
        calls.swap(calls_);
    }

    for(auto& item:calls){
        item.second.set_value(
            MakeErrorResponse(item.first,status_code,error_text)
        );
    }
}

std::size_t PendingCalls::Size()const{
    std::lock_guard<std::mutex>lock(mutex_);
    return calls_.size();
}

}
