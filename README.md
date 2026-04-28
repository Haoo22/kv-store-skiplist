# KV-Store

基于 C++14 实现的轻量级键值存储系统，采用单线程 Reactor 网络模型、跳表内存索引和 WAL 持久化日志，提供 RESP-like 长度前缀协议与配套测试、恢复验证和 benchmark 工具。

## 功能概览

- 单线程 `epoll` Reactor 服务端
- 非阻塞 TCP Socket
- 跳表索引，支持 `PUT`、`GET`、`DEL`、`SCAN`
- WAL 追加写、手动 `CHECKPOINT` 与重启恢复
- RESP-like 长度前缀协议：`PING`、`PUT`、`GET`、`DEL`、`SCAN`、`CHECKPOINT`、`QUIT`
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

发送原始 RESP 报文：

```bash
./bin/kvstore_client --raw-resp 127.0.0.1 6380
```

客户端交互示例命令：

```text
PING
PUT user alice
GET user
SCAN a z
DEL user
CHECKPOINT
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

当 WAL 长时间增长后，可以通过客户端发送：

```text
CHECKPOINT
```

服务端会把当前内存状态写入 `data/wal.log.snapshot`，并将 `data/wal.log` 截断为新的空日志文件。

RESP-like 请求示例：

```text
*3
$3
PUT
$4
user
$11
hello world
```

其中 `$11` 表示后续参数长度为 11 字节，因此 value 可以安全包含空格。

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
./bin/kvstore_compare_bench 20000 8 100000 read-all
./bin/kvstore_compare_bench 20000 8 100000 read
./bin/kvstore_compare_bench 20000 8 100000 write
```

其中 `read-all` 为读多写少负载，覆盖 `PUT`、`GET`、`DEL`、`SCAN` 四类存储操作。

当前对比基线包括：

- `kvstore_no_wal`
- `kvstore_with_wal`
- `std_map_mutex`
- `std_map_mutex_wal`

## 文档索引

- [docs/implementation_overview.md](docs/implementation_overview.md)
  实现综述，说明项目目标、核心组件和能力边界。
- [docs/system_architecture.md](docs/system_architecture.md)
  系统架构说明，描述接入层、网络调度层、存储持久化层和验证层的关系。
- [docs/request_flow.md](docs/request_flow.md)
  请求处理流程，说明命令从 TCP 接入到协议解析、存储执行和响应返回的路径。
- [docs/protocol_reference.md](docs/protocol_reference.md)
  RESP-like 协议参考，定义 `PING`、`PUT`、`GET`、`DEL`、`SCAN`、`QUIT` 的请求和响应格式。
- [docs/cli_reference.md](docs/cli_reference.md)
  CLI 参考，列出服务端、客户端、benchmark 和验证脚本的命令格式。
- [docs/validation_workflow.md](docs/validation_workflow.md)
  验证工作流，给出构建、测试、协议回归、WAL 恢复验证和 benchmark 的推荐顺序。
- [docs/wal_recovery_validation.md](docs/wal_recovery_validation.md)
  WAL 恢复验证说明，描述恢复验证脚本的目标、步骤和成功条件。
- [docs/benchmark_methodology.md](docs/benchmark_methodology.md)
  Benchmark 方法说明，解释网络 benchmark、进程内 benchmark、workload 和结果解读方式。
- [docs/benchmark_summary.md](docs/benchmark_summary.md)
  Benchmark 结果汇总，记录代表性进程内对比数据和当前结论。
