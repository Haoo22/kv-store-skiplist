# KV-Store

基于跳表的轻量级键值存储引擎，使用 C++14、Linux 原生 Socket API、`epoll` 和 CMake 实现。

当前主线能力：

- 跳表索引：`Put / Get / Delete / Scan`
- WAL 持久化：追加写与重启恢复
- 网络模型：`epoll + 非阻塞 socket + 单线程 Reactor`
- 文本协议：`\r\n` 分隔，支持粘包半包处理
- 配套工具：客户端、正式 benchmark、恢复验证脚本、答辩 demo

当前工程构建标准为 C++14；当前主线 `KVStore` 已采用分片跳表实现细粒度锁控制，每个分片内部仍使用 `std::shared_timed_mutex` 作为读写锁。

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
- 最终数据对照总表：[final_benchmark_summary.md](/home/haoo/code/study/KV-Store/docs/final_benchmark_summary.md)
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
│   ├── final_benchmark_summary.md
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
./scripts/verify_protocol_regression.sh --help
./scripts/run_network_bench.sh --help
./scripts/run_compare_bench.sh --help
./scripts/verify_wal_recovery.sh --help
./scripts/verify_demo_http.sh --help
```

完整参数见 [cli_reference.md](/home/haoo/code/study/KV-Store/docs/cli_reference.md)。

## 5. 验证链路

| 类型 | 命令 | 用途 |
| --- | --- | --- |
| 单元/集成测试 | `ctest --test-dir build --output-on-failure` | 验证跳表、WAL、协议与基础正确性 |
| 协议回归 | `./scripts/verify_protocol_regression.sh` | 启动服务端并验证 `PING/PUT/GET/SCAN/DEL/QUIT` 主链路 |
| WAL 恢复 | `./scripts/verify_wal_recovery.sh` | 端到端验证重启恢复 |
| 网络 benchmark | `./bin/kvstore_bench ...` | 测串行、pipeline、多客户端 aggregate QPS |
| 进程内对比 | `./bin/kvstore_compare_bench ...` | 测不同结构和锁策略 |
| Demo 可达性 | `./scripts/verify_demo_http.sh` | 验证答辩展示页面可访问 |
| Demo 展示 | `python3 demo/defense_demo_server.py` | 答辩展示，不作为正式性能结论 |

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
- `std_map_mutex_wal`
- `skiplist_sharded`
- `skiplist_sharded_wal`
- `std_map_sharded`
- `std_map_sharded_wal`

其中：

- `kvstore_no_wal` / `kvstore_with_wal` 现在就是毕设提交版主线实现
- `skiplist_sharded` / `skiplist_sharded_wal` 仍保留在 compare benchmark 中，作为主线分片索引的结构镜像对照
- `*_wal` 系列用于观察追加日志开销下的相对表现
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
- 不应再把当前主线写成“单一整表锁跳表”

### 7.2 答辩主比较：主线细粒度锁 KVStore vs 原版红黑树基线

当前既然主线已经切到细粒度分片跳表，答辩时建议主比较直接使用主线实现：

- `kvstore_no_wal`
- `std_map_mutex`

测试命令：

```bash
./bin/kvstore_compare_bench 5000 8 100000 read
./bin/kvstore_compare_bench 5000 8 300000 read
```

说明：

- `read` workload 为 `90% GET, 10% PUT`
- 这组测试更接近“先有稳定数据规模，再做读多写少访问”的场景
- `kvstore_no_wal` 对应当前主线细粒度锁版本在不带 WAL 时的结果

结果：

| preload | `std_map_mutex` | `kvstore_no_wal` | 提升倍数 |
| --- | ---: | ---: | ---: |
| `100000` | `619595.15 ops/s` | `1867065.15 ops/s` | `3.01x` |
| `300000` | `410030.13 ops/s` | `2013055.88 ops/s` | `4.91x` |

这组结果支持的答辩口径是：

- 当前毕设主线已经采用细粒度分片跳表
- 在读多写少、稳定数据规模场景下，主线版本已经明显超过原版 `std::map + mutex` 基线

### 7.3 WAL 主比较：主线细粒度锁 KVStore vs 原版红黑树基线

对于带 WAL 的主线版本，建议直接比较：

- `kvstore_with_wal`
- `std_map_mutex_wal`

测试命令：

```bash
./bin/kvstore_compare_bench 5000 8 100000 read
./bin/kvstore_compare_bench 5000 8 300000 read
```

关注 `8` 线程结果：

| preload | `std_map_mutex_wal` | `kvstore_with_wal` | 提升倍数 |
| --- | ---: | ---: | ---: |
| `100000` | `471489.49 ops/s` | `863073.39 ops/s` | `1.83x` |
| `300000` | `341984.65 ops/s` | `948082.91 ops/s` | `2.77x` |

当前可支持的说法是：

- 主线版本在带 WAL 时也已经超过 `std_map_mutex_wal`
- 这说明细粒度锁主线在追加日志开销下仍然保持相对优势
- `skiplist_sharded_wal` 现在更适合作为结构镜像对照，而不是主叙事主体

### 7.4 主线网络 benchmark

当前主线服务端仍然是单线程 Reactor。代表性结果如下：

| 模式 | 命令 | 结果 |
| --- | --- | --- |
| `no-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 19480.95 ops/s` `GET 23533.51 ops/s` |
| `no-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 162200.74 ops/s` `GET 473978.58 ops/s` |
| `no-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=219296.44` |
| `with-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 9427.74 ops/s` `GET 23002.57 ops/s` |
| `with-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 66074.64 ops/s` `GET 191629.62 ops/s` |
| `with-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=82047.62` |

### 7.5 恢复能力补验

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
- 当前毕设主线本身已经是细粒度锁版本，并且在读多写少场景下明显超过原版 `std_map_mutex` 基线

## 10. 文档入口

- 最终数据对照总表：[final_benchmark_summary.md](/home/haoo/code/study/KV-Store/docs/final_benchmark_summary.md)
- Benchmark 方法说明：[benchmark_methodology.md](/home/haoo/code/study/KV-Store/docs/benchmark_methodology.md)
- 论文支撑摘要：[thesis_materials.md](/home/haoo/code/study/KV-Store/docs/thesis_materials.md)
- 答辩讲述提纲：[defense_talk_track.md](/home/haoo/code/study/KV-Store/docs/defense_talk_track.md)
- 系统架构说明：[system_architecture.md](/home/haoo/code/study/KV-Store/docs/system_architecture.md)
- 请求处理流程：[request_flow.md](/home/haoo/code/study/KV-Store/docs/request_flow.md)
- Demo 使用说明：[demo_usage.md](/home/haoo/code/study/KV-Store/docs/demo_usage.md)
- 线程池反例摘要：[thread_pool_findings.md](/home/haoo/code/study/KV-Store/docs/thread_pool_findings.md)
