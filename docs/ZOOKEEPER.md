# ZooKeeper 服务注册

ZooKeeper 支持以可选模块构建，不影响没有安装 ZooKeeper C 客户端的
基础 RPC 库。

## 依赖与构建

Ubuntu 安装多线程 C 客户端：

```bash
sudo apt install libzookeeper-mt-dev
```

启用注册中心模块：

```bash
cmake -S . -B build-zk \
  -DMINIRPC_WITH_ZOOKEEPER=ON
cmake --build build-zk -j
```

业务程序需要链接 `minirpc_zookeeper`。

## Provider 节点

Provider 使用持久父节点和临时顺序子节点：

```text
/mini-rpc/services/UserService/providers/instance-xxxxxxxxxx
/mini-rpc/services/OrderService/providers/instance-xxxxxxxxxx
```

节点数据为 Provider 的 `ip:port`。

```cpp
using namespace minirpc;

registry::ZooKeeperClientOptions options;
options.servers="127.0.0.1:2181";

auto client=std::make_shared<registry::ZooKeeperClient>(options);
registry::ZooKeeperProvider provider(client);

client->Start();
if(!client->WaitUntilConnected(std::chrono::seconds(5))){
    throw std::runtime_error("ZooKeeper connect timeout");
}

provider.Register(
    "UserService",
    cluster::Endpoint("127.0.0.1",9000)
);
```

Consumer 首次拉取 Provider，同时注册一次性 child watch：

```cpp
registry::ZooKeeperDiscovery discovery(client);
discovery.WatchService("UserService");

registry::DiscoveryResult result=
    discovery.Resolve("UserService");

if(result.status==registry::DiscoveryStatus::NoProvider){
    // 当前没有可用实例
}
```

Watch 触发后会重新拉取全部 Provider，并在同一次操作中重新注册 Watch。
本地缓存使用 `shared_ptr<const vector<Endpoint>>` 不可变快照更新，调用线程
读取旧快照或新快照，不会观察到更新到一半的数据。普通断线期间保留最后
一次成功快照，新 Session 建立后会重新拉取。

普通网络闪断由 ZooKeeper 客户端在原 Session 中自动重连，不会重复注册。
Session 过期时封装会销毁旧 `zhandle_t`、创建新 Session；Provider 在新
Session 连接成功后重新创建临时顺序节点。

连接状态回调在客户端的管理线程执行。回调可以更新本地状态，但不要在
回调内部销毁 `ZooKeeperClient`；需要关闭时应投递到调用方自己的线程。

## 集成测试

默认测试只验证参数和节点路径。连接真实 ZooKeeper 的测试需要设置：

```bash
MINIRPC_ZOOKEEPER_TEST_SERVERS=127.0.0.1:2181 \
  ctest --test-dir build-zk -R zookeeper --output-on-failure
```
