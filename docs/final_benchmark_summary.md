# KV-Store 最终数据对照总表

本文档集中给出当前节点级锁版本的最终实验口径，便于直接写入论文正文或答辩总结页。

## 1. 主线网络 benchmark

主线服务端：

- 单线程 Reactor
- `no-wal`：`./bin/kvstore_server --no-wal`
- `with-wal`：`./bin/kvstore_server --wal-sync-ms 10`

测试工具：

- `./bin/kvstore_bench`

### 1.1 结果总表

| 场景 | 命令 | 结果 |
| --- | --- | --- |
| `no-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 19480.95 ops/s` `GET 23533.51 ops/s` |
| `no-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 162200.74 ops/s` `GET 473978.58 ops/s` |
| `no-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=219296.44` |
| `with-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 9427.74 ops/s` `GET 23002.57 ops/s` |
| `with-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 66074.64 ops/s` `GET 191629.62 ops/s` |
| `with-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=82047.62` |

### 1.2 可引用结论

- 当前主线已经具备稳定的网络吞吐表现
- `pipeline` 可以明显抬高端到端吞吐
- 引入 WAL 后，`PUT` 吞吐下降明显，但系统获得了恢复能力

## 2. 主比较：节点级锁跳表 vs 原版红黑树基线

compare benchmark 当前固定比较：

- `kvstore_no_wal`
- `kvstore_with_wal`
- `std_map_mutex`
- `std_map_mutex_wal`

测试命令：

```text
./bin/kvstore_compare_bench 20000 8 100000 mixed
./bin/kvstore_compare_bench 20000 8 100000 read
./bin/kvstore_compare_bench 20000 8 100000 write
```

关注 `8` 线程结果：

| workload | `std_map_mutex` | `kvstore_no_wal` | `std_map_mutex_wal` | `kvstore_with_wal` |
| --- | ---: | ---: | ---: | ---: |
| `mixed` | `613721.45 ops/s` | `4596228.24 ops/s` | `222463.99 ops/s` | `199994.80 ops/s` |
| `read` | `919778.04 ops/s` | `7855355.03 ops/s` | `545412.63 ops/s` | `769301.87 ops/s` |
| `write` | `578380.03 ops/s` | `2827052.72 ops/s` | `137791.88 ops/s` | `128888.06 ops/s` |

### 2.1 可引用结论

- 在无 WAL 场景下，节点级锁跳表已经明显超过 `std::map + mutex` 基线
- 在 `read` workload 下优势最明显，说明当前设计更适合论文中“读多写少”的主叙事
- 引入 WAL 后，系统主要受同步写盘约束，但节点级锁主线仍保持了可接受的相对表现

## 3. WAL 恢复补验

测试命令：

```text
./scripts/verify_wal_recovery.sh
```

结果：

```text
WAL recovery verification passed
```

## 4. 总结

- 项目已经完成轻量级 KV-Store 的主线实现，具备完整闭环、恢复能力、多客户端接入能力和正式 benchmark 链路
- 当前主线固定为单线程 Reactor，线程池方案经实验验证退化，因此不进入主线
- 如果按“节点级锁跳表 vs 原版红黑树基线”进行比较，当前主线版本已经在典型 workload 下明显占优
- 正式性能结论应收敛到当前实测结果，不扩写成工业级或绝对领先结论
