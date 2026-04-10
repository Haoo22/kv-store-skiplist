# KV-Store 模块总览

本文档从“论文阅读”和“答辩讲解”的角度，对当前仓库模块做一次职责拆分。目标不是列出所有文件，而是回答两个问题：

1. 当前系统由哪些核心模块组成
2. 每个模块在论文里该怎么讲

## 1. 主线系统

当前项目主线是一个基于单线程 Reactor 的轻量级 KV 存储系统，核心链路如下：

客户端请求
-> 文本协议接入
-> `epoll` 事件循环读写
-> 协议解析与命令分发
-> `SkipList` 内存索引读写
-> `WAL` 追加写与恢复
-> 响应返回客户端

论文里可以把它分成四个核心层次：

- 存储层：`SkipList` + `KVStore`
- 持久化层：`WAL`
- 网络与协议层：`Server` + `Protocol`
- 验证与展示层：benchmark、协议验证、demo

## 2. 核心模块对应关系

### 2.1 存储层

文件：

- [SkipList.hpp](../include/kvstore/SkipList.hpp)
- [kvstore.hpp](../include/kvstore/kvstore.hpp)
- [kvstore.cpp](../src/kvstore.cpp)

职责：

- 提供 `Put`、`Get`、`Delete`、`Scan`
- 以跳表作为核心内存索引
- 向上层屏蔽底层索引和持久化细节

### 2.2 持久化层

文件：

- [WAL.hpp](../include/kvstore/WAL.hpp)
- [WAL.cpp](../src/WAL.cpp)

职责：

- 负责追加写日志
- 负责系统重启后的日志重放恢复
- 负责处理日志尾部不完整记录

### 2.3 网络与协议层

文件：

- [Server.hpp](../include/kvstore/Server.hpp)
- [Server.cpp](../src/Server.cpp)
- [Protocol.hpp](../include/kvstore/Protocol.hpp)
- [Protocol.cpp](../src/Protocol.cpp)
- [server_main.cpp](../src/server_main.cpp)
- [client_main.cpp](../src/client_main.cpp)

职责：

- 使用 `epoll + 非阻塞 socket` 处理网络事件
- 通过文本协议解析 `PING/PUT/GET/DEL/SCAN/QUIT`
- 处理 TCP 粘包和半包

补充材料：

- [protocol_reference.md](../docs/protocol_reference.md)
- [cli_reference.md](../docs/cli_reference.md)

### 2.4 实验与验证层

文件：

- [benchmark_main.cpp](../src/benchmark_main.cpp)
- [compare_benchmark_main.cpp](../src/compare_benchmark_main.cpp)
- [run_compare_bench.sh](../scripts/run_compare_bench.sh)
- [run_network_bench.sh](../scripts/run_network_bench.sh)
- [test_main.cpp](../tests/test_main.cpp)

职责：

- 功能正确性验证
- 协议回归验证
- 正式 benchmark
- 进程内对比压测

说明：

- `benchmark_main.cpp` 和 `compare_benchmark_main.cpp` 是两类正式实验的底层程序入口
- `run_network_bench.sh` 和 `run_compare_bench.sh` 是围绕这两个入口的包装脚本

### 2.5 展示与答辩层

文件：

- [defense_dashboard.html](../demo/defense_dashboard.html)
- [defense_demo_server.py](../demo/defense_demo_server.py)
- [defense_demo_prompt.md](../docs/internal/defense_demo_prompt.md)

职责：

- 用浏览器展示协议交互、并发客户端、性能指标和实验结论
- 通过真实事件映射驱动前端，而不是纯静态回放

## 3. 当前仓库推荐阅读顺序

1. [thesis_alignment.md](../docs/internal/thesis_alignment.md)
2. [README.md](../README.md)
3. [module_overview.md](../docs/module_overview.md)
4. [experiment_classification.md](../docs/experiment_classification.md)
5. [demo_usage.md](../docs/demo_usage.md)
6. [protocol_reference.md](../docs/protocol_reference.md)
7. [cli_reference.md](../docs/cli_reference.md)

## 4. 当前主线与实验性内容的边界

- 主线实现：单线程 Reactor、SkipList、WAL、文本协议
- 主线验证：`kvstore_bench`、`kvstore_compare_bench`、`tests`
- 实验性反例：线程池并行化方案及其退化结论，保留在 README 开发日志中
- 展示模块：demo 页面与本地 SSE 服务，仅用于答辩
