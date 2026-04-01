# KV-Store

基于跳表的高性能键值存储引擎，使用现代 C++、Linux 原生 Socket API、`epoll` 和 CMake 实现。当前版本已经完成跳表索引、WAL 持久化恢复、基于 Reactor 的服务端、交互式客户端和基础压力测试工具。

## 1. 项目概述

本项目面向毕业设计场景，核心目标是实现一个具备以下特性的轻量级键值存储系统：

- 内存索引采用跳表，支持 `Put`、`Get`、`Delete`、`Scan`
- 持久化采用预写日志 WAL，支持追加写和重启恢复
- 网络模型采用 `epoll + 非阻塞 socket + 单线程 Reactor`
- 协议采用 `\r\n` 结尾的文本命令，能够处理 TCP 粘包和半包
- 提供配套客户端和基础压测工具，便于实验展示和性能分析

## 2. 目录结构

```text
KV-Store/
├── CMakeLists.txt
├── README.md
├── include/kvstore/
│   ├── Protocol.hpp
│   ├── Server.hpp
│   ├── SkipList.hpp
│   ├── WAL.hpp
│   └── kvstore.hpp
├── src/
│   ├── Protocol.cpp
│   ├── Server.cpp
│   ├── WAL.cpp
│   ├── benchmark_main.cpp
│   ├── client_main.cpp
│   ├── kvstore.cpp
│   └── server_main.cpp
├── tests/
│   └── test_main.cpp
└── bin/
```

## 3. 核心模块说明

### 3.1 跳表索引

- 文件：[SkipList.hpp](/home/haoo/code/study/KV-Store/include/kvstore/SkipList.hpp)
- 采用泛型模板实现，支持不同键值类型
- 使用随机层级生成算法，平均时间复杂度为 `O(log N)`
- 支持范围查询 `Scan(start, end)`，适合有序键遍历
- 节点使用智能指针管理，符合 RAII 风格

### 3.2 WAL 持久化

- 文件：[WAL.hpp](/home/haoo/code/study/KV-Store/include/kvstore/WAL.hpp)、[WAL.cpp](/home/haoo/code/study/KV-Store/src/WAL.cpp)
- 采用 append-only 二进制日志格式
- 写入顺序为：先写 WAL，再更新内存索引
- 每条记录包含操作类型、键长度、值长度和校验信息
- 系统重启时自动重放日志恢复内存数据
- 对日志尾部不完整记录进行跳过处理，避免异常断电后恢复失败

### 3.3 网络层与 Reactor

- 文件：[Server.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Server.hpp)、[Server.cpp](/home/haoo/code/study/KV-Store/src/Server.cpp)
- 使用 `epoll` 进行 I/O 多路复用
- 使用非阻塞监听 socket 和连接 socket
- 使用单线程 Reactor 循环处理连接、读写事件和命令响应
- 协议解析与业务处理分离，便于维护和测试

### 3.4 协议层

- 文件：[Protocol.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Protocol.hpp)、[Protocol.cpp](/home/haoo/code/study/KV-Store/src/Protocol.cpp)
- `LineCodec` 负责按照 `\r\n` 提取完整命令
- `CommandProcessor` 负责命令分发与响应生成
- 已支持命令：
  - `PING`
  - `PUT <key> <value>`
  - `GET <key>`
  - `DEL <key>`
  - `SCAN <start> <end>`
  - `QUIT`

## 4. 构建方法

在项目根目录执行：

```bash
cmake -S . -B build
cmake --build build -j
```

编译后生成的主要可执行文件位于 `bin/`：

- `kvstore_server`
- `kvstore_client`
- `kvstore_bench`
- `kvstore_tests`

## 5. 测试方法

执行单元/集成测试：

```bash
ctest --test-dir build --output-on-failure
```

当前测试覆盖了以下内容：

- 跳表插入、更新、删除、范围查询
- WAL 追加写、重放恢复
- 截断日志尾部容错
- 协议分包、命令解析和响应正确性

## 6. 启动与使用

### 6.1 启动服务端

```bash
./bin/kvstore_server
```

默认监听：

- 地址：`0.0.0.0`
- 端口：`6380`

启动后会看到类似输出：

```text
KVStore server listening on 0.0.0.0:6380, WAL path: data/wal.log
```

