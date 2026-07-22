#include "minirpc/rpc/RpcClient.h"

#include "minirpc/net/Buffer.h"
#include "minirpc/net/EventLoop.h"
#include "minirpc/net/TcpConnection.h"

#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace minirpc::rpc{
namespace{

std::uint64_t CurrentTimeMicros(){
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

std::uint64_t AddTimeoutToNow(
    std::uint64_t now,
    std::chrono::microseconds timeout
){
    std::uint64_t duration=static_cast<std::uint64_t>(
        timeout.count()
    );

    if(duration>std::numeric_limits<std::uint64_t>::max()-now){
        return std::numeric_limits<std::uint64_t>::max();
    }

    return now+duration;
}

std::uint64_t ResolveDeadline(const CallOptions& options){
    if(options.timeout.count()<0){
        throw std::invalid_argument("rpc timeout must not be negative");
    }

    std::uint64_t deadline=options.deadline_us;

    if(options.timeout.count()>0){
        std::uint64_t timeout_deadline=AddTimeoutToNow(
            CurrentTimeMicros(),
            options.timeout
        );

        if(deadline==0||timeout_deadline<deadline){
            deadline=timeout_deadline;
        }
    }

    return deadline;
}

bool DeadlineReached(std::uint64_t deadline_us){
    return deadline_us!=0&&CurrentTimeMicros()>=deadline_us;
}

}

RpcClient::RpcClient(
    net::EventLoop* loop,
    const net::InetAddress& server_addr
):loop_(loop),
  tcp_client_(loop,server_addr),
  next_request_id_(1),
  connected_(false){
    tcp_client_.SetConnectionCallback(
        [this](net::TcpConnection*){
            connected_.store(true);

            if(connection_callback_){
                connection_callback_();
            }
        }
    );

    tcp_client_.SetMessageCallback(
        [this](net::TcpConnection* connection,net::Buffer* buffer){
            HandleMessage(connection,buffer);
        }
    );

    tcp_client_.SetCloseCallback([this](){
        connected_.store(false);
        pending_calls_.FailAll(
            protocol::StatusCode::InternalError,
            "rpc connection closed"
        );

        if(close_callback_){
            close_callback_();
        }
    });

    tcp_client_.SetErrorCallback([this](int error){
        connected_.store(false);
        pending_calls_.FailAll(
            protocol::StatusCode::InternalError,
            "rpc connection failed"
        );

        if(error_callback_){
            error_callback_(error);
        }
    });
}

void RpcClient::Connect(){
    loop_->RunInLoop([this](){
        tcp_client_.Connect();
    });
}

protocol::RpcMessage RpcClient::Call(
    std::string service_name,
    std::string method_name,
    std::string payload,
    CallOptions options
){
    if(loop_->IsInLoopThread()){
        throw std::logic_error(
            "synchronous rpc call in EventLoop thread"
        );
    }

    return FutureCall(
        std::move(service_name),
        std::move(method_name),
        std::move(payload),
        options
    ).get();
}

RpcClient::ResponseFuture RpcClient::FutureCall(
    std::string service_name,
    std::string method_name,
    std::string payload,
    CallOptions options
){
    auto promise=
        std::make_shared<std::promise<protocol::RpcMessage>>();
    ResponseFuture future=promise->get_future();

    StartCall(
        std::move(service_name),
        std::move(method_name),
        std::move(payload),
        options,
        [promise](protocol::RpcMessage response){
            promise->set_value(std::move(response));
        }
    );

    return future;
}

void RpcClient::AsyncCall(
    std::string service_name,
    std::string method_name,
    std::string payload,
    ResponseCallback callback,
    CallOptions options
){
    if(!callback){
        throw std::invalid_argument("rpc response callback is empty");
    }

    StartCall(
        std::move(service_name),
        std::move(method_name),
        std::move(payload),
        options,
        std::move(callback)
    );
}

void RpcClient::StartCall(
    std::string service_name,
    std::string method_name,
    std::string payload,
    CallOptions options,
    ResponseCallback completion
){
    auto state=std::make_shared<CallState>();
    state->service_name=std::move(service_name);
    state->method_name=std::move(method_name);
    state->payload=std::move(payload);
    state->deadline_us=ResolveDeadline(options);
    state->max_retries=options.max_retries;
    state->completion=std::move(completion);

    StartAttempt(state);
}

void RpcClient::StartAttempt(
    const std::shared_ptr<CallState>& state
){
    protocol::RpcMessage request=MakeRequest(*state);
    std::string bytes=codec_.Encode(request);
    ++state->attempts;

    pending_calls_.Add(
        request.request_id,
        state->deadline_us,
        [this,state](protocol::RpcMessage response){
            HandleAttemptResponse(state,std::move(response));
        }
    );

    if(state->deadline_us!=0){
        AddTimeout(request.request_id,state->deadline_us);
    }

    if(DeadlineReached(state->deadline_us)){
        return;
    }

    SendRequest(request.request_id,std::move(bytes));
}

void RpcClient::HandleAttemptResponse(
    const std::shared_ptr<CallState>& state,
    protocol::RpcMessage response
){
    bool can_retry=
        response.meta.status_code==protocol::RpcError::InternalError&&
        state->attempts<=state->max_retries&&
        !DeadlineReached(state->deadline_us);

    if(can_retry){
        loop_->RunAfter(
            std::chrono::microseconds(1),
            [this,state](){
                StartAttempt(state);
            }
        );
        return;
    }

    state->completion(std::move(response));
}

protocol::RpcMessage RpcClient::MakeRequest(
    const CallState& state
){
    protocol::RpcMessage request;
    request.message_type=protocol::MessageType::Request;
    request.request_id=NextRequestId();
    request.meta.service_name=state.service_name;
    request.meta.method_name=state.method_name;
    request.meta.deadline_us=state.deadline_us;
    request.payload=state.payload;
    return request;
}

void RpcClient::AddTimeout(
    std::uint64_t request_id,
    std::uint64_t deadline_us
){
    std::uint64_t now=CurrentTimeMicros();
    std::uint64_t remaining=deadline_us>now?deadline_us-now:0;

    net::EventLoop::TimerId timer_id=loop_->RunAfter(
        std::chrono::microseconds(remaining),
        [this,request_id](){
            pending_calls_.Expire(request_id);
        }
    );

    bool attached=pending_calls_.SetTimeoutCancel(
        request_id,
        [this,timer_id](){
            loop_->CancelTimer(timer_id);
        }
    );

    if(!attached){
        loop_->CancelTimer(timer_id);
    }
}

void RpcClient::SendRequest(
    std::uint64_t request_id,
    std::string bytes
){
    loop_->RunInLoop(
        [this,request_id,bytes=std::move(bytes)](){
            if(!tcp_client_.IsConnected()){
                pending_calls_.Fail(
                    request_id,
                    protocol::StatusCode::InternalError,
                    "rpc client is not connected"
                );
                return;
            }

            tcp_client_.Send(bytes);
        }
    );
}

bool RpcClient::IsConnected()const noexcept{
    return connected_.load();
}

void RpcClient::SetConnectionCallback(ConnectionCallback callback){
    connection_callback_=std::move(callback);
}

void RpcClient::SetCloseCallback(CloseCallback callback){
    close_callback_=std::move(callback);
}

void RpcClient::SetErrorCallback(ErrorCallback callback){
    error_callback_=std::move(callback);
}

std::uint64_t RpcClient::NextRequestId()noexcept{
    std::uint64_t request_id=next_request_id_.fetch_add(1);

    while(request_id==0){
        request_id=next_request_id_.fetch_add(1);
    }

    return request_id;
}

void RpcClient::HandleMessage(
    net::TcpConnection* connection,
    net::Buffer* buffer
){
    while(true){
        protocol::RpcMessage response;
        std::string error;

        protocol::DecodeStatus status=codec_.DecodeOne(
            buffer,
            &response,
            &error
        );

        if(status==protocol::DecodeStatus::NeedMoreData){
            return;
        }

        if(status==protocol::DecodeStatus::ProtocolError||
           response.message_type!=protocol::MessageType::Response){
            connection->Close();
            return;
        }

        pending_calls_.Complete(std::move(response));
    }
}

}
