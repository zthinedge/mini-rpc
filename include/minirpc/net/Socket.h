#pragma once
#include <cstddef>  // size_t
#include <sys/types.h> // ssize_t

// 前向声明，避免在头文件里包含 InetAddress.h（减少依赖）
namespace net::minirpc {
    class InetAddress;
}

namespace net::minirpc {

class Socket {
private:
    int fd_;

public:
    Socket();
    ~Socket();

    // 禁用拷贝
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // 支持移动
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // 核心操作
    void Bind(const InetAddress& addr);
    void Listen(int backlog = 1024);
    Socket Accept(InetAddress* peer_addr = nullptr);
    void Connect(const InetAddress& server_addr);

    ssize_t Send(const void* data, size_t len, int flags = 0);
    ssize_t Recv(void* buffer, size_t len, int flags = 0);

    int GetFd() const noexcept;

private:
    // 私有构造：仅供 Accept 内部使用
    explicit Socket(int fd);
};

}