### 6.2 启动客户端

```bash
./bin/kvstore_client 127.0.0.1 6380
```

示例交互：

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

### 6.3 运行基础压测

```bash
./bin/kvstore_bench 127.0.0.1 6380 1000
./bin/kvstore_bench 127.0.0.1 6380 10000 64
```

参数含义：

- 第 1 个参数：服务端 IP
- 第 2 个参数：服务端端口
- 第 3 个参数：每个阶段的操作次数
- 第 4 个参数：可选，pipeline 深度，默认 `1`

输出示例：

```text
PUT      ops=1000 elapsed=3.8123s throughput=262.31 ops/s avg_latency=3812.30 us
GET      ops=1000 elapsed=0.0561s throughput=17825.31 ops/s avg_latency=56.10 us
```

### 6.4 运行 KV-Store 与 std::map 并发对比压测

```bash
./bin/kvstore_compare_bench 500 8
./bin/kvstore_compare_bench 250 16
./scripts/run_compare_bench.sh
```

说明：

- 第 1 个参数：每线程操作数 `ops_per_thread`
- 第 2 个参数：最大线程数 `max_threads`
- 基准程序会自动测试 `1/2/4/8/.../max_threads`
- 对比对象包括：
  - `kvstore_no_wal`
  - `kvstore_with_wal`
  - `std_map_mutex`

脚本方式：

- [run_compare_bench.sh](/home/haoo/code/study/KV-Store/scripts/run_compare_bench.sh) 会先执行 `cmake --build build -j`，再运行对比压测
- 默认参数等价于 `./bin/kvstore_compare_bench 500 8`
- 可通过环境变量覆盖：

```bash
OPS_PER_THREAD=1000 MAX_THREADS=16 ./scripts/run_compare_bench.sh
OUTPUT_FILE=results/compare.txt ./scripts/run_compare_bench.sh
```

混合负载比例：

- 约 40% `PUT`
- 约 50% `GET`
- 约 10% `DELETE/PUT+GET`

## 7. 并发压测结果

以下数据为 2026-04-01 在当前项目版本下通过 [compare_benchmark_main.cpp](/home/haoo/code/study/KV-Store/src/compare_benchmark_main.cpp) 实测得到。

### 7.1 数据集 A

命令：

```bash
./bin/kvstore_compare_bench 500 8
```

结果：

```text
benchmark             threads   ops/thread    total_ops     seconds     throughput(op/s)  avg_latency(ns)   final_size
kvstore_no_wal        1         500           500           0.0007      750694.39         1332.10           200
kvstore_with_wal      1         500           500           1.9841      252.01            3968173.22        200
std_map_mutex         1         500           500           0.0003      1954193.70        511.72            200
kvstore_no_wal        2         500           1000          0.0017      601210.84         1663.31           400
kvstore_with_wal      2         500           1000          3.7595      265.99            3759535.01        400
std_map_mutex         2         500           1000          0.0006      1735237.47        576.29            400
kvstore_no_wal        4         500           2000          0.0045      441081.44         2267.16           800
kvstore_with_wal      4         500           2000          8.1053      246.75            4052643.10        800
std_map_mutex         4         500           2000          0.0012      1692276.45        590.92            800
kvstore_no_wal        8         500           4000          0.0106      377576.49         2648.47           1600
kvstore_with_wal      8         500           4000          16.2892     245.56            4072292.76        1600
std_map_mutex         8         500           4000          0.0029      1393296.16        717.72            1600
```

### 7.2 数据集 B

命令：

```bash
./bin/kvstore_compare_bench 250 16
```

结果：

