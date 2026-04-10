# KV-Store 毕设提交清单

本文档用于区分哪些内容属于毕设正式提交物，哪些只作为辅助材料保留。

## 1. 必须提交

- [CMakeLists.txt](../CMakeLists.txt)
- [include/kvstore](../include/kvstore)
- [src](../src)
- [tests/test_main.cpp](../tests/test_main.cpp)
- [scripts/run_network_bench.sh](../scripts/run_network_bench.sh)
- [scripts/run_compare_bench.sh](../scripts/run_compare_bench.sh)
- [scripts/verify_protocol_regression.sh](../scripts/verify_protocol_regression.sh)
- [scripts/verify_wal_recovery.sh](../scripts/verify_wal_recovery.sh)
- [scripts/verify_demo_http.sh](../scripts/verify_demo_http.sh)
- [README.md](../README.md)

## 2. 建议提交

- [docs/thesis_materials.md](../docs/thesis_materials.md)
- [docs/final_benchmark_summary.md](../docs/final_benchmark_summary.md)
- [docs/benchmark_methodology.md](../docs/benchmark_methodology.md)
- [docs/system_architecture.md](../docs/system_architecture.md)
- [docs/request_flow.md](../docs/request_flow.md)
- [docs/protocol_reference.md](../docs/protocol_reference.md)
- [docs/validation_workflow.md](../docs/validation_workflow.md)
- [docs/defense_talk_track.md](../docs/defense_talk_track.md)
- [docs/figure_materials.md](../docs/figure_materials.md)
- [docs/demo_usage.md](../docs/demo_usage.md)
- [demo/defense_dashboard.html](../demo/defense_dashboard.html)
- [demo/defense_demo_server.py](../demo/defense_demo_server.py)

## 3. 可保留但不是核心提交物

- [docs/thread_pool_findings.md](../docs/thread_pool_findings.md)
- [docs/thesis_alignment.md](../docs/thesis_alignment.md)
- [docs/restructure_execution_plan.md](../docs/restructure_execution_plan.md)
- [docs/development_log_index.md](../docs/development_log_index.md)

## 4. 不需要提交

- `build/`
- `bin/`
- `.codex`
- `/tmp` 中间文件

## 5. 当前答辩主比较口径

如果答辩里要讲“跳表优于红黑树基线”，建议固定表述为：

- 主线细粒度锁版本：`kvstore_no_wal`
- 原版红黑树基线：`std_map_mutex`

不要扩写成“跳表已经优于所有分片化 `std::map` 实现”。
