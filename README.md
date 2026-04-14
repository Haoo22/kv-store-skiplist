# KV-Store

基于跳表的轻量级键值存储引擎，使用 C++14、Linux 原生 Socket API、`epoll` 和 CMake 实现。

本仓库当前固定采用以下主线方案：

- 单线程 Reactor 网络模型
- 节点级锁跳表内存索引
- WAL 持久化与重启恢复
- 文本协议：`PING / PUT / GET / DEL / SCAN / QUIT`
- 对比实验：`kvstore_*` vs `std_map_mutex*`

这个口径与开题报告保持主目标一致，但做了必要收敛：

- 保留 `epoll + 非阻塞 socket + Reactor`
- 保留跳表、WAL、协议解析、性能测试
- 项目内容收敛到最终答辩主线，不再保留额外实验分支和展示型模块

## 1. 项目结构

```text
KV-Store/
├── CMakeLists.txt
├── README.md
├── docs/
├── include/kvstore/
├── scripts/
├── src/
├── tests/
└── bin/
```

核心文件：

- 存储层：`include/kvstore/SkipList.hpp` `include/kvstore/kvstore.hpp` `src/kvstore.cpp`
- 持久化层：`include/kvstore/WAL.hpp` `src/WAL.cpp`
- 网络与协议层：`include/kvstore/Server.hpp` `src/Server.cpp` `include/kvstore/Protocol.hpp` `src/Protocol.cpp`
- 可执行入口：`src/server_main.cpp` `src/client_main.cpp` `src/benchmark_main.cpp` `src/compare_benchmark_main.cpp`
- 测试：`tests/test_main.cpp`

## 2. 当前实现说明

### 2.1 存储层

- 跳表支持 `Put / Get / Delete / Scan`
- 当前跳表内部采用节点级锁控制并发访问
- `Scan` 依赖跳表全局有序键空间完成范围查询

### 2.2 持久化层

- 写操作先追加 WAL，再更新内存索引
- 重启时通过 replay 重建内存状态
- 日志尾部不完整记录会被跳过

### 2.3 网络层

- 服务端采用 `epoll + 非阻塞 socket + 单线程 Reactor`
- 协议以 `\r\n` 为分隔
- `LineCodec` 负责半包与粘包处理

## 3. 构建

```bash
cmake -S . -B build
cmake --build build -j
```

## 4. 常用命令

服务端：

```bash
./bin/kvstore_server
./bin/kvstore_server --no-wal
./bin/kvstore_server --wal-sync-ms 10
```

客户端：

```bash
./bin/kvstore_client 127.0.0.1 6380
```

网络 benchmark：

```bash
./bin/kvstore_bench 127.0.0.1 6380 5000 1
./bin/kvstore_bench 127.0.0.1 6380 5000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8
```

进程内对比 benchmark：

```bash
./bin/kvstore_compare_bench 20000 8 100000 mixed
./bin/kvstore_compare_bench 20000 8 100000 read
./bin/kvstore_compare_bench 20000 8 100000 write
```

当前 `kvstore_compare_bench` 只比较：

- `kvstore_no_wal`
- `kvstore_with_wal`
- `std_map_mutex`
- `std_map_mutex_wal`

## 5. 验证链路

单元/集成测试：

```bash
ctest --test-dir build --output-on-failure
./bin/kvstore_tests
```

协议回归：

```bash
./scripts/verify_protocol_regression.sh
```

WAL 恢复：

```bash
./scripts/verify_wal_recovery.sh
```

批量 benchmark：

```bash
./scripts/run_network_bench.sh
./scripts/run_compare_bench.sh
```

## 6. 推荐阅读

- [docs/system_architecture.md](docs/system_architecture.md)
- [docs/request_flow.md](docs/request_flow.md)
- [docs/protocol_reference.md](docs/protocol_reference.md)
- [docs/benchmark_methodology.md](docs/benchmark_methodology.md)
- [docs/final_benchmark_summary.md](docs/final_benchmark_summary.md)
- [docs/thesis_materials.md](docs/thesis_materials.md)