```text
benchmark             threads   ops/thread    total_ops     seconds     throughput(op/s)  avg_latency(ns)   final_size
kvstore_no_wal        1         250           250           0.0005      494931.90         2020.48           100
kvstore_with_wal      1         250           250           0.9741      256.66            3896223.76        100
std_map_mutex         1         250           250           0.0002      1166697.78        857.12            100
kvstore_no_wal        2         250           500           0.0011      453457.16         2205.28           200
kvstore_with_wal      2         250           500           2.0123      248.47            4024554.82        200
std_map_mutex         2         250           500           0.0006      852326.00         1173.26           200
kvstore_no_wal        4         250           1000          0.0028      352306.20         2838.44           400
kvstore_with_wal      4         250           1000          4.0599      246.31            4059919.27        400
std_map_mutex         4         250           1000          0.0006      1695748.76        589.71            400
kvstore_no_wal        8         250           2000          0.0048      415119.48         2408.95           800
kvstore_with_wal      8         250           2000          8.0382      248.81            4019101.35        800
std_map_mutex         8         250           2000          0.0015      1303543.03        767.14            800
kvstore_no_wal        16        250           4000          0.0131      304451.07         3284.60           1600
kvstore_with_wal      16        250           4000          15.6657     255.34            3916415.58        1600
std_map_mutex         16        250           4000          0.0042      959867.92         1041.81           1600
```

### 7.3 当前最新结果

以下为当前代码版本通过 [run_compare_bench.sh](/home/haoo/code/study/KV-Store/scripts/run_compare_bench.sh) 实测得到的代表性结果：

```text
benchmark             threads   ops/thread    total_ops     seconds     throughput(op/s)  avg_latency(ns)   final_size
kvstore_no_wal        1         500           500           0.0005      926509.28         1079.32           200
kvstore_with_wal      1         500           500           1.0303      485.29            2060633.08        200
std_map_mutex         1         500           500           0.0003      1916296.18        521.84            200
kvstore_no_wal        2         500           1000          0.0024      409297.60         2443.21           400
kvstore_with_wal      2         500           1000          2.0044      498.89            2004436.17        400
std_map_mutex         2         500           1000          0.0006      1580203.21        632.83            400
kvstore_no_wal        4         500           2000          0.0041      492585.36         2030.11           800
kvstore_with_wal      4         500           2000          4.2242      473.46            2112120.83        800
std_map_mutex         4         500           2000          0.0014      1458629.62        685.58            800
kvstore_no_wal        8         500           4000          0.0109      367997.13         2717.41           1600
kvstore_with_wal      8         500           4000          8.4606      472.78            2115159.56        1600
std_map_mutex         8         500           4000          0.0026      1514205.14        660.41            1600
```

### 7.4 优化过程记录

2026-04-01 并发控制重构：

- 去掉 `KVStore` 外层总锁
- `SkipList` 改为 `shared_timed_mutex` 读写锁
- `WAL` 追加写单独加锁

阶段结论：

- `with_wal` 吞吐有提升
- `no_wal` 仍明显低于 `std_map_mutex`

2026-04-01 网络路径优化：

- 服务端与客户端 socket 开启 `TCP_NODELAY`
- 服务端改为读完命令后优先直接写回，仅在 `EAGAIN` 时注册 `EPOLLOUT`
- benchmark/client 的响应读取从逐字节 `read` 改为缓冲读取

阶段结论：

- 端到端 `no_wal` 网络压测从约 `500 qps` 提升到约 `5万+ qps`
- 说明此前网络请求响应路径才是端到端测试中的主瓶颈

2026-04-01 WAL 刷盘策略优化：

- 将 WAL 从“每次写后立即 `fsync`”改为“可配置刷盘间隔”
- 服务端默认使用 `--wal-sync-ms 10`
- 保留 `--wal-sync-ms 0` 作为每次同步刷盘模式

阶段结论：

- `with_wal` 端到端网络吞吐大幅提升
- 当前 durability 与吞吐的权衡变为可配置

2026-04-01 SkipList 节点链路优化：

- 将跳表节点前向链路从 `shared_ptr` 改为 `unique_ptr + Node*`
- 目标是降低 `kvstore_no_wal` 热点路径中的引用计数开销

阶段结论：

- `kvstore_no_wal` 较之前版本有明显提升
- 说明 `shared_ptr` 的热点开销判断成立

2026-04-01 Pipeline Benchmark 压测补充：

- 为 [benchmark_main.cpp](/home/haoo/code/study/KV-Store/src/benchmark_main.cpp) 增加 `pipeline_depth` 参数
- 支持批量发送请求、批量接收响应，用于测量端到端服务端吞吐上限

测试命令：

```bash
./bin/kvstore_bench 127.0.0.1 6380 10000 1
./bin/kvstore_bench 127.0.0.1 6380 10000 64
```

端到端结果：

