# KV-Store 答辩讲述提纲

本文档把当前仓库已经沉淀下来的材料压缩成一份可直接复用的答辩讲述顺序，目标是让你在 5 到 8 分钟内讲清楚项目主线、实验依据和设计取舍。

## 1. 开场定位

可以先用一句话概括项目：

“这个项目实现的是一个基于 C++14、跳表、WAL 和单线程 Reactor 的轻量级 KV-Store，我不仅把它做出来了，还配了协议验证、benchmark、恢复验证和答辩展示链路。”

紧接着说明三件事：

- 主线系统已经可运行
- 关键设计有实验支撑
- 线程池方案做过，但结果是退化，所以没有硬塞进主线

## 2. 系统主线

建议按四层讲：

1. 存储层：`SkipList + KVStore`
2. 持久化层：`WAL`
3. 网络与协议层：`epoll + 单线程 Reactor + 文本协议`
4. 验证与展示层：tests、benchmark、demo

对应材料：

- [system_architecture.md](../docs/system_architecture.md)
- [module_overview.md](../docs/module_overview.md)

## 3. 请求处理链路

这里适合讲一条完整请求如何流转：

客户端命令
-> `LineCodec`
-> `CommandProcessor`
-> `KVStore`
-> `WAL`
-> 文本响应返回

重点点明：

- 文本协议可处理粘包半包
- `PUT` 写入时先 WAL 再更新内存
- `SCAN` 依赖跳表有序键空间

对应材料：

- [request_flow.md](../docs/request_flow.md)
- [protocol_reference.md](../docs/protocol_reference.md)

## 4. 为什么主线保留单线程 Reactor

这是答辩里最值得主动讲的一点。

推荐表述：

- 我不是没考虑并发，而是做过线程池方案
- 但端到端 benchmark 显示线程池明显退化
- 因此主线保留单线程 Reactor，这是基于实验结果的取舍

再补一句原因摘要：

- 入口读取和最终写回仍在 Reactor 线程
- 中间再切线程会引入排队、唤醒、回传和重排成本
- 底层存储层本身也不是完全无锁

对应材料：

- [thread_pool_findings.md](../docs/thread_pool_findings.md)
- [request_flow.md](../docs/request_flow.md)

## 5. 实验怎么分

答辩时一定要主动把三条线分开：

- 协议验证：`kvstore_client`、`kvstore_bench ... full`、`packetsender`
- 正式 benchmark：`kvstore_bench`、`kvstore_compare_bench`
- 展示 demo：`defense_demo_server.py + defense_dashboard.html`

一句话说明边界：

“正式性能结论只引用 benchmark，demo 只负责展示，不负责裁决性能。”

对应材料：

- [experiment_classification.md](../docs/experiment_classification.md)
- [benchmark_methodology.md](../docs/benchmark_methodology.md)
- [demo_usage.md](../docs/demo_usage.md)

## 5.1 主比较怎么讲

如果老师追问“跳表和红黑树到底谁更快”，当前建议直接按答辩主比较口径回答：

- 比较对象：`kvstore_no_wal` vs `std_map_mutex`
- 场景：预加载后、读多写少
- 结论：当前主线细粒度锁版本已经明显超过原版红黑树基线

可以直接引用：

- `preload=100000`：`3.01x`
- `preload=300000`：`4.91x`

同时主动补一句边界：

- 这是“当前主线细粒度锁 KVStore”对“原版红黑树基线”的比较
- `skiplist_sharded` 现在只保留为结构镜像对照，不再是主线替身
- 我没有把它夸大成“跳表已经优于所有分片化 map 实现”

## 6. demo 怎么讲

推荐演示顺序：

1. 协议演示：证明不是静态页面
2. 并发演示：展示真实事件映射和动态指标
3. WAL 恢复：收束到可恢复性

讲的时候要主动补一句：

“页面上的动态 QPS 和延迟是展示型指标，正式实验数据我引用的是 benchmark 结果。”

## 7. 收尾结论

结尾可以收成三点：

- 项目已经形成完整主线：跳表、WAL、Reactor、文本协议
- 验证链路已经完整：tests、协议验证、benchmark、demo
- 关键取舍有实验支撑：线程池不是没做，而是做了以后退化，所以主线保留单线程 Reactor
- 如果按答辩主比较口径，当前主线细粒度锁版本已经在读多写少场景下明显超过原版红黑树基线
