# KV-Store

基于跳表的轻量级键值存储引擎，使用 C++14、Linux 原生 Socket API、`epoll` 和 CMake 实现。

当前主线能力：

- 跳表索引：`Put / Get / Delete / Scan`
- WAL 持久化：追加写与重启恢复
- 网络模型：`epoll + 非阻塞 socket + 单线程 Reactor`
- 文本协议：`\r\n` 分隔，支持粘包半包处理
- 配套工具：客户端、正式 benchmark、恢复验证脚本、答辩 demo

当前工程构建标准为 C++14；当前主线 `SkipList` 使用 `std::shared_timed_mutex` 作为读写锁。

## 1. 当前口径

答辩和论文建议固定使用下面的口径：

- 主线服务端：单线程 Reactor
- 正式网络 benchmark：`kvstore_bench`
- 进程内对比 benchmark：`kvstore_compare_bench`
- 协议验证：`kvstore_client`、`kvstore_bench ... full`、`packetsender`
- 答辩展示：`demo/defense_demo_server.py + defense_dashboard.html`
- 线程池方案：实验反例，不进入主线

推荐先看：

- 论文支撑摘要：[thesis_materials.md](/home/haoo/code/study/KV-Store/docs/thesis_materials.md)
- Benchmark 方法说明：[benchmark_methodology.md](/home/haoo/code/study/KV-Store/docs/benchmark_methodology.md)
- 系统架构说明：[system_architecture.md](/home/haoo/code/study/KV-Store/docs/system_architecture.md)
- 请求处理流程：[request_flow.md](/home/haoo/code/study/KV-Store/docs/request_flow.md)
- 答辩讲述提纲：[defense_talk_track.md](/home/haoo/code/study/KV-Store/docs/defense_talk_track.md)

## 2. 目录结构

```text
KV-Store/
├── CMakeLists.txt
├── README.md
├── demo/
│   ├── defense_dashboard.html
│   └── defense_demo_server.py
├── docs/
│   ├── benchmark_methodology.md
│   ├── cli_reference.md
│   ├── defense_talk_track.md
│   ├── demo_usage.md
│   ├── experiment_classification.md
│   ├── figure_materials.md
│   ├── module_overview.md
│   ├── protocol_reference.md
│   ├── request_flow.md
│   ├── system_architecture.md
│   ├── thesis_alignment.md
│   ├── thesis_chapter_mapping.md
│   ├── thesis_materials.md
│   ├── thread_pool_findings.md
│   ├── validation_workflow.md
│   └── wal_recovery_validation.md
├── include/kvstore/
├── scripts/
├── src/
├── tests/
└── bin/
```

## 3. 核心模块

### 3.1 存储层

- [SkipList.hpp](/home/haoo/code/study/KV-Store/include/kvstore/SkipList.hpp)
- [kvstore.hpp](/home/haoo/code/study/KV-Store/include/kvstore/kvstore.hpp)
- [kvstore.cpp](/home/haoo/code/study/KV-Store/src/kvstore.cpp)

职责：

- 维护有序键空间
- 提供 `Put / Get / Delete / Scan`
- 对上层隐藏索引与持久化细节

### 3.2 持久化层

- [WAL.hpp](/home/haoo/code/study/KV-Store/include/kvstore/WAL.hpp)
- [WAL.cpp](/home/haoo/code/study/KV-Store/src/WAL.cpp)

职责：

- 写操作追加日志
- 系统重启时重放恢复
- 跳过尾部不完整记录

### 3.3 网络与协议层

- [Server.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Server.hpp)
- [Server.cpp](/home/haoo/code/study/KV-Store/src/Server.cpp)
- [Protocol.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Protocol.hpp)
- [Protocol.cpp](/home/haoo/code/study/KV-Store/src/Protocol.cpp)

职责：

- 使用 `epoll` 处理多连接
- 通过 `LineCodec` 按 `\r\n` 提取命令
- 通过 `CommandProcessor` 执行 `PING / PUT / GET / DEL / SCAN / QUIT`

协议细节见 [protocol_reference.md](/home/haoo/code/study/KV-Store/docs/protocol_reference.md)。

## 4. 构建与常用命令

构建：

```bash
cmake -S . -B build
cmake --build build -j
```

常用帮助：

```bash
./bin/kvstore_server --help
./bin/kvstore_client --help
./bin/kvstore_bench --help
./bin/kvstore_compare_bench --help
./scripts/run_network_bench.sh --help
./scripts/run_compare_bench.sh --help
./scripts/verify_wal_recovery.sh --help
```

完整参数见 [cli_reference.md](/home/haoo/code/study/KV-Store/docs/cli_reference.md)。

## 5. 验证链路

| 类型 | 命令 | 用途 |
| --- | --- | --- |
| 单元/集成测试 | `ctest --test-dir build --output-on-failure` | 验证跳表、WAL、协议与基础正确性 |
| 协议回归 | `./bin/kvstore_bench 127.0.0.1 6380 20 1 full` | 验证 `PING/PUT/GET/SCAN/DEL/QUIT` 主链路 |
| WAL 恢复 | `./scripts/verify_wal_recovery.sh` | 端到端验证重启恢复 |
| 网络 benchmark | `./bin/kvstore_bench ...` | 测串行、pipeline、多客户端 aggregate QPS |
| 进程内对比 | `./bin/kvstore_compare_bench ...` | 测不同结构和锁策略 |
| Demo | `python3 demo/defense_demo_server.py` | 答辩展示，不作为正式性能结论 |

