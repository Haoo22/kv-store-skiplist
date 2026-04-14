# KV-Store

基于 C++14 实现的轻量级键值存储系统，采用单线程 Reactor 网络模型、跳表内存索引和 WAL 持久化日志，提供简单的文本协议与配套测试、恢复验证和 benchmark 工具。

## 功能概览

- 单线程 `epoll` Reactor 服务端
- 非阻塞 TCP Socket
- 跳表索引，支持 `PUT`、`GET`、`DEL`、`SCAN`
- WAL 追加写与重启恢复
- 文本协议：`PING`、`PUT`、`GET`、`DEL`、`SCAN`、`QUIT`
- 端到端网络 benchmark
- 进程内基线对比 benchmark

## 目录结构

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

核心模块：

- 网络与调度：[src/Server.cpp](src/Server.cpp)、[include/kvstore/Server.hpp](include/kvstore/Server.hpp)
- 协议解析：[src/Protocol.cpp](src/Protocol.cpp)、[include/kvstore/Protocol.hpp](include/kvstore/Protocol.hpp)
- 存储接口：[src/kvstore.cpp](src/kvstore.cpp)、[include/kvstore/kvstore.hpp](include/kvstore/kvstore.hpp)
- 跳表实现：[include/kvstore/SkipList.hpp](include/kvstore/SkipList.hpp)
- WAL：[src/WAL.cpp](src/WAL.cpp)、[include/kvstore/WAL.hpp](include/kvstore/WAL.hpp)
- 测试入口：[tests/test_main.cpp](tests/test_main.cpp)

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

默认会生成以下可执行文件：

- `./bin/kvstore_server`
- `./bin/kvstore_client`
- `./bin/kvstore_bench`
- `./bin/kvstore_compare_bench`
- `./bin/kvstore_tests`

## 快速开始

启动服务端：

```bash
./bin/kvstore_server
```

关闭 WAL 启动：

```bash
./bin/kvstore_server --no-wal
```

手工连接客户端：

```bash
./bin/kvstore_client 127.0.0.1 6380
```

示例命令：

```text
PING
PUT user alice
GET user
SCAN a z
DEL user
QUIT
```

## 运行模式

服务端支持两种常用模式：

- `./bin/kvstore_server --no-wal`
  纯内存模式，适合功能调试和网络 benchmark。
- `./bin/kvstore_server --wal-sync-ms 10`
  启用 WAL，后台按固定周期 `fsync`，适合恢复验证和带 WAL 的性能测试。

也可以使用：

- `./bin/kvstore_server --wal-sync-ms 0`
  每次写入都同步刷盘。

## 测试与验证

单元与集成测试：

```bash
ctest --test-dir build --output-on-failure
./bin/kvstore_tests
```

协议回归：

```bash
./scripts/verify_protocol_regression.sh
```

WAL 恢复验证：

```bash
./scripts/verify_wal_recovery.sh
```

## Benchmark

端到端网络 benchmark：

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

当前对比基线包括：

- `kvstore_no_wal`
- `kvstore_with_wal`
- `std_map_mutex`
- `std_map_mutex_wal`

## 文档索引

- [docs/implementation_overview.md](docs/implementation_overview.md)
- [docs/system_architecture.md](docs/system_architecture.md)
- [docs/request_flow.md](docs/request_flow.md)
- [docs/protocol_reference.md](docs/protocol_reference.md)
- [docs/cli_reference.md](docs/cli_reference.md)
- [docs/validation_workflow.md](docs/validation_workflow.md)
- [docs/wal_recovery_validation.md](docs/wal_recovery_validation.md)
- [docs/benchmark_methodology.md](docs/benchmark_methodology.md)
- [docs/benchmark_summary.md](docs/benchmark_summary.md)
