#include "minirpc/net/TcpServer.h"
#include "minirpc/net/EventLoop.h"

#include <utility>

namespace minirpc::net{

TcpServer::TcpServer(EventLoop*loop,const InetAddress& addr)
    :loop_(loop),acceptor_(loop,addr){
    acceptor_.SetNewConnectionCallback(
        [this](Socket socket,const InetAddress& peer_addr){
            HandleNewConnection(std::move(socket),peer_addr);
        }
    );
}

void TcpServer::Start(){
    acceptor_.Listen();
}

void TcpServer::SetMessageCallback(MessageCallback cb){
    message_callback_=std::move(cb);
}

void TcpServer::HandleNewConnection(Socket socket,const InetAddress&){
    auto connection=std::make_unique<TcpConnection>(loop_,std::move(socket));

    int fd=connection->Fd();

    connection->SetMessageCallback(
        [this](TcpConnection* connection,Buffer* buffer){
            if(message_callback_){
                message_callback_(connection,buffer);
            }
        }
    );

    connection->SetCloseCallback(
        [this](TcpConnection* connection){
            HandleClose(connection);
        }
    );

    TcpConnection* connection_ptr=connection.get();
    connections_.emplace(fd,std::move(connection));
    connection_ptr->Start();
}

void TcpServer::HandleClose(TcpConnection* connection){
    int fd=connection->Fd();
    //放入EventLoop的待执行队列
    loop_->QueueInLoop([this,fd](){
        connections_.erase(fd);
    });
}

}
