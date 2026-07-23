# rpc_bench

先启动提供 `BenchService.Echo` 的示例服务端：

```bash
./build/calculator_server 9000
```

再打开另一个终端执行基线压测：

```bash
cd build
./rpc_bench --concurrency 50 --requests 10000 --payload 1024
```

参数含义：

- `--concurrency`：同时发起请求的工作线程数
- `--requests`：总请求数
- `--payload`：每次请求携带的字节数
- `--host`、`--port`：服务端地址，默认 `127.0.0.1:9000`

结果包含成功数、失败数、QPS，以及平均、最小、最大、
P50、P95、P99 延迟。
