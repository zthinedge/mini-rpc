# mini-rpc
## 1.项目定位
`mini-rpc`是一个面向`C++`后端服务拆分场景的轻量级 `RPC`框架。

## 2.设计目标
1. 
## 3.整体架构
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


