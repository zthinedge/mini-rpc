# mini-rpc 目录结构

```text
include/minirpc/
  common/          通用状态、错误码、不可拷贝基类等基础类型
  net/             Reactor 网络层：Buffer、EventLoop、Channel、Poller、TcpConnection
  protocol/        RPC 二进制协议：RpcHeader、RpcMeta、RpcMessage、RpcCodec
  rpc/             RPC 调用层：RpcClient、RpcServer、RpcChannel、PendingCalls、ServiceRegistry
  serialization/   序列化层：Serializer、MiniProtobufSerializer

src/
  common/          common 对应实现
  net/             net 对应实现
  protocol/        protocol 对应实现
  rpc/             rpc 对应实现
  serialization/   serialization 对应实现

examples/
  client/          示例客户端入口
  server/          示例服务端入口
  services/user/   UserService 示例的 message、stub、adapter、impl
  services/order/  OrderService 示例的 message、stub、adapter、impl

tests/
  common/          基础类型测试
  net/             Buffer/EventLoop/TcpConnection 测试
  protocol/        RpcCodec 半包、粘包、非法包测试
  rpc/             PendingCalls、ServiceRegistry、RpcClient/RpcServer 测试
  serialization/   mini-protobuf 序列化测试
```

核心原则：

1. `net` 不知道 RPC，只处理 fd、事件、连接和 Buffer。
2. `protocol` 不知道业务对象，只处理 `RpcMessage <-> bytes`。
3. `rpc` 不知道 `UserService` / `OrderService` 的具体类型，只处理 payload、request_id、注册分发和错误。
4. 示例业务只能放在 `examples/services`，不能污染框架核心。
