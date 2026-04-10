# KV-Store 论文插图与表格取材说明

本文档用于回答一个实际问题：如果后面要写论文正文或做答辩 PPT，哪些图、表和说明可以直接从当前仓库材料中提炼。

## 1. 系统总体结构图

建议来源：

- [system_architecture.md](../system_architecture.md)

适合画成：

- 四层系统结构图
- 主线模块关系图

图中建议保留的关键词：

- Client / Protocol
- Single-Thread Reactor
- SkipList
- WAL
- Benchmark / Demo

## 2. 请求处理时序图

建议来源：

- [request_flow.md](../request_flow.md)
- [protocol_reference.md](../protocol_reference.md)

适合画成：

- 一条 `PUT` 请求的处理时序
- 一条 `GET` 请求的处理时序

建议强调：

- `LineCodec` 负责按 `\r\n` 提取命令
- `CommandProcessor` 负责命令分发
- 写操作先 WAL 后更新内存

## 3. 模块职责表

建议来源：

- [module_overview.md](../module_overview.md)

适合画成：

- “模块 / 文件 / 职责”三列表格

可直接列的模块：

- SkipList
- KVStore
- WAL
- Server
- Protocol
- Benchmark
- Demo

## 4. 实验分类表

建议来源：

- [experiment_classification.md](../experiment_classification.md)

适合画成：

- “实验类型 / 工具 / 用途 / 是否可作为正式结论”表格

建议在论文中直接保留的分类：

- 单元/集成测试
- 协议验证
- 正式网络 benchmark
- 进程内对比 benchmark
- demo 展示

## 5. benchmark 方法表

建议来源：

- [benchmark_methodology.md](../benchmark_methodology.md)

适合画成：

- `kvstore_bench` 参数说明表
- `kvstore_compare_bench` 参数说明表
- 单客户端与多客户端统计口径对照表

重点要写清：

- 单客户端按 phase 统计
- 多客户端按整组 wall clock 统计 aggregate QPS
- `QUIT` 不计入 aggregate 统计

## 6. 线程池反例总结表

建议来源：

- [thread_pool_findings.md](../internal/thread_pool_findings.md)
- [development_log_index.md](../internal/development_log_index.md)

适合画成：

- “方案 / 预期 / 实测结论 / 原因摘要”表格

推荐表述：

- 单线程 Reactor：当前主线
- 线程池方案：实验反例
- 结论依据：端到端 benchmark

## 7. demo 说明图

建议来源：

- [demo_usage.md](../demo_usage.md)
- [defense_dashboard.html](../../demo/defense_dashboard.html)

适合画成：

- demo 页面截图
- demo 三种模式说明表

必须附带的说明：

- 页面动态指标属于展示型指标
- 正式性能结论仍以 benchmark 为准

## 8. 当前使用建议

- 论文正文：优先引用本文件列出的独立文档
- 论文附录：再回到 [README.md](../../README.md) 的开发日志
- 答辩 PPT：优先使用结构图、实验分类表、线程池反例总结表、demo 页面截图
