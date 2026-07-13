#pragma once

#include "minirpc/net/Channel.h"
#include "minirpc/net/Socket.h"
#include <functional>

namespace minirpc::net{

class EventLoop;
class InetAddress;

class Acceptor{
public:
    using NewConnectionCallback=std::function<void(Socket,const InetAddress&)>;

    Acceptor(EventLoop*loop,const InetAddress& addr);

    void SetNewConnectionCallback(NewConnectionCallback cb);
    void Listen();

private:
    void HandleRead();

    Socket listen_socket_;
    Channel accept_channel_;
    NewConnectionCallback new_connection_callback_;
};

}
