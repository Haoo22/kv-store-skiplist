# KV-Store 验证工作流

本文档给出当前项目推荐的验证顺序，目标是让“测试、协议验证、benchmark、WAL 恢复、demo”形成一条清晰可复现的链路。

## 1. 推荐验证顺序

1. 编译项目
2. 跑单元/集成测试
3. 跑协议回归
4. 跑 WAL 恢复验证
5. 跑正式 benchmark
6. 验证 demo 页面可达性
7. 最后准备答辩 demo

## 2. 基础编译

```bash
cmake -S . -B build
cmake --build build -j
```

## 3. 单元/集成测试

```bash
ctest --test-dir build --output-on-failure
```

用途：

- 验证跳表、WAL、协议解析和基础恢复逻辑

## 4. 协议回归

```bash
./scripts/verify_protocol_regression.sh
```

用途：

- 验证 `PING/PUT/GET/SCAN/DEL/QUIT` 主链路

协议格式与响应语义见 [protocol_reference.md](../docs/protocol_reference.md)。

## 5. WAL 恢复验证

推荐脚本：

```bash
./scripts/verify_wal_recovery.sh
```

脚本会自动完成：

- 清理旧数据目录
- 启动开启 WAL 的服务端
- 写入测试 key
- 停止服务端
- 重新启动服务端
- 验证重启后仍能读回 key

详细说明见 [wal_recovery_validation.md](../docs/wal_recovery_validation.md)。

当前状态：

- 本轮已在允许本地网络权限后实际跑通这条验证链路
- 如果后续仍处于受限沙箱，`kvstore_client` 的本地 TCP 连接可能被限制；若脚本报出 `socket failed: Operation not permitted`，需要在沙箱外或本地终端重新执行

## 6. 正式 benchmark

网络 benchmark：

```bash
./bin/kvstore_bench 127.0.0.1 6380 10000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 4
./scripts/run_network_bench.sh
```

说明：

- `kvstore_bench` 适合单条命令精确控制参数
- `run_network_bench.sh` 适合批量执行一组网络 benchmark
- 若需要查看脚本支持的环境变量，可执行 `./scripts/run_network_bench.sh --help`
- 完整 CLI 参数说明见 [cli_reference.md](../docs/cli_reference.md)

进程内对比：

```bash
./bin/kvstore_compare_bench 500 8
./scripts/run_compare_bench.sh
```

说明：

- `kvstore_compare_bench` 适合精确控制线程数、预加载规模和 workload
- `run_compare_bench.sh` 适合批量执行并导出结果
- 若需要查看脚本支持的环境变量，可执行 `./scripts/run_compare_bench.sh --help`
- 完整 CLI 参数说明见 [cli_reference.md](../docs/cli_reference.md)

用途：

- 这是当前论文中可引用的正式实验数据来源

## 7. Demo 页面可达性

```bash
./scripts/verify_demo_http.sh
```

用途：

- 启动 demo HTTP 服务并拉取页面
- 验证答辩展示静态页可访问

说明：

- 脚本内部会绕过本机代理，避免 `127.0.0.1` 访问被代理变量劫持

## 8. 答辩展示

```bash
python3 demo/defense_demo_server.py
curl --noproxy '*' http://127.0.0.1:8765/defense_dashboard.html
```

用途：

- 展示真实事件映射、协议交互、并发客户端和 WAL 恢复过程
- 确认本地 demo 页面可正常提供 HTTP 内容

边界：

- demo 用于展示，不替代正式 benchmark
