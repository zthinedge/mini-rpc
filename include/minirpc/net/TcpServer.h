#pragma once

#include "minirpc/net/Acceptor.h"
#include "minirpc/net/TcpConnection.h"

#include <memory>
#include <unordered_map>

namespace minirpc::net{

class EventLoop;
class InetAddress;

class TcpServer{
public:
    using MessageCallback=TcpConnection::MessageCallback;

    TcpServer(EventLoop*loop,const InetAddress& addr);
    ~TcpServer()=default;

    void Start();
    void SetMessageCallback(MessageCallback cb);

private:
    void HandleNewConnection(Socket socket,const InetAddress& peer_addr);

    void HandleClose(TcpConnection* connection);

    EventLoop* loop_;
    Acceptor acceptor_;

    std::unordered_map<int,std::unique_ptr<TcpConnection>> connections_;

    MessageCallback message_callback_;
};

}
