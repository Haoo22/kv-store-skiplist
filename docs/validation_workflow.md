# KV-Store 验证工作流

本文档给出推荐的验证顺序和每一步的目的。

## 1. 推荐顺序

1. 编译项目
2. 运行单元与集成测试
3. 运行协议回归
4. 运行 WAL 恢复验证
5. 运行 benchmark

## 2. 编译

```bash
cmake -S . -B build
cmake --build build -j
```

## 3. 单元与集成测试

```bash
ctest --test-dir build --output-on-failure
./bin/kvstore_tests
```

覆盖内容：

- 跳表基础行为
- 协议解析
- WAL 回放
- 并发插入、读取、删除
- 扫描一致性与高 churn 场景

## 4. 协议回归

```bash
./scripts/verify_protocol_regression.sh
```

覆盖内容：

- `PING`
- `PUT`
- `GET`
- `SCAN`
- `DEL`
- `QUIT`

## 5. WAL 恢复验证

```bash
./scripts/verify_wal_recovery.sh
```

覆盖内容：

- 写入 WAL
- 停止并重启服务端
- 重放日志恢复内存状态
- 恢复后 `GET` 校验

## 6. Benchmark

网络 benchmark：

```bash
./bin/kvstore_bench 127.0.0.1 6380 5000 1
./bin/kvstore_bench 127.0.0.1 6380 5000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 8
```

进程内对比 benchmark：

```bash
./bin/kvstore_compare_bench 500 8 0 mixed
./bin/kvstore_compare_bench 500 8 0 read
./bin/kvstore_compare_bench 500 8 0 write
```

说明：

- 网络 benchmark 用于观察端到端吞吐表现
- 进程内 benchmark 用于对比不同数据结构和 WAL 包装的相对开销
