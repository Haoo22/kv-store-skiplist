# KV-Store Benchmark 方法说明

本文档说明项目中的性能测试工具、参数含义和结果解读方式。

## 1. Benchmark 工具

项目包含两类 benchmark：

- `kvstore_bench`
  端到端网络 benchmark，覆盖协议解析、事件循环、存储执行与响应写回。
- `kvstore_compare_bench`
  进程内 benchmark，用于对比不同存储实现和 WAL 开销。

这两类工具的目的不同，不应混合解读。

## 2. 网络 Benchmark

命令格式：

```text
./bin/kvstore_bench [host] [port] [operations] [pipeline_depth] [scenario] [clients]
```

参数说明：

- `host`
  服务端地址，默认 `127.0.0.1`
- `port`
  服务端端口，默认 `6380`
- `operations`
  每个阶段、每个客户端执行的操作数
- `pipeline_depth`
  每批次在途请求数
- `scenario`
  `put-get` 或 `full`
- `clients`
  并发客户端数

建议使用方式：

- `pipeline_depth=1`
  观察串行请求路径的吞吐与延迟
- 较大的 `pipeline_depth`
  观察协议层和事件循环的吞吐上限
- `clients > 1`
  观察多客户端下的 aggregate QPS
- `scenario=full`
  做完整协议主链路回归

## 3. 进程内 Benchmark

命令格式：

```text
./bin/kvstore_compare_bench [ops_per_thread] [max_threads] [preload_keys] [mixed|read|read-all|write]
```

输出字段：

- `threads`
- `ops/thread`
- `total_ops`
- `seconds`
- `throughput(op/s)`
- `avg_latency(ns)`
- `final_size`

当前固定比较以下实现：

- `kvstore_no_wal`
- `kvstore_with_wal`
- `std_map_mutex`
- `std_map_mutex_wal`

说明：

- `kvstore_*` 对应当前项目的跳表实现
- `std_map_mutex*` 为 `std::map + mutex` 基线
- `*_wal` 表示带 WAL 包装的版本

## 4. Workload 说明

- `mixed`
  混合读写删除，适合观察常规工作负载。该场景不是严格读多写少，写操作占比较高。
- `read`
  预加载数据后持续读，适合观察读取路径吞吐。该场景为 `90% GET`、`10% PUT`，主要反映读路径扩展性。
- `read-all`
  读多写少，同时覆盖 `PUT`、`GET`、`DELETE` 和 `SCAN`。该场景为 `75% GET`、`10% SCAN`、`10% PUT`、`5% DELETE`，适合展示完整存储接口在读多写少环境下的表现。
- `write`
  连续写入，适合观察写锁竞争与 WAL 开销。该场景为 `80% PUT`、`10% GET`、`10% DELETE`，不代表项目的主要目标负载，但能暴露 WAL 追加路径的上限。

## 5. 环境建议

为了获得更稳定的结果，建议：

- 使用同一台机器重复多次测试
- 在测试前清理旧的 `data/` 目录
- 不与其他高负载任务同时运行
- 网络 benchmark 与协议验证脚本顺序执行，避免端口冲突

## 6. 结果解读

- `kvstore_bench` 结果反映端到端服务能力
- `kvstore_compare_bench` 结果反映存储结构和 WAL 包装的相对开销
- 带 WAL 与不带 WAL 的结果应分别比较
- 不同工具的吞吐数值不应直接横向比较
- 读多写少场景下，`GET` 和 `SCAN` 不写 WAL，因此带 WAL 的结果主要受少量 `PUT`、`DELETE` 影响。
- 写多场景下，大部分请求都需要追加 WAL，日志写入会成为主要瓶颈。
