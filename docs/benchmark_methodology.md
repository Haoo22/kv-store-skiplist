# KV-Store Benchmark 方法说明

本文档说明当前节点级锁版本中正式 benchmark 的统计口径、参数含义和推荐引用方式。

## 1. 正式 benchmark 工具

当前正式性能数据主要来自两类工具：

- [benchmark_main.cpp](../src/benchmark_main.cpp)
- [compare_benchmark_main.cpp](../src/compare_benchmark_main.cpp)

其中：

- `kvstore_bench` 负责端到端网络 benchmark
- `kvstore_compare_bench` 负责进程内对比实验

## 2. `kvstore_bench` 统计口径

命令格式：

```text
./bin/kvstore_bench [host] [port] [operations] [pipeline_depth] [scenario] [clients]
```

说明：

- `operations`：每个 phase、每个 client 执行的操作次数
- `pipeline_depth`：单批次并行发出的请求数
- `scenario`：`put-get` 或 `full`
- `clients`：并发客户端数

引用建议：

- 串行结果用于说明交互延迟场景
- pipeline 结果用于说明端到端吞吐上限
- 多客户端结果用于说明 aggregate QPS

## 3. `kvstore_compare_bench` 统计口径

命令格式：

```text
./bin/kvstore_compare_bench [ops_per_thread] [max_threads] [preload_keys>=0] [mixed|read|write]
```

统计字段包括：

- `threads`
- `ops/thread`
- `total_ops`
- `seconds`
- `throughput(op/s)`
- `avg_latency(ns)`
- `final_size`

当前 compare benchmark 固定比较：

- `kvstore_no_wal`
- `kvstore_with_wal`
- `std_map_mutex`
- `std_map_mutex_wal`

说明：

- `kvstore_no_wal` / `kvstore_with_wal` 对应当前节点级锁主线实现
- `std_map_mutex` / `std_map_mutex_wal` 是原版红黑树基线
- `*_wal` 使用 compare benchmark 内部补充的 WAL 包装，用于观察追加日志开销下的相对影响

## 4. 当前主比较口径

论文和答辩里建议固定使用下面的主比较：

- 无 WAL：`kvstore_no_wal` vs `std_map_mutex`
- 有 WAL：`kvstore_with_wal` vs `std_map_mutex_wal`

推荐 workload：

- `read`
  说明节点级锁跳表在读多写少、稳定数据规模下的吞吐优势
- `mixed`
  说明更接近日常读写混合场景的表现
- `write`
  说明写多场景下锁竞争与 WAL 开销的影响

## 5. 当前分支的代表性结果

以下结果来自：

```text
./bin/kvstore_compare_bench 20000 8 100000 mixed
./bin/kvstore_compare_bench 20000 8 100000 read
./bin/kvstore_compare_bench 20000 8 100000 write
```

关注 `8` 线程结果：

| workload | `std_map_mutex` | `kvstore_no_wal` | `std_map_mutex_wal` | `kvstore_with_wal` |
| --- | ---: | ---: | ---: | ---: |
| `mixed` | `613721.45 ops/s` | `4596228.24 ops/s` | `222463.99 ops/s` | `199994.80 ops/s` |
| `read` | `919778.04 ops/s` | `7855355.03 ops/s` | `545412.63 ops/s` | `769301.87 ops/s` |
| `write` | `578380.03 ops/s` | `2827052.72 ops/s` | `137791.88 ops/s` | `128888.06 ops/s` |

解读：

- 节点级锁主线在无 WAL 场景下已经明显超过 `std_map_mutex`
- 在 `read` workload 下优势最明显，符合论文“读写锁进一步细粒度化”的叙事
- 引入 WAL 后，吞吐主要受同步写盘开销约束，但主线仍与红黑树基线保持相近或更优的相对表现

## 6. 引用边界

- 正式性能结论只引用 `kvstore_bench` 和 `kvstore_compare_bench`
- 协议回归、WAL 恢复脚本和 demo 页面不作为性能 benchmark 证据
- 当前主线固定表述为“单线程 Reactor + 节点级锁跳表 + WAL”
- 不把当前实现写成“多线程主线服务端”
