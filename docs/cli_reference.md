# KV-Store CLI 参考

本文档说明仓库中的可执行文件和脚本入口。

## 1. `kvstore_server`

`kvstore_server` 是系统的服务端入口。启动后会监听 TCP 端口，接收客户端发送的 RESP-like 协议命令，并调用内部 KVStore 完成读写操作。

命令：

```bash
./bin/kvstore_server [--no-wal] [--wal-sync-ms <ms>]
```

参数：

- `--no-wal`
  关闭 WAL，以纯内存模式运行。
- `--wal-sync-ms <ms>`
  设置 WAL 的 `fsync` 周期，默认 `10`。
- `--wal-sync-ms 0`
  每次写入都同步刷盘。

## 2. `kvstore_client`

`kvstore_client` 是交互式客户端，适合手工输入命令检查服务端是否正常工作。它不是唯一接入方式，任何能建立 TCP 连接并发送 RESP-like 协议的程序都可以访问服务端。

命令：

```bash
./bin/kvstore_client [host] [port]
./bin/kvstore_client --raw-resp [host] [port]
```

默认值：

- `host=127.0.0.1`
- `port=6380`

用途：

- 手工输入命令，由客户端自动转码为 RESP-like 请求
- 做交互式冒烟验证
- 手动发送 `CHECKPOINT` 触发快照与 WAL 截断
- 在 `--raw-resp` 模式下直接转发显式 RESP 测试报文

## 3. `kvstore_bench`

`kvstore_bench` 用于端到端测试服务端吞吐。它会真实经过 TCP、协议解析、Reactor 调度和存储执行，因此结果反映完整服务路径的表现。

命令：

```bash
./bin/kvstore_bench [host] [port] [operations] [pipeline_depth] [scenario] [clients]
```

参数：

- `host`
- `port`
- `operations`
- `pipeline_depth`
- `scenario`
- `clients`

`scenario` 支持：

- `put-get`
- `full`

典型命令：

```bash
./bin/kvstore_bench 127.0.0.1 6380 5000 1
./bin/kvstore_bench 127.0.0.1 6380 5000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8
./bin/kvstore_bench 127.0.0.1 6380 20 1 full
```

## 4. `kvstore_compare_bench`

`kvstore_compare_bench` 不经过网络层，只在进程内比较不同存储实现。它适合观察跳表、`std::map + mutex` 和 WAL 包装本身的相对开销。

命令：

```bash
./bin/kvstore_compare_bench [ops_per_thread] [max_threads] [preload_keys] [mixed|read|read-all|write]
```

用途：

- 对比跳表实现与 `std::map + mutex` 基线
- 对比带 WAL 与不带 WAL 的性能差异
- 使用 `read-all` 观察覆盖 `PUT`、`GET`、`DELETE`、`SCAN` 的读多写少负载

## 5. `verify_protocol_regression.sh`

该脚本用于验证 RESP-like 协议主链路，适合在修改协议解析、服务端读写或命令处理后运行。

命令：

```bash
./scripts/verify_protocol_regression.sh
```

用途：

- 启动 `--no-wal` 服务端
- 执行完整协议回归
- 验证 `PING`、`PUT`、`GET`、`SCAN`、`DEL`、`QUIT`

可覆盖环境变量：

- `HOST`
- `PORT`
- `OPERATIONS`
- `PIPELINE_DEPTH`

## 6. `verify_wal_recovery.sh`

该脚本用于验证 WAL 恢复链路，适合在修改 WAL、KVStore 写路径或服务端启动流程后运行。

命令：

```bash
./scripts/verify_wal_recovery.sh
```

用途：

- 启动启用 WAL 的服务端
- 写入测试数据
- 重启服务端
- 验证恢复后的读取结果

## 7. `run_network_bench.sh`

该脚本会自动编译、启动服务端并运行网络 benchmark，适合批量观察无 WAL、同步 WAL 和周期 WAL 的端到端表现。

命令：

```bash
./scripts/run_network_bench.sh
```

用途：

- 编译项目
- 启动服务端
- 运行单客户端与多客户端网络 benchmark

## 8. `run_compare_bench.sh`

该脚本是 `kvstore_compare_bench` 的批量入口，可以通过环境变量调整线程数、预加载数据量和 workload。

命令：

```bash
./scripts/run_compare_bench.sh
```

用途：

- 批量运行 `kvstore_compare_bench`
- 记录不同 workload 下的对比结果