```text
no_wal, pipeline=1
PUT  19837.57 ops/s
GET  20877.61 ops/s

no_wal, pipeline=64
PUT  546776.75 ops/s
GET  608050.59 ops/s

with_wal(sync_ms=10), pipeline=1
PUT   9389.67 ops/s
GET  19974.59 ops/s

with_wal(sync_ms=10), pipeline=64
PUT  131589.34 ops/s
GET  385104.17 ops/s
```

阶段结论：

- 端到端 `10w qps` 目标在 pipeline 模式下已经达到
- 串行 benchmark 更接近交互延迟测试，pipeline benchmark 更接近服务端吞吐上限测试

当前综合结论：

- `kvstore_no_wal` 较之前版本有明显提升，说明 `shared_ptr` 的热点开销判断是成立的
- `kvstore_with_wal` 仍稳定在约 `470~500 ops/s`，说明当前主瓶颈仍是同步 WAL 路径
- `std_map_mutex` 依然高于 `kvstore_no_wal`，后续仍可继续优化锁与节点布局

### 7.5 结果分析摘要

- `std_map_mutex` 在当前实现下整体吞吐高于 `kvstore_no_wal`
- `kvstore_with_wal` 在进程内对比压测下当前稳定在约 `470~500 ops/s`
- WAL 的主要瓶颈仍来自刷盘路径，只是现在已支持通过刷盘间隔进行权衡
- `kvstore_no_wal` 在线程数增加后仍未达到 `std_map_mutex` 水平，说明当前版本仍受锁与节点布局常数开销影响

### 7.6 数据规模与负载实验

为验证 `SkipList` 是否会在更大数据规模和偏读场景下反超 `std::map`，对 [compare_benchmark_main.cpp](/home/haoo/code/study/KV-Store/src/compare_benchmark_main.cpp) 增加了：

- `preload_keys`：预填充键数量
- `workload`：`mixed/read/write`

示例命令：

```bash
./bin/kvstore_compare_bench 50000 8 100000 read
./bin/kvstore_compare_bench 50000 8 300000 read
./bin/kvstore_compare_bench 50000 8 100000 mixed
```

读密集场景（`90% GET, 10% PUT`）下的 `8` 线程结果：

```text
preload=10000
kvstore_no_wal  413791.87 ops/s
std_map_mutex   998355.36 ops/s

preload=100000
kvstore_no_wal  394564.46 ops/s
std_map_mutex   916509.13 ops/s

preload=200000
kvstore_no_wal  338968.57 ops/s
std_map_mutex   770316.47 ops/s

preload=300000
kvstore_no_wal  342112.01 ops/s
std_map_mutex   789707.82 ops/s
```

混合场景（`40% PUT, 50% GET, 10% DELETE/PUT+GET`）在 `preload=100000, 8` 线程下：

```text
kvstore_no_wal  249540.59 ops/s
std_map_mutex   628435.53 ops/s
```

实验结论：

- 到当前测试规模为止，`SkipList` 尚未在进程内对比压测中反超 `std_map_mutex`
- 随着数据量增大，两者吞吐都会下降，但目前没有出现 `SkipList` 越跑越占优的趋势
- 说明当前瓶颈仍主要来自实现常数与锁开销，而不是数据结构理论复杂度本身
- 在 `preload=500000` 的读密集实验中，曾出现过一次异常退出（`exit 139`）

### 7.7 大数据量稳定性问题排查

对 `preload=500000, workload=read` 的异常退出进行复现与定位后，确认问题并非读写竞争，而是 `SkipList` 在大数据量析构时的递归释放导致的栈溢出。

复现命令：

```bash
./bin/kvstore_compare_bench 50000 8 500000 read
```

问题原因：

- `SkipList` 当前通过 `unique_ptr<Node> next` 串起 level 0 的所有权链
- 在 benchmark 结束后，局部 `KVStore` / `SkipList` 析构时会沿 `next` 递归析构整条节点链
- 当节点规模达到 `50w+` 时，递归深度足以触发栈溢出

修复方式：

- 为 `SkipList` 增加显式析构函数
- 将节点释放改为迭代式 `ReleaseAllNodes()`，避免深链递归销毁

修复后重新测试：