验证工作流见 [validation_workflow.md](/home/haoo/code/study/KV-Store/docs/validation_workflow.md)。

## 6. 启动与使用

### 6.1 服务端

```bash
./bin/kvstore_server
./bin/kvstore_server --no-wal
./bin/kvstore_server --wal-sync-ms 10
```

默认监听：

- `0.0.0.0:6380`

### 6.2 客户端

```bash
./bin/kvstore_client 127.0.0.1 6380
```

示例：

```text
> PUT user alice
OK PUT
> GET user
VALUE alice
> SCAN a z
RESULT 1 user=alice
> DEL user
OK DELETE
> QUIT
BYE
```

### 6.3 正式网络 benchmark

```bash
./bin/kvstore_bench 127.0.0.1 6380 5000 1
./bin/kvstore_bench 127.0.0.1 6380 5000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8
./scripts/run_network_bench.sh
```

### 6.4 进程内对比 benchmark

```bash
./bin/kvstore_compare_bench 5000 8 100000 read
./scripts/run_compare_bench.sh
```

当前 `kvstore_compare_bench` 可对比：

- `kvstore_no_wal`
- `kvstore_with_wal`
- `std_map_mutex`
- `skiplist_sharded`
- `std_map_sharded`

其中：

- `skiplist_sharded` 是答辩阶段用于验证“细粒度锁跳表”的实验版本
- `std_map_sharded` 是更公平的补充对照，不作为答辩主比较口径

## 7. 答辩版实验结果

### 7.1 主线系统是否达成开题核心目标

已经达成：

- 完整数据处理闭环
- 多客户端接入
- WAL 重启恢复
- 正式 benchmark、协议验证、demo 三条链路

不能直接写成已达成：

- 主线不是多线程 worker server
- 线程池方案没有带来端到端收益
- 原版整表锁 `SkipList` 不能写成已经优于 `std::map + mutex`

### 7.2 答辩主比较：细粒度锁跳表 vs 原版红黑树基线

为了和论文里“细粒度锁跳表 vs 原版红黑树基线”的表述一致，答辩时建议主比较使用：

- `skiplist_sharded`
- `std_map_mutex`

测试命令：

```bash
./bin/kvstore_compare_bench 5000 8 100000 read
./bin/kvstore_compare_bench 5000 8 300000 read
```

说明：

- `read` workload 为 `90% GET, 10% PUT`
- 这组测试更接近“先有稳定数据规模，再做读多写少访问”的场景

结果：

| preload | `std_map_mutex` | `skiplist_sharded` | 提升倍数 |
| --- | ---: | ---: | ---: |
| `100000` | `873795.87 ops/s` | `2419362.06 ops/s` | `2.77x` |
| `300000` | `658386.40 ops/s` | `1911372.71 ops/s` | `2.90x` |

这组结果支持的答辩口径是：

- 原版整表锁跳表没有发挥出并发潜力
- 当锁粒度下沉到分片级之后，跳表在读多写少、稳定数据规模场景下可以明显超过原版 `std::map + mutex` 基线

### 7.3 主线网络 benchmark

当前主线服务端仍然是单线程 Reactor。代表性结果如下：

| 模式 | 命令 | 结果 |
| --- | --- | --- |
| `no-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 21908.01 ops/s` `GET 22166.07 ops/s` |
| `no-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 337518.56 ops/s` `GET 260525.22 ops/s` |
| `no-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=256408.86` |
| `with-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 11492.80 ops/s` `GET 23503.42 ops/s` |
| `with-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 69097.99 ops/s` `GET 371471.03 ops/s` |
| `with-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=98319.75` |

### 7.4 恢复能力补验

```bash
./scripts/verify_wal_recovery.sh
```

结果：

```text
WAL recovery verification passed
```

## 8. 答辩展示 Demo

启动：

```bash
python3 demo/defense_demo_server.py
```

访问：

```text
http://127.0.0.1:8765/defense_dashboard.html
```

说明：

- 页面展示协议事件、并发演示和 WAL 恢复过程
- 页面动态图表属于展示型指标
- 正式性能结论仍以 benchmark 为准

## 9. 当前结论

当前最稳的结论是：

- 项目已经形成跳表、WAL、Reactor、文本协议的完整主线
- 主线系统已经具备恢复能力、多客户端接入能力和正式 benchmark 链路
- 线程池方案经实测退化，因此不进入主线
- 如果按照“细粒度锁跳表 vs 原版红黑树基线”来比较，当前 `skiplist_sharded` 已经在读多写少场景下明显超过 `std_map_mutex`

## 10. 文档入口

- Benchmark 方法说明：[benchmark_methodology.md](/home/haoo/code/study/KV-Store/docs/benchmark_methodology.md)
- 论文支撑摘要：[thesis_materials.md](/home/haoo/code/study/KV-Store/docs/thesis_materials.md)
- 答辩讲述提纲：[defense_talk_track.md](/home/haoo/code/study/KV-Store/docs/defense_talk_track.md)
- 系统架构说明：[system_architecture.md](/home/haoo/code/study/KV-Store/docs/system_architecture.md)
- 请求处理流程：[request_flow.md](/home/haoo/code/study/KV-Store/docs/request_flow.md)
- Demo 使用说明：[demo_usage.md](/home/haoo/code/study/KV-Store/docs/demo_usage.md)
- 线程池反例摘要：[thread_pool_findings.md](/home/haoo/code/study/KV-Store/docs/thread_pool_findings.md)
