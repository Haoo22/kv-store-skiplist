# KV-Store 工具与 CLI 参考

本文档集中说明仓库内主线可执行文件和脚本入口，避免 README 同时承担所有参数细节。

## 1. 服务端

命令：

```bash
./bin/kvstore_server [--no-wal] [--wal-sync-ms <ms>]
```

参数：

- `--no-wal`：关闭 WAL，以纯内存模式运行单线程 Reactor 服务端
- `--wal-sync-ms <ms>`：设置 WAL 的 `fsync` 周期，默认 `10`
- `--wal-sync-ms 0`：表示每次写入都同步刷盘

帮助命令：

```bash
./bin/kvstore_server --help
```

启动后输出示例：

```text
KVStore single-thread Reactor server listening on 0.0.0.0:6380, WAL enabled, path: data/wal.log, sync_ms: 10
```

## 2. 客户端

命令：

```bash
./bin/kvstore_client [host] [port]
```

默认值：

- `host=127.0.0.1`
- `port=6380`

帮助命令：

```bash
./bin/kvstore_client --help
```

用途：

- 手工输入协议命令
- 做最直接的交互式协议冒烟验证

## 3. 网络 benchmark

命令：

```bash
./bin/kvstore_bench [host] [port] [operations] [pipeline_depth] [scenario] [clients]
```

参数：

- `host`：服务端 IPv4 地址，默认 `127.0.0.1`
- `port`：服务端端口，默认 `6380`
- `operations`：每个阶段、每个客户端的请求次数，默认 `1000`
- `pipeline_depth`：批次内并发在途请求数，默认 `1`
- `scenario`：`put-get` 或 `full`，默认 `put-get`
- `clients`：并发 benchmark 客户端数，默认 `1`

帮助命令：

```bash
./bin/kvstore_bench --help
```

典型用途：

- 单客户端吞吐测试
- pipeline 深度测试
- 多客户端 aggregate QPS 测试
- 完整协议主链路回归

示例：

```bash
./bin/kvstore_bench 127.0.0.1 6380 10000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 4
./bin/kvstore_bench 127.0.0.1 6380 20 1 full
```

## 4. 进程内对比 benchmark

命令：

```bash
./bin/kvstore_compare_bench [ops_per_thread] [max_threads] [preload_keys] [mixed|read|write]
```

帮助命令：

```bash
./bin/kvstore_compare_bench --help
```

用途：

- 对比 `kvstore_no_wal`
- 对比 `kvstore_with_wal`
- 对比 `std_map_mutex`
- 对比 `skiplist_sharded`
- 对比 `std_map_sharded`

该工具不经过网络协议层，适合观察数据结构和持久化路径对性能的影响。

## 5. 脚本入口

### 5.1 `run_network_bench.sh`

命令：

```bash
./scripts/run_network_bench.sh
./scripts/run_network_bench.sh --help
```

用途：

- 编译项目
- 启动服务端
- 运行单客户端 benchmark
- 运行基于 `kvstore_bench` 内建 `clients` 参数的多客户端 benchmark

可覆盖环境变量：

- `HOST`
- `PORT`
- `SINGLE_OPS`
- `MULTI_OPS`
- `PIPELINE_DEPTH`
- `MULTI_PIPELINE_DEPTH`
- `SCENARIO`
- `CLIENTS`

### 5.2 `run_compare_bench.sh`

命令：

```bash
./scripts/run_compare_bench.sh
./scripts/run_compare_bench.sh --help
```

用途：

- 运行 `kvstore_compare_bench`
- 便于批量记录对比结果

可覆盖环境变量：

- `OPS_PER_THREAD`
- `MAX_THREADS`
- `PRELOAD_KEYS`
- `WORKLOAD`
- `OUTPUT_FILE`

### 5.3 `verify_wal_recovery.sh`

命令：

```bash
./scripts/verify_wal_recovery.sh
./scripts/verify_wal_recovery.sh --help
```

用途：

- 启动启用 WAL 的服务端
- 写入测试数据
- 重启服务端
- 验证 WAL 恢复后的 `GET` 结果

注意：

- 在当前 Codex 沙箱中，本地 TCP 连接可能受限
- 如果出现 `Operation not permitted`，应在本地终端或沙箱外执行该脚本

## 6. 工具边界

- 正式 benchmark：`kvstore_bench`、`kvstore_compare_bench`
- 协议验证：`kvstore_client`、`kvstore_bench ... full`、`packetsender`
- 答辩展示：`python3 demo/defense_demo_server.py`

这三类工具应在论文和 README 中严格区分，不混用结论。