```text
Workload read: 90% GET, 10% PUT
Preloaded keys: 500000

8 threads
kvstore_no_wal   382900.39 ops/s
kvstore_with_wal 266878.65 ops/s
std_map_mutex    834123.06 ops/s
```

结论：

- `preload=500000` 的读密集压测已可稳定完成
- 本次修复解决的是大数据量稳定性问题，不改变当前“`std_map_mutex` 仍快于 `SkipList`”的整体判断

### 7.8 问题追踪记录

这一节用于记录优化过程中的问题链路，便于后续回顾思考过程，也便于展示给他人查看每一步判断是如何形成的。

问题 1：`SkipList` 的高并发优势为什么没有体现出来？

- 初始判断：可能是并发程度、数据规模和负载模式还不够，导致测试更偏向实现常数开销，而不是数据结构优势
- 对应动作：为对比 benchmark 增加 `preload_keys` 和 `workload(read/mixed/write)` 控制项
- 实验结果：到 `preload=300000`、`read(90% GET)` 为止，`SkipList` 仍未反超 `std_map_mutex`
- 当前结论：现阶段瓶颈仍主要来自实现常数和锁开销，而不是理论复杂度优势尚未被激发

问题 2：`preload=500000` 时为什么会崩溃？

- 初始现象：`./bin/kvstore_compare_bench 50000 8 500000 read` 出现 `exit 139`
- 排查方式：复现、缩小线程范围、使用 ASan 定位
- 定位结果：不是并发数据竞争，而是 `SkipList` 通过 `unique_ptr<Node> next` 形成长链后，在大数据量析构时递归释放导致栈溢出
- 修复方式：增加显式析构并采用迭代式 `ReleaseAllNodes()`
- 修复后结论：`preload=500000` 已可稳定完成，不再出现栈溢出

问题 3：网络层是否导致实际运行接近单线程？

- 代码结论：是
- 原因：当前 `kvstore_server` 采用单线程 Reactor，`epoll_wait -> HandleConnectionEvent -> Protocol::Execute -> KVStore` 整条链都在同一个线程同步执行
- 含义：`epoll` 解决的是“一个线程管理多个连接”，但并没有把请求处理并行化

问题 4：既然 server 是单线程，是否说明锁细粒度优化应该有明显收益？

- 初始猜想：如果请求执行本来就是单线程，那 `SkipList` 上的读写锁可能只是额外开销
- 实验动作：曾短暂做过“关闭索引锁”的服务端实验版本
- 实验结果：端到端 `no_wal` 与 `with_wal` QPS 没有提升，反而略低
- 当前结论：在当前代码中，单线程事实是成立的，但 `SkipList` 这把索引锁并不是当前端到端吞吐的主瓶颈

下一步判断：

- 继续优化锁细粒度之前，应该先解决真正更重的热点：协议解析、字符串处理、单线程执行模型本身
- 锁细粒度优化仍值得继续验证，但应建立在“协议路径和执行模型已基本稳定”的前提下再做，否则测试结果容易被更大的上层开销掩盖

## 8. 文本协议说明

所有请求与响应均以 `\r\n` 结束。

### 请求格式

```text
PING\r\n
PUT key value\r\n
GET key\r\n
DEL key\r\n
SCAN start end\r\n
QUIT\r\n
```

### 响应格式

```text
PONG\r\n
OK PUT\r\n
OK UPDATE\r\n
VALUE <value>\r\n
OK DELETE\r\n
NOT_FOUND\r\n
RESULT <count> k1=v1 k2=v2 ...\r\n
BYE\r\n
ERROR ...\r\n
```

## 9. 当前实现的技术特点

- 使用 RAII 管理文件描述符和资源释放
- 使用智能指针管理核心对象生命周期
- 内存索引与持久化解耦，便于后续扩展 SSTable 或 LSM
- 协议层与事件循环解耦，便于单独测试
- 工程结构清晰，便于继续扩展多线程、配置文件和更完整的压测框架

## 10. 复杂对象序列化约束与接入说明

当前版本的 KV-Store 只直接支持：

- `std::string -> std::string` 键值存储
- 文本协议下的 `PUT <key> <value>` 命令
- WAL 中对字符串键值的持久化记录

