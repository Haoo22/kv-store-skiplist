# KV-Store 论文对齐说明

本文档用于把开题报告中的研究目标，与当前仓库里的真实实现、实验结论和后续重构方向逐项对齐。后续所有“按论文重构项目”的动作，都以本文件为准，而不是以开题报告中的理想化表述直接驱动代码。

## 1. 对齐原则

- 论文服务于项目，但不能违背当前仓库已经验证过的事实。
- 当前主线实现固定为单线程 Reactor，不把线程池重新拉回主线。
- 线程池方案只保留为实验性探索和反例结论，用于论文分析与答辩说明。
- 正式 benchmark、协议验证、答辩展示 demo 必须严格区分。

## 2. 总体结论

当前项目与开题报告并不冲突，但需要做一次“口径收敛”：

- 可以保留的主线：`SkipList + WAL + Epoll + 文本协议 + benchmark + demo`
- 需要修正的口径：不要再把“多线程 worker + 主线程监听”写成当前主线实现
- 需要强调的事实：线程池方案已做过 benchmark，端到端明显退化，因此主线保留单线程 Reactor

## 3. 论文目标与项目现状映射

| 论文目标 | 当前仓库模块 | 当前状态 | 是否与论文原表述一致 | 后续动作 |
| --- | --- | --- | --- | --- |
| 轻量级高性能 KV 存储引擎 | `src/kvstore.cpp` `src/Server.cpp` `src/benchmark_main.cpp` | 已实现主线系统 | 部分一致 | 将“高性能”口径收敛到已实测结果，不扩大结论 |
| 跳表作为核心内存索引 | `include/kvstore/SkipList.hpp` | 已实现 | 基本一致 | 补足论文可复用的结构说明、复杂度说明、范围查询价值 |
| WAL 持久化与恢复 | `include/kvstore/WAL.hpp` `src/WAL.cpp` | 已实现 | 基本一致 | 继续整理恢复验证步骤和写入策略说明 |
| Epoll + Reactor 网络模型 | `include/kvstore/Server.hpp` `src/Server.cpp` | 已实现，主线为单线程 Reactor | 与开题“多线程处理”预期不一致 | 统一改成“单线程 Reactor 主线 + 并行化方案实验评估” |
| 自定义文本协议与客户端 | `src/Protocol.cpp` `src/client_main.cpp` | 已实现 | 一致 | 提炼协议格式、命令行为、粘包半包处理逻辑 |
| 性能压测与实验分析 | `src/benchmark_main.cpp` `src/compare_benchmark_main.cpp` `scripts/` | 已实现且较完整 | 超出开题原始粒度 | 统一实验分类、输出口径和论文实验方法描述 |
| 答辩展示与可视化 | `demo/defense_dashboard.html` `demo/defense_demo_server.py` | 已实现 | 开题中只笼统提到 demo | 明确其为展示链路，不作为正式 benchmark 证据 |

## 4. 需要冻结的论文口径

以下表述从现在开始固定，不再反复摇摆：

- 当前系统主线：单线程 Reactor KV-Store
- 当前核心模块：SkipList、WAL、文本协议、Client/Server、benchmark、demo
- 当前并发探索结论：线程池方案已实验验证，但端到端退化，不纳入主线
- 当前正式性能工具：`kvstore_bench`、`kvstore_compare_bench`
- 当前协议验证工具：`kvstore_client`、`kvstore_bench ... full`、`packetsender`
- 当前答辩展示工具：`demo/defense_demo_server.py` + `demo/defense_dashboard.html`

## 5. 开题报告中需要重点修正的内容

这些点后续需要同步到论文正文、答辩稿或开题报告修订版中：

1. 并发模型
- 原表述偏向“主线程监听、工作线程处理”
- 当前应改为“主线为单线程 Reactor；线程池方案作为实验性探索已验证退化”

2. 锁与多线程扩展
- 原文对“多线程同步”和“shared_mutex”预期较强
- 当前项目构建标准是 C++14
- 当前主线 `KVStore` 已改为节点级锁跳表
- 开题报告里“读写锁进一步优化”的这部分，现在应明确写成：
  1. 主线服务端仍是单线程 Reactor
  2. 存储层通过节点级锁缩小跳表内部临界区
- 当前应改为“服务端主线仍是单线程 Reactor，但内部索引已经切换到节点级锁版本”

3. 高性能表述
- 原文写法较理想化
- 当前应改为“高性能结论以 README 中 benchmark 结果和实验记录为准，分串行、pipeline、多客户端 aggregate QPS 三类描述”

4. Demo 定位
- 原文把压测与演示放在同一任务描述里
- 当前应拆开：正式 benchmark 负责实验结论，demo 负责答辩展示

## 6. 不再做的方向

为了避免重构失控，下列方向不作为本轮主目标：

- 不把线程池重新做成主线版本
- 不为了贴合开题报告原始预期而强行引入多线程网络架构
- 不在没有论文收益的前提下做大规模重构
- 不把 demo 结果写成正式性能结论

## 7. 后续重构的直接目标

后续项目重构的直接目标是：

1. 让 README、`docs/`、代码实现、论文叙事完全一致
2. 让主线系统更适合论文描述与答辩复述
3. 让实验链路可复现、可区分、可写入论文
4. 让 demo 成为真正服务答辩的展示模块，而不是混入实验结论
