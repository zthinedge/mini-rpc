#pragma once
#include <netinet/in.h>
#include <string>
#include <cstdint>
#include <arpa/inet.h>

namespace net::minirpc {

class InetAddress {
private:
    sockaddr_in addr_;    
public:
    InetAddress(const std::string& ip, std::uint16_t port);
    ~InetAddress() = default; // 默认即可

    std::string Ip() const;
    std::string Port() const;

    const struct sockaddr* GetSockAddr() const {
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }
    socklen_t GetSockLen() const {
        return sizeof(addr_);
    }
};

}