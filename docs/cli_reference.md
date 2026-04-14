# KV-Store CLI 参考

本文档说明仓库中的可执行文件和脚本入口。

## 1. `kvstore_server`

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

命令：

```bash
./bin/kvstore_client [host] [port]
```

默认值：

- `host=127.0.0.1`
- `port=6380`

用途：

- 手工发送协议命令
- 做交互式冒烟验证

## 3. `kvstore_bench`

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

命令：

```bash
./bin/kvstore_compare_bench [ops_per_thread] [max_threads] [preload_keys] [mixed|read|write]
```

用途：

- 对比跳表实现与 `std::map + mutex` 基线
- 对比带 WAL 与不带 WAL 的性能差异

## 5. `verify_protocol_regression.sh`

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

命令：

```bash
./scripts/run_network_bench.sh
```

用途：

- 编译项目
- 启动服务端
- 运行单客户端与多客户端网络 benchmark

## 8. `run_compare_bench.sh`

命令：

```bash
./scripts/run_compare_bench.sh
```

用途：

- 批量运行 `kvstore_compare_bench`
- 记录不同 workload 下的对比结果