这意味着系统本身不负责复杂 C++ 对象的自动序列化，也没有内建对象映射框架。复杂对象如果要存入本系统，必须先由业务层自行序列化成字符串或字节串，再写入 KV-Store。

### 10.1 当前没有内建支持的对象类型

以下内容不应直接序列化后写入本系统：

- 裸指针、智能指针中的地址值
- 引用、函数指针、虚函数表相关运行时状态
- `std::mutex`、`std::thread`、条件变量等同步原语
- 文件描述符、socket、进程句柄等操作系统资源
- 依赖当前进程地址空间的对象内部状态

原因很直接：这些内容在进程重启、机器重启或跨机器传输后都没有稳定语义，存储它们的“值”通常没有可恢复意义。

### 10.2 推荐的接入方式

推荐使用下面的业务层流程：

1. 将业务对象转换为稳定格式
2. 把结果编码成 `std::string`
3. 调用 `Put(key, serialized_value)` 写入
4. 读取时用 `Get(key, &value)` 拿到字符串
5. 再由业务层反序列化为对象

推荐的稳定格式包括：

- JSON
- Protocol Buffers
- MessagePack
- 自定义二进制结构

### 10.3 一个简单的接入示例

假设业务对象为：

```cpp
struct User {
    std::string name;
    int age;
};
```

一种简单做法是业务层自行实现：

```cpp
std::string SerializeUser(const User& user);
User DeserializeUser(const std::string& data);
```

然后以如下方式使用：

```cpp
User user {"alice", 20};
std::string encoded = SerializeUser(user);
store.Put("user:1001", encoded);

std::string raw;
if (store.Get("user:1001", &raw)) {
    User restored = DeserializeUser(raw);
}
```

### 10.4 设计约束建议

如果你要把本项目写进毕业设计，建议在论文或接口文档中明确声明：

- 存储引擎层只保证字符串键值的存储与恢复
- 对象序列化与反序列化由上层业务负责
- 不支持直接持久化进程态资源和内存地址
- 复杂对象的兼容性、版本迁移和字段演进由序列化协议负责

这样接口边界会非常清楚，也更符合 KV 存储引擎的职责划分。

## 11. 可用于毕业设计论文的实验设计建议

可以从以下几个方向组织实验章节：

### 11.1 功能正确性实验

- 测试 `Put/Get/Delete/Scan` 是否正确
- 测试服务端是否能处理多个连续命令
- 测试 WAL 是否能在重启后恢复数据
- 测试异常截断日志后系统是否仍能恢复有效记录

### 11.2 性能实验

- 固定 value 长度，逐步增加操作次数，统计吞吐量和平均时延
- 对比 `PUT` 和 `GET` 的性能差异
- 对比冷启动和日志恢复后的性能变化
- 扩展后可对比单线程版本与多线程版本

### 11.3 协议与网络实验

- 构造粘包、半包场景，验证 `\r\n` 分隔策略有效性
- 记录 `epoll` 单线程下的请求处理能力
- 分析高并发下瓶颈位置，如日志刷盘和单连接串行请求

## 12. 当前版本的局限性

- 当前为单线程 Reactor，尚未实现多线程工作池
- 仅支持文本协议，未实现二进制协议
- 压测工具为基础串行压测，尚未实现多连接并发压测
- WAL 只有日志追加与重放，尚未实现日志压缩和快照
- 服务端启动参数暂未做配置文件化

## 13. 后续可扩展方向

- 引入 `shared_mutex` 提升读多写少场景性能
- 实现线程池或主从 Reactor 模型
- 增加配置文件和命令行参数解析
- 增加更完整的 benchmark 统计，如 P95/P99 延迟
- 引入快照、SSTable 或 compaction 机制
- 增加客户端自动脚本和更系统的实验报告数据采集

## 14. Git 提交记录

当前项目关键迭代如下：

```text
4acb5c4 chore: bootstrap KV-Store project skeleton and skip list
251f4ff feat: add WAL persistence and recovery
171cbcb feat: add epoll reactor server and text protocol
d435fda feat: add client CLI and benchmark tool
```

如果你要把这个项目继续打磨成更完整的毕业设计版本，建议下一步优先补充：

1. 配置文件和命令行参数
2. 并发压测工具
3. 更多实验数据采集脚本
4. 论文中的系统设计图和时序图
