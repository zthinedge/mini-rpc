#pragma once

#include "minirpc/net/Buffer.h"
#include "minirpc/net/Channel.h"
#include "minirpc/net/Socket.h"

#include <functional>
#include <string>

namespace minirpc::net{

class EventLoop;

class TcpConnection{
public:
    using MessageCallback=
        std::function<void(TcpConnection*,Buffer*)>;

    using CloseCallback=
        std::function<void(TcpConnection*)>;

    TcpConnection(EventLoop*loop,Socket socket);
    ~TcpConnection()=default;

    void Start();
    void Send(const std::string& data);
    void Close();

    void SetMessageCallback(MessageCallback cb);
    void SetCloseCallback(CloseCallback cb);

    int Fd()const noexcept;

private:
    void HandleRead();
    void HandleWrite();
    void HandleClose();

    EventLoop* loop_;
    Socket socket_;
    Channel channel_;

    Buffer input_buffer_;
    Buffer output_buffer_;

    MessageCallback message_callback_;
    CloseCallback close_callback_;
    bool closed_;
};

}
