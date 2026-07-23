#include "minirpc/net/TcpClient.h"
#include "minirpc/net/EventLoop.h"

#include <utility>

namespace minirpc::net{

TcpClient::TcpClient(EventLoop*loop,const InetAddress& server_addr)
    :loop_(loop),connector_(loop,server_addr){
    connector_.SetNewConnectionCallback([this](Socket socket){
        HandleNewConnection(std::move(socket));
    });

    connector_.SetErrorCallback([this](int error){
        if(error_callback_){
            error_callback_(error);
        }
    });
}

void TcpClient::Connect(){
    connector_.Connect();
}

void TcpClient::Disconnect(){
    loop_->RunInLoop([this](){
        if(connection_){
            connection_->Close();
        }
    });
}

void TcpClient::Send(const std::string& data){
    if(connection_){
        connection_->Send(data);
    }
}

bool TcpClient::IsConnected()const noexcept{
    return connection_!=nullptr;
}

void TcpClient::SetConnectionCallback(ConnectionCallback cb){
    connection_callback_=std::move(cb);
}

void TcpClient::SetCloseCallback(CloseCallback cb){
    close_callback_=std::move(cb);
}

void TcpClient::SetMessageCallback(MessageCallback cb){
    message_callback_=std::move(cb);
}

void TcpClient::SetErrorCallback(ErrorCallback cb){
    error_callback_=std::move(cb);
}

void TcpClient::HandleNewConnection(Socket socket){
    auto connection=std::make_unique<TcpConnection>(
        loop_,
        std::move(socket)
    );

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

    connection_=std::move(connection);
    connection_->Start();

    if(connection_callback_){
        connection_callback_(connection_.get());
    }
}

void TcpClient::HandleClose(TcpConnection* connection){
    loop_->QueueInLoop([this,connection](){
        if(connection_.get()==connection){
            connection_.reset();

            if(close_callback_){
                close_callback_();
            }
        }
    });
}

}
