# KV-Store 论文支撑材料摘要

本文档用于把当前仓库中最适合写入论文正文或答辩 PPT 的内容集中整理出来。

## 1. 系统定位

本项目实现的是一个轻量级键值存储系统，核心特征为：

- 基于 C++14 与 CMake 构建
- 以内存跳表作为核心索引
- 以 WAL 作为基础持久化与恢复机制
- 以 `epoll + 非阻塞 socket + 单线程 Reactor` 作为主线网络模型
- 以文本协议承载 `PING/PUT/GET/DEL/SCAN/QUIT`
- 以 benchmark、协议验证和 demo 形成完整验证链路

## 2. 论文中的推荐系统结构表述

建议在论文中把系统拆成四层：

1. 存储层：`SkipList + KVStore`
2. 持久化层：`WAL`
3. 网络与协议层：`Server + Protocol`
4. 验证与展示层：benchmark、tests、demo

这样的好处是：

- 能直接对应仓库模块
- 易于画系统架构图
- 能清楚区分“主线实现”和“实验/展示工具”

## 3. 推荐实验结构

论文实验部分建议分成四类：

### 3.1 正确性验证

- 单元/集成测试
- 协议回归测试
- WAL 恢复验证

### 3.2 网络 benchmark

- 单客户端串行
- 单客户端 pipeline
- 多客户端 aggregate QPS

### 3.3 进程内对比实验

- `kvstore_no_wal`
- `kvstore_with_wal`
- `std_map_mutex`
- `skiplist_sharded`

### 3.4 反例分析

- 线程池方案端到端退化
- 说明“并行化设计不等于吞吐提升”

## 4. 推荐答辩讲述主线

答辩时建议按下面顺序讲：

1. 为什么要做轻量级 KV-Store
2. 为什么选跳表
3. 为什么需要 WAL
4. 为什么主线采用单线程 Reactor
5. benchmark 如何验证系统
6. 为什么线程池方案最后没有进入主线
7. demo 如何把真实事件映射到前端做展示

## 5. 当前最稳的结论

这些结论在当前仓库和 README 中都有支撑，适合直接进入论文或答辩材料：

- 当前主线实现是单线程 Reactor
- 文本协议主链路已经可用
- WAL 支持重放恢复
- 项目当前使用 C++14，`SkipList` 使用 `std::shared_timed_mutex` 作为读写锁
- `kvstore_bench` 已支持 `scenario` 和多客户端 aggregate QPS
- `packetsender` 更适合外部协议验证，不适合作为主 benchmark 工具
- 线程池方案已做过完整 benchmark，但端到端明显退化，因此未纳入主线
- 当前主线已经达到“完整闭环、恢复能力、多客户端接入、正式 benchmark 链路”这几个核心预期
- 当前主线尚不能写成“多线程并行主线”
- 如果按答辩主比较口径使用“细粒度锁跳表 `skiplist_sharded` vs 原版红黑树基线 `std_map_mutex`”，当前跳表实验版已经明显占优
- 但这不应扩写成“跳表已经优于所有分片化 `std::map` 实现”

## 6. 写作时应避免的表述

- 不要把“多线程 worker 模型”写成当前主线实现
- 不要把 demo 指标写成正式 benchmark 数据
- 不要把线程池探索写成成功优化
- 不要写超出当前 README 和实测结果之外的性能结论
