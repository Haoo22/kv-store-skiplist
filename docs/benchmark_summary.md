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
./bin/kvstore_compare_bench 20000 8 100000 read-all
./bin/kvstore_compare_bench 20000 8 100000 read
./bin/kvstore_compare_bench 20000 8 100000 write
```

每条命令都会输出 `1/2/4/8` 线程下各实现的吞吐和平均延迟。

其中 `read-all` 是读多写少并覆盖所有存储操作的 workload：`75% GET`、`10% SCAN`、`10% PUT`、`5% DELETE`。

## 3. 代表性结果

以下表格摘取最近一次运行中的 `8` 线程结果：

| workload | `std_map_mutex` | `kvstore_no_wal` | `std_map_mutex_wal` | `kvstore_with_wal` |
| --- | ---: | ---: | ---: | ---: |
| `read-all` | `488572.88 ops/s` | `4667315.23 ops/s` | `362850.07 ops/s` | `681620.83 ops/s` |
| `read` | `591654.80 ops/s` | `5351213.52 ops/s` | `499362.11 ops/s` | `951069.33 ops/s` |
| `write` | `337998.42 ops/s` | `1784484.07 ops/s` | `131063.38 ops/s` | `135895.93 ops/s` |

## 4. 结果说明

- 在这组测试参数下，`kvstore_no_wal` 在三类 workload 中都高于 `std_map_mutex` 基线。
- `read-all` workload 是读多写少场景，同时覆盖 `PUT`、`GET`、`DELETE` 和 `SCAN` 四类存储操作。
- `read-all` 和 `read` workload 中写操作占比较低，带 WAL 的跳表实现仍高于 `std_map_mutex_wal`。
- `write` workload 中写操作占比较高，WAL 追加路径成为主要瓶颈，两类带 WAL 实现的吞吐接近。
- 当前跳表实现支持有序索引和范围查询；无 WAL 场景主要体现内存索引并发能力，带 WAL 场景还会受到日志追加和同步策略影响。
- 该结果主要反映当前实现的锁策略和代码路径开销，不代表端到端网络场景的全部表现。

## 5. 网络 Benchmark 说明

端到端网络 benchmark 使用 `kvstore_bench` 或 `run_network_bench.sh` 执行。

由于该测试依赖本地 TCP 回环与服务端进程，结果受运行环境影响较大，建议在目标机器上重新执行并记录：

```bash
./bin/kvstore_bench 127.0.0.1 6380 5000 1
./bin/kvstore_bench 127.0.0.1 6380 5000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8
```
