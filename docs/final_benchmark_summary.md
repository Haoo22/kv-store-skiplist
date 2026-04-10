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
| `no-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 19480.95 ops/s` `GET 23533.51 ops/s` |
| `no-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 162200.74 ops/s` `GET 473978.58 ops/s` |
| `no-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=219296.44` |
| `with-wal` 串行 | `./bin/kvstore_bench 127.0.0.1 6380 5000 1` | `PUT 9427.74 ops/s` `GET 23002.57 ops/s` |
| `with-wal` pipeline | `./bin/kvstore_bench 127.0.0.1 6380 5000 64` | `PUT 66074.64 ops/s` `GET 191629.62 ops/s` |
| `with-wal` 多客户端 | `./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8` | `aggregate_qps=82047.62` |

### 1.2 可引用结论

- 当前主线已经具备较稳定的网络吞吐表现
- `pipeline` 能显著抬高端到端吞吐
- 引入 WAL 后，`PUT` 吞吐下降明显，但系统获得了恢复能力
- 主线服务端仍固定为单线程 Reactor

## 2. 答辩主比较：主线细粒度锁 KVStore vs 原版红黑树基线

当前答辩主比较口径：

- 主线细粒度锁版本：`kvstore_no_wal`
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

| preload | `std_map_mutex` | `kvstore_no_wal` | 提升倍数 |
| --- | ---: | ---: | ---: |
| `100000` | `619595.15 ops/s` | `1867065.15 ops/s` | `3.01x` |
| `300000` | `410030.13 ops/s` | `2013055.88 ops/s` | `4.91x` |

### 2.2 可引用结论

- 当前毕设主线本身已经采用细粒度分片跳表
- 在读多写少场景下，主线版本已明显超过原版 `std::map + mutex` 基线
- 这组结果适合直接作为答辩主对照表

## 3. WAL 主比较：主线细粒度锁 KVStore vs 原版红黑树基线

当前答辩可直接引用带 WAL 的主线结果：

- `std_map_mutex_wal`
- `kvstore_with_wal`

### 3.1 结果总表

| preload | `std_map_mutex_wal` | `kvstore_with_wal` | 提升倍数 |
| --- | ---: | ---: | ---: |
| `100000` | `471489.49 ops/s` | `863073.39 ops/s` | `1.83x` |
| `300000` | `341984.65 ops/s` | `948082.91 ops/s` | `2.77x` |

### 3.2 可引用结论

- 主线版本在带 WAL 时也已经超过 `std_map_mutex_wal`
- 这说明主线细粒度锁版本在追加日志开销下仍然保持相对优势
- `skiplist_sharded_wal` 现在更适合作为结构镜像对照，而不是主叙事主体

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
- 如果按“主线细粒度锁 KVStore vs 原版红黑树基线”进行比较，当前主线版本已经在读多写少场景下明显占优
- 即使带上 WAL，当前主线版本 `kvstore_with_wal` 也已经超过 `std_map_mutex_wal`
- 但这一结论不应被扩写为“跳表已经优于所有分片化 `std::map` 实现”
