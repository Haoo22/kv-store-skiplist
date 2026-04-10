# KV-Store 论文章节映射

本文档用于回答一个实际问题：论文正文每一章应该从仓库里的哪些实现、文档和实验材料取材。

## 1. 绪论

建议内容：

- 课题背景与研究意义
- 国内外研究现状
- 本课题的研究目标

主要材料来源：

- `/home/haoo/code/study/lunwen.txt`
- [thesis_alignment.md](/home/haoo/code/study/KV-Store/docs/thesis_alignment.md)
- [thread_pool_findings.md](/home/haoo/code/study/KV-Store/docs/thread_pool_findings.md)

## 2. 系统需求与总体设计

建议内容：

- 系统目标
- 模块划分
- 总体架构
- 数据流说明

主要材料来源：

- [README.md](/home/haoo/code/study/KV-Store/README.md)
- [module_overview.md](/home/haoo/code/study/KV-Store/docs/module_overview.md)
- [system_architecture.md](/home/haoo/code/study/KV-Store/docs/system_architecture.md)
- [thesis_materials.md](/home/haoo/code/study/KV-Store/docs/thesis_materials.md)

## 3. 核心数据结构与存储引擎设计

建议内容：

- 跳表结构设计
- `KVStore` 的读写接口
- `Scan` 的范围查询价值

主要材料来源：

- [SkipList.hpp](/home/haoo/code/study/KV-Store/include/kvstore/SkipList.hpp)
- [kvstore.hpp](/home/haoo/code/study/KV-Store/include/kvstore/kvstore.hpp)
- [kvstore.cpp](/home/haoo/code/study/KV-Store/src/kvstore.cpp)
- [compare_benchmark_main.cpp](/home/haoo/code/study/KV-Store/src/compare_benchmark_main.cpp)

## 4. 持久化与恢复机制设计

建议内容：

- WAL 记录格式
- 追加写流程
- 恢复流程
- 尾部损坏处理

主要材料来源：

- [WAL.hpp](/home/haoo/code/study/KV-Store/include/kvstore/WAL.hpp)
- [WAL.cpp](/home/haoo/code/study/KV-Store/src/WAL.cpp)
- [wal_recovery_validation.md](/home/haoo/code/study/KV-Store/docs/wal_recovery_validation.md)

## 5. 网络通信与协议实现

建议内容：

- 单线程 Reactor 模型
- `epoll + 非阻塞 socket`
- 文本协议设计
- 粘包半包处理

主要材料来源：

- [Server.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Server.hpp)
- [Server.cpp](/home/haoo/code/study/KV-Store/src/Server.cpp)
- [Protocol.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Protocol.hpp)
- [Protocol.cpp](/home/haoo/code/study/KV-Store/src/Protocol.cpp)
- [server_main.cpp](/home/haoo/code/study/KV-Store/src/server_main.cpp)
- [request_flow.md](/home/haoo/code/study/KV-Store/docs/request_flow.md)
- [protocol_reference.md](/home/haoo/code/study/KV-Store/docs/protocol_reference.md)

## 6. 系统实现与工具链

建议内容：

- 客户端
- benchmark 工具
- 构建、测试、脚本组织方式

主要材料来源：

- [client_main.cpp](/home/haoo/code/study/KV-Store/src/client_main.cpp)
- [benchmark_main.cpp](/home/haoo/code/study/KV-Store/src/benchmark_main.cpp)
- [compare_benchmark_main.cpp](/home/haoo/code/study/KV-Store/src/compare_benchmark_main.cpp)
- [validation_workflow.md](/home/haoo/code/study/KV-Store/docs/validation_workflow.md)
- [cli_reference.md](/home/haoo/code/study/KV-Store/docs/cli_reference.md)

## 7. 实验设计与结果分析

建议内容：

- 正确性验证
- 正式 benchmark
- 进程内对比实验
- 线程池反例分析

主要材料来源：

- [experiment_classification.md](/home/haoo/code/study/KV-Store/docs/experiment_classification.md)
- [benchmark_methodology.md](/home/haoo/code/study/KV-Store/docs/benchmark_methodology.md)
- [validation_workflow.md](/home/haoo/code/study/KV-Store/docs/validation_workflow.md)
- [thread_pool_findings.md](/home/haoo/code/study/KV-Store/docs/thread_pool_findings.md)
- [development_log_index.md](/home/haoo/code/study/KV-Store/docs/development_log_index.md)
- [README.md](/home/haoo/code/study/KV-Store/README.md) 的答辩版实验结果部分

## 8. 答辩展示与工程总结

建议内容：

- demo 设计目的
- 真实事件映射方案
- 项目局限性与后续优化方向

主要材料来源：

- [demo_usage.md](/home/haoo/code/study/KV-Store/docs/demo_usage.md)
- [defense_demo_prompt.md](/home/haoo/code/study/KV-Store/docs/defense_demo_prompt.md)
- [thesis_materials.md](/home/haoo/code/study/KV-Store/docs/thesis_materials.md)
- [defense_talk_track.md](/home/haoo/code/study/KV-Store/docs/defense_talk_track.md)
- [figure_materials.md](/home/haoo/code/study/KV-Store/docs/figure_materials.md)

## 9. 写作注意事项

- 正式性能结论只引用 `kvstore_bench` 和 `kvstore_compare_bench`
- demo 只写展示价值，不写成正式实验数据来源
- 线程池方案写成实验性反例，不写成成功优化
- 当前主线固定表述为单线程 Reactor
- 如果正文采用答辩主口径，应优先比较 `kvstore_no_wal` 与 `std_map_mutex`
- 如果正文要回应开题报告中的“读写锁进一步优化”，应明确写成：主线已经切到分片跳表细粒度锁版本，`skiplist_sharded` 仅保留为结构镜像对照
