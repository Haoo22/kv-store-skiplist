# KV-Store Benchmark Summary

本文档汇总项目当前的 benchmark 结论和一组代表性进程内对比结果。

## 1. 基准对象

`kvstore_compare_bench` 当前固定比较：

- `kvstore_no_wal`
- `kvstore_with_wal`
- `std_map_mutex`
- `std_map_mutex_wal`

## 2. 测试命令

```text
./bin/kvstore_compare_bench 20000 8 100000 mixed
./bin/kvstore_compare_bench 20000 8 100000 read
./bin/kvstore_compare_bench 20000 8 100000 write
```

每条命令都会输出 `1/2/4/8` 线程下各实现的吞吐和平均延迟。

## 3. 代表性结果

以下表格摘取最近一次运行中的 `8` 线程结果：

| workload | `std_map_mutex` | `kvstore_no_wal` | `std_map_mutex_wal` | `kvstore_with_wal` |
| --- | ---: | ---: | ---: | ---: |
| `mixed` | `543344.79 ops/s` | `118476.74 ops/s` | `186216.23 ops/s` | `68322.00 ops/s` |
| `read` | `881290.59 ops/s` | `299732.26 ops/s` | `481873.88 ops/s` | `213193.37 ops/s` |
| `write` | `540828.77 ops/s` | `123014.98 ops/s` | `138676.21 ops/s` | `54000.91 ops/s` |

## 4. 结果说明

- 在这组测试参数下，`std_map_mutex` 基线在三类 workload 中都高于当前跳表实现。
- 带 WAL 后，两类实现都会受到追加日志与同步策略影响。
- 该结果主要反映当前实现的锁策略和代码路径开销，不代表端到端网络场景的全部表现。

## 5. 网络 Benchmark 说明

端到端网络 benchmark 使用 `kvstore_bench` 或 `run_network_bench.sh` 执行。

由于该测试依赖本地 TCP 回环与服务端进程，结果受运行环境影响较大，建议在目标机器上重新执行并记录：

```bash
./bin/kvstore_bench 127.0.0.1 6380 5000 1
./bin/kvstore_bench 127.0.0.1 6380 5000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8
```
