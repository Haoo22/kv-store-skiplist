# KV-Store Benchmark 方法说明

本文档说明当前项目中正式 benchmark 的统计口径、参数含义和推荐引用方式。

## 1. 正式 benchmark 工具

当前正式性能数据主要来自两类工具：

- [benchmark_main.cpp](/home/haoo/code/study/KV-Store/src/benchmark_main.cpp)
- [compare_benchmark_main.cpp](/home/haoo/code/study/KV-Store/src/compare_benchmark_main.cpp)

其中：

- `kvstore_bench` 负责端到端网络 benchmark
- `kvstore_compare_bench` 负责进程内对比实验

## 2. `kvstore_bench` 统计口径

命令格式：

```text
./bin/kvstore_bench [host] [port] [operations] [pipeline_depth] [scenario] [clients]
```

### 2.1 参数含义

- `operations`：每个 phase、每个 client 执行的操作次数
- `pipeline_depth`：单批次并行发出的请求数
- `scenario`：
  - `put-get`
  - `full`
- `clients`：并发客户端数

### 2.2 单客户端模式

当 `clients=1` 时，输出按 phase 展示，例如：

```text
PUT      ops=1000 pipeline=1 elapsed=... throughput=... avg_latency=...
GET      ops=1000 pipeline=1 elapsed=... throughput=... avg_latency=...
```

这里的统计口径是：

- `throughput`：当前 phase 的 `operations / elapsed`
- `avg_latency`：当前 phase 的平均耗时

### 2.3 多客户端模式

当 `clients>1` 时，输出整组 aggregate QPS，例如：

```text
clients=4 scenario=put-get pipeline=8 ops_per_client=500 total_requests=4000 wall_seconds=... aggregate_qps=...
```

这里的统计口径是：

- `ops_per_client`：每个客户端每个 phase 的操作次数
- `total_requests = clients * operations * phase_count`
- `aggregate_qps = total_requests / wall_seconds`

注意：

- `QUIT` 不计入 aggregate 统计
- `full` 场景的 `phase_count` 为 `5`
- `put-get` 场景的 `phase_count` 为 `2`

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

用途：

- 对比 `kvstore_no_wal`
- 对比 `kvstore_with_wal`
- 对比 `std_map_mutex`
- 对比 `std_map_mutex_wal`
- 对比 `std_map_sharded`
- 对比 `std_map_sharded_wal`
- 对比 `skiplist_sharded` 这种“通过 key 分片提升锁粒度”的实验版本
- 对比 `skiplist_sharded_wal`

当前实现边界：

- `skiplist_sharded`、`std_map_sharded` 及其 `*_wal` 版本，都是进程内实验性对照，不是主线服务端结构
- 其中 `*_wal` 使用的是 compare benchmark 内部补充的实验性 WAL 包装，不等于主线服务端已经切换到该实现
- 因此带 WAL 的结论可以用于回答“索引结构在追加日志开销下的相对表现”，但不应和主线网络 benchmark 直接混写

## 4. 推荐实验引用方式

论文中建议这样区分：

- 网络吞吐、pipeline 上限、多客户端 aggregate QPS：引用 `kvstore_bench`
- 数据结构和持久化路径对比：引用 `kvstore_compare_bench`
- 协议正确性验证：不要把它写成 benchmark 结果
- demo 图表：不要把它写成正式实验数据

## 5. 推荐表述

适合写进论文的口径包括：

- “单客户端串行测试更接近交互延迟场景”
- “pipeline 模式更接近服务端吞吐上限测试”
- “多客户端模式使用整组 wall clock 时间计算 aggregate QPS”
- “进程内对比实验主要用于分析数据结构与持久化路径差异”

## 6. 不建议的表述

- 不要把 demo 中的动态图表当作 benchmark 结果
- 不要把协议回归命令当作性能结论
- 不要混淆单客户端 phase 吞吐和多客户端 aggregate QPS

## 7. 2026-04-09 对照开题预期的实测结果

下面这组结果用于回答一个更具体的问题：当前主线实现是否已经达到开题报告里“完整闭环、支持并发接入、具备恢复能力、可做性能验证”的预期。

### 7.1 网络 benchmark

测试环境与口径：

- 主线服务端：单线程 Reactor
- `no-wal`：`./bin/kvstore_server --no-wal`
- `with-wal`：`./bin/kvstore_server --wal-sync-ms 10`
- benchmark 工具：`./bin/kvstore_bench`

| 模式 | 命令 | 结果 |
| --- | --- | --- |
| `no-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 21908.01 ops/s` `GET 22166.07 ops/s` |
| `no-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 337518.56 ops/s` `GET 260525.22 ops/s` |
| `no-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=256408.86` |
| `with-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 11492.80 ops/s` `GET 23503.42 ops/s` |
| `with-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 69097.99 ops/s` `GET 371471.03 ops/s` |
| `with-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=98319.75` |

### 7.2 进程内对比 benchmark

测试命令：

```text
./bin/kvstore_compare_bench 5000 8 10000 mixed
./bin/kvstore_compare_bench 5000 8 10000 read
```

关注 `8` 线程结果：

