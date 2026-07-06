# mini-rpc

## 1.项目定位

`mini-rpc` 是一个面向 `C++` 后端服务拆分场景的轻量级 `RPC` 框架。

项目实现保持小而可控，但设计边界对标工程化 RPC 框架：参考 `gRPC` 的 `Stub/Channel/Status/Deadline` 思想，参考 `brpc` 对高并发、连接、Buffer、日志和可观测性的重视。第一版不堆功能，优先把协议、调用语义、序列化、网络和日志边界设计正确。

## 2.设计目标

第一版：

1. 自定义 RPC 二进制协议。
2. 支持 `request_id` 请求响应匹配。
3. 支持同步调用和超时控制。
4. 支持服务端方法注册和分发。
5. 支持 `mini-protobuf` 序列化。
6. 接入 `AsyncLogger`。
7. 提供 `UserService` / `OrderService` 示例。

第二版：

1. 异步调用、连接复用、连接池。
2. 服务发现、轮询负载均衡、失败重试。
3. metrics 统计和压测报告。

第三版：

1. HTTP 网关、配置中心、健康检查。
2. 服务注册中心、管理页面。
3. `trace_id` 链路追踪。

## 3.整体架构

详细设计见：[DESIGN-CN.md](./DESIGN-CN.md)

```text
业务层    
RPC接口层    
RPC调用层    
序列化层     
mini-protobuf

---
协议层   
RPCMessage<-->bytes

---   
网络层    
TCP /EventLoop /Buffer

---
基础组件     
ThreadPool /AsyncLogger /Timer
```
