# KV-Store 验证工作流

本文档给出当前答辩主线版本的推荐验证顺序。

## 1. 推荐顺序

1. 编译项目
2. 跑单元/集成测试
3. 跑协议回归
4. 跑 WAL 恢复验证
5. 跑正式 benchmark

## 2. 编译

```bash
cmake -S . -B build
cmake --build build -j
```

## 3. 单元/集成测试

```bash
ctest --test-dir build --output-on-failure
./bin/kvstore_tests
```

用途：

- 验证跳表、WAL、协议解析和基础恢复逻辑
- 验证节点级锁跳表在并发插入、读取、删除和扫描压力下的基本正确性

## 4. 协议回归

```bash
./scripts/verify_protocol_regression.sh
```

用途：

- 验证 `PING/PUT/GET/SCAN/DEL/QUIT` 主链路

## 5. WAL 恢复验证

```bash
./scripts/verify_wal_recovery.sh
```

脚本会自动完成：

- 清理旧数据目录
- 启动启用 WAL 的服务端
- 写入测试 key
- 重启服务端
- 验证重启后仍能读回 key

## 6. 正式 benchmark

网络 benchmark：

```bash
./bin/kvstore_bench 127.0.0.1 6380 10000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 4
./scripts/run_network_bench.sh
```

进程内对比：

```bash
./bin/kvstore_compare_bench 20000 8 100000 mixed
./bin/kvstore_compare_bench 20000 8 100000 read
./bin/kvstore_compare_bench 20000 8 100000 write
./scripts/run_compare_bench.sh
```

用途：

- `kvstore_bench` 用于网络端到端吞吐结果
- `kvstore_compare_bench` 用于节点级锁跳表与 `std_map_mutex` 基线对照
