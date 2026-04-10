# KV-Store 最终数据对照总表

本文档用于集中给出当前项目最适合写入论文正文、答辩 PPT 或结果总结页的最终实验数据。

使用原则：

- 正式性能结论优先引用本文档
- 更完整的方法说明见 [benchmark_methodology.md](/home/haoo/code/study/KV-Store/docs/benchmark_methodology.md)
- 如果需要追溯实验设计和背景，再回到对应方法文档

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
| `no-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 21908.01 ops/s` `GET 22166.07 ops/s` |
| `no-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 337518.56 ops/s` `GET 260525.22 ops/s` |
| `no-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=256408.86` |
| `with-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 11492.80 ops/s` `GET 23503.42 ops/s` |
| `with-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 69097.99 ops/s` `GET 371471.03 ops/s` |
| `with-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=98319.75` |

### 1.2 可引用结论

- 当前主线已经具备较稳定的网络吞吐表现
- `pipeline` 能显著抬高端到端吞吐
- 引入 WAL 后，`PUT` 吞吐下降明显，但系统获得了恢复能力
- 主线服务端仍固定为单线程 Reactor

## 2. 答辩主比较：细粒度锁跳表 vs 原版红黑树基线

当前答辩主比较口径：

- 细粒度锁跳表：`skiplist_sharded`
- 原版红黑树基线：`std_map_mutex`

测试命令：

```text
./bin/kvstore_compare_bench 5000 8 100000 read
./bin/kvstore_compare_bench 5000 8 300000 read
```

测试场景：

- 预加载稳定数据
- `read` workload：`90% GET, 10% PUT`
- 重点关注 `8` 线程结果

### 2.1 结果总表

| preload | `std_map_mutex` | `skiplist_sharded` | 提升倍数 |
| --- | ---: | ---: | ---: |
| `100000` | `873795.87 ops/s` | `2419362.06 ops/s` | `2.77x` |
| `300000` | `658386.40 ops/s` | `1911372.71 ops/s` | `2.90x` |

### 2.2 可引用结论

- 原版整表锁跳表没有发挥出并发潜力
- 当锁粒度下沉到分片级之后，跳表在读多写少场景下已明显超过原版 `std::map + mutex` 基线
- 这组结果适合直接作为答辩主对照表
- 这组结果来自不带 WAL 的进程内对比

## 3. WAL 补测：细粒度锁跳表 vs 原版红黑树基线

为补齐“带 WAL 后还能不能保持优势”这个问题，当前在 compare benchmark 中补充了实验性 WAL 包装对照项：

- `std_map_mutex_wal`
- `skiplist_sharded_wal`

### 3.1 结果总表

| preload | `std_map_mutex_wal` | `skiplist_sharded_wal` | 提升倍数 |
| --- | ---: | ---: | ---: |
| `100000` | `482484.93 ops/s` | `854925.36 ops/s` | `1.77x` |
| `300000` | `416153.30 ops/s` | `917692.71 ops/s` | `2.21x` |

### 3.2 可引用结论

- 在这组进程内 WAL 对照里，`skiplist_sharded_wal` 也已经超过 `std_map_mutex_wal`
- 这说明加上 WAL 后，细粒度锁跳表相对原版红黑树基线的优势仍然存在
- 但这组结果属于 compare benchmark 的实验性 WAL 包装对照，不应和主线网络 benchmark 混写

## 4. 补充公平对照：`std_map_sharded`

这一组不作为答辩主比较口径，但适合在老师追问时补充说明。

### 3.1 `preload=100000, read-heavy, 8 threads`

| benchmark | throughput |
| --- | ---: |
| `std_map_mutex` | `873795.87 ops/s` |
| `std_map_sharded` | `4430013.92 ops/s` |
| `skiplist_sharded` | `2419362.06 ops/s` |

### 3.2 可引用结论

- 锁粒度优化本身带来的收益非常大
- 当前实现下，`std_map_sharded` 仍快于 `skiplist_sharded`
- 因此当前更稳妥的说法是：
  细粒度锁跳表已经明显优于原版红黑树基线，但不应扩写成“已经优于所有分片化 `std::map` 实现”

## 5. WAL 恢复补验

测试命令：

```text
./scripts/verify_wal_recovery.sh
```

结果：

```text
WAL recovery verification passed
```

可引用结论：

- 系统不仅具备 WAL 设计，还完成了端到端恢复验证

## 6. 最终建议口径

适合论文和答辩直接使用的最终说法：

- 项目已经完成轻量级 KV-Store 的主线实现，具备完整闭环、恢复能力、多客户端接入能力和正式 benchmark 链路
- 主线固定为单线程 Reactor，线程池方案经实验验证退化，因此不进入主线
- 如果按“细粒度锁跳表 vs 原版红黑树基线”进行比较，当前跳表实验版已经在读多写少场景下明显占优
- 即使补上 compare benchmark 中的实验性 WAL 包装对照，`skiplist_sharded_wal` 也已经超过 `std_map_mutex_wal`
- 但这一结论不应被扩写为“跳表已经优于所有分片化 `std::map` 实现”
