# KV-Store 验证工作流

本文档给出推荐的验证顺序和每一步的目的。

## 1. 推荐顺序

1. 编译项目
2. 运行单元与集成测试
3. 运行协议回归
4. 运行 WAL 恢复验证
5. 运行 benchmark

这个顺序从低成本检查开始，逐步覆盖更完整的运行路径。前四步用于确认功能正确性，最后的 benchmark 用于观察不同 workload 下的性能表现。

## 2. 编译

```bash
cmake -S . -B build
cmake --build build -j
```

编译完成后，可执行文件会输出到项目根目录的 `bin/` 下。后续测试、验证脚本和 benchmark 都依赖这些二进制文件。

## 3. 单元与集成测试

```bash
ctest --test-dir build --output-on-failure
./bin/kvstore_tests
```

这一步主要检查核心数据结构、协议处理和 WAL 回放逻辑。它运行速度较快，适合每次修改核心代码后优先执行。

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

协议回归会启动一个 `--no-wal` 服务端，并通过 benchmark 客户端执行完整协议场景。它能覆盖网络读取、半包/粘包处理、命令解析和响应写回。

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

WAL 恢复验证会真实启动、停止并重启服务端，用于确认写入的数据能够通过日志重放恢复。它比单元测试更接近实际运行方式。

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
./bin/kvstore_compare_bench 500 8 0 read-all
./bin/kvstore_compare_bench 500 8 0 read
./bin/kvstore_compare_bench 500 8 0 write
```

说明：

- 网络 benchmark 用于观察端到端吞吐表现
- 进程内 benchmark 用于对比不同数据结构和 WAL 包装的相对开销
- `read-all` 用于覆盖 `PUT`、`GET`、`DELETE` 和 `SCAN` 的读多写少场景
- Benchmark 结果会受机器负载、文件系统和 CPU 调度影响，建议在同一环境下重复运行后再比较。
