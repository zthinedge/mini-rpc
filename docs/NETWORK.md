# mini-rpc 网络层设计

## 1. 网络层职责
网络层主要是负责服务端和客户端之间的通信。服务端和客户端通过TCP进行数据的传输，网络层则是基于Reactor模式，高并发地处理这些发送的事件。网络层是项目的基座，接收时，网络层把 TCP 字节追加到 Buffer，并通过回调交给协议层解码；发送时，协议层将消息编码成字节，再交给网络层发送

## 2. 组件关系
Socket在bind/connect时使用InetAddress,Tcpconnnection拥有Socket，Channel，Buffer，EventLoop通过Poller管理一组channel，TcpServer持有一个非拥有型EventLoop指针，并拥有Acceptor和多个TcpConnection。

## 3. Reactor 事件处理流程
底层采用Socket TCP进行通信。事件处理为channel触发读事件或写事件 执行对应的回调函数。

## 4. Socket 和 InetAddress
InetAddress地址协议类包含IP Port等等。Socket包含一些由socket引申出来的一些函数，比如将fd设置为非阻塞，端口重用，listen，accept，以及send、recv

## 5. Buffer
在一个TCP连接中，存在输入缓冲区和输出缓冲区。主要用于处理 TCP 字节流、非阻塞部分读写、业务处理速度与网络速度不一致。通过读下标和写下标来操作vector中的一块缓冲区，这样每一次清理由上层读取过的数据时，只需要移动读下标，相比string优化了性能。当协议层读完缓冲区内容时，读索引和写索引均设置为0

## 6. Channel、Poller 和 EventLoop
EventLoop是用来管理channel的，主要操作是对channel上发生的每一个事件进行处理... Poller则主要是对epoll一些系统函数的封装，以及Poller将触发事件的Channel存在数组中，交给EventLoop。Channel 表示一个 fd 在 EventLoop 中的事件状态，但不拥有也不关闭该 fd，一个Channel对应一个fd，同时管理回调函数，比如监听器的channel读触发监听的功能。一个连接拥有一个channel，以及多个回调。同时事件循环拥有待执行队列，当连接关闭时需要将对应的操作函数放入待执行队列后，在当前一轮处理channel完后立即执行

## 7. Acceptor 和 TcpServer
Acceptor拥有acceptchannel只要有读事件，就会触发最开始注册的acceptor的回调处理函数，创建一个Socket交给TcpServer，TcpServer创建一个Tcp连接，然后将这个连接存入哈希表进行管理，并注册相关回调函数

## 8. Connector 和 TcpClient
Tcpclient拥有Connector，Connector主要负责连接服务端。
## 9. TcpConnection 读写流程
可读事件发生后，Channel 调用 TcpConnection 的读回调，HandleRead 循环 recv 到 EAGAIN，将数据追加到输入缓冲区，再调用消息回调。发送时，如果没有积压数据就先直接 send；发送不完的部分进入输出缓冲区并开启可写事件，HandleWrite 在 Socket 可写后继续发送，全部发送完成后关闭可写事件。

## 10. 连接建立与关闭时序
客户端调用连接器于服务端的监听器相作用，通过TCP三次握手建立连接。连接建立的时候需要传入参数Socket，同时设置对应的读写回调。

## 11. 线程模型和生命周期约束
当前采用单 Reactor、单线程模型。EventLoop 必须比关联的 Channel、TcpServer 和 TcpClient 活得更久。Socket 和 Poller 使用 RAII 在析构时关闭 fd，TcpConnection 由 unique_ptr 管理。连接关闭时通过 QueueInLoop 延迟释放，避免删除正在执行回调的 Channel。

## 12. 测试方式
使用 CMake 构建后通过 ctest --test-dir build --output-on-failure 运行测试，当前覆盖 Socket、Buffer、Connector 和 TcpClient/TcpServer 端到端收发

## 13. 当前限制
fd设置成了非阻塞，但是accept可能可以优化成accept4；主从reactor可能性能更好