| workload | `kvstore_no_wal` | `kvstore_with_wal` | `std_map_mutex` |
| --- | --- | --- | --- |
| `mixed` | `303610.76 ops/s` | `162551.06 ops/s` | `865979.50 ops/s` |
| `read` | `443724.76 ops/s` | `286471.41 ops/s` | `1207779.54 ops/s` |

### 7.3 恢复能力补验

测试命令：

```text
./scripts/verify_wal_recovery.sh
```

结果：

```text
WAL recovery verification passed
```

### 7.4 对照开题报告的判断

已经达到的预期：

- 完整数据处理闭环已经形成
- `epoll + 非阻塞 socket + Reactor` 已可支撑多客户端接入
- WAL 恢复能力已经通过端到端脚本验证
- benchmark、协议验证、demo 三条链路已经齐全

只能部分达到的预期：

- “高性能”可以成立，但应收敛到“当前原型已经具备较高吞吐”，不要扩写成绝对领先或工业级结论

当前不能按开题原话写成已达成的部分：

- 当前主线不是多线程 worker server
- 线程池方案没有带来端到端收益，反而退化
- 当前 `SkipList` 实现未在进程内对比实验中跑赢 `std_map_mutex`

推荐结论口径：

- 当前项目已经完成轻量级 KV-Store 的主线实现、恢复能力、并发接入能力和正式 benchmark 链路
- 但多线程并行化方案未达到预期，且当前跳表实现尚未在对比实验中体现出优于 `std::map + mutex` 的性能优势

## 8. 2026-04-09 锁粒度实验：分片跳表

为验证“当前 `SkipList` 是否主要受整表锁粒度限制”，在 [compare_benchmark_main.cpp](/home/haoo/code/study/KV-Store/src/compare_benchmark_main.cpp) 中新增了一个仅用于对比压测的实验版本：

- `skiplist_sharded`

它的做法不是修改主线 `KVStore`，而是把 key 按哈希分布到多个 `SkipList` 分片中，让不同 key 的操作尽量落到不同锁上。

这个版本当前只用于进程内对比实验，不代表主线服务端已经切换到该设计。

### 8.1 答辩主比较：读多写少、预加载 100k

命令：

```text
./bin/kvstore_compare_bench 5000 8 100000 read
```

关注 `8` 线程结果：

| benchmark | throughput |
| --- | ---: |
| `std_map_mutex` | `873795.87 ops/s` |
| `skiplist_sharded` | `2419362.06 ops/s` |

说明：

- 这组是当前答辩主比较口径
- 比较对象是“原版红黑树基线”与“细粒度锁跳表实验版”

### 8.2 答辩主比较：读多写少、预加载 300k

命令：

```text
./bin/kvstore_compare_bench 5000 8 300000 read
```

关注 `8` 线程结果：

| benchmark | throughput |
| --- | ---: |
| `std_map_mutex` | `658386.40 ops/s` |
| `skiplist_sharded` | `1911372.71 ops/s` |

### 8.3 当前判断

- 这组结果强烈说明，当前原版 `SkipList` 的主要问题之一确实是锁粒度过粗
- 一旦把 key 分散到多个跳表分片，读多写少场景下吞吐会明显上升
- 按当前答辩主比较口径，`skiplist_sharded` 已经明显超过 `std_map_mutex`

### 8.4 WAL 补测：`std_map_mutex_wal` vs `skiplist_sharded_wal`

为补齐“带 WAL 后还能不能保持优势”这个问题，当前在 compare benchmark 中增加了实验性 WAL 包装对照项：

- `std_map_mutex_wal`
- `skiplist_sharded_wal`

命令：

```text
./bin/kvstore_compare_bench 5000 8 100000 read
./bin/kvstore_compare_bench 5000 8 300000 read
```

关注 `8` 线程结果：

| preload | `std_map_mutex_wal` | `skiplist_sharded_wal` | 提升倍数 |
| --- | ---: | ---: | ---: |
| `100000` | `482484.93 ops/s` | `854925.36 ops/s` | `1.77x` |
| `300000` | `416153.30 ops/s` | `917692.71 ops/s` | `2.21x` |

当前判断：

- 在这组进程内 WAL 对照里，`skiplist_sharded_wal` 也已经超过 `std_map_mutex_wal`
- 这说明加上 WAL 之后，细粒度锁跳表相对原版红黑树基线的优势仍然成立
- 但这组结果仍然属于 compare benchmark 的实验性 WAL 包装对照，不应直接写成主线网络服务端结论

### 8.5 补充对照：`std_map_sharded`

为了区分“数据结构收益”和“锁粒度收益”，当前程序中还保留了一个补充对照：

- `std_map_sharded`

它不作为答辩主比较口径，但可用于内部分析。

以 `./bin/kvstore_compare_bench 5000 8 100000 read` 的 `8` 线程结果为例：

| benchmark | throughput |
| --- | ---: |
| `std_map_mutex` | `873795.87 ops/s` |
| `std_map_sharded` | `4430013.92 ops/s` |
| `skiplist_sharded` | `2419362.06 ops/s` |

这说明：

- 当前锁粒度优化本身带来的收益非常大
- 当前实现下，`std_map_sharded` 仍快于 `skiplist_sharded`
- 因此“跳表优势”在当前阶段应收敛表述为：细粒度锁跳表已经显著优于原版红黑树基线，而不是声称它已经优于所有分片化 `std::map` 实现
