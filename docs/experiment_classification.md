# KV-Store 实验分类说明

本文档用于明确当前项目中不同实验、验证和展示链路的用途，避免把协议验证、正式 benchmark 和答辩 demo 混为一谈。

## 1. 分类总览

| 类别 | 工具/入口 | 目标 | 是否可作为正式性能结论 |
| --- | --- | --- | --- |
| 单元/集成测试 | `ctest --test-dir build --output-on-failure` | 验证基础正确性 | 否 |
| 协议冒烟测试 | `kvstore_client` | 手工检查命令可用性 | 否 |
| 协议回归测试 | `verify_protocol_regression.sh` | 脚本化验证完整协议主链路 | 否 |
| 外部协议验证 | `packetsender` | 使用独立 CLI 验证 TCP 文本协议 | 否 |
| 正式网络 benchmark | `kvstore_bench` `run_network_bench.sh` | 测单客户端吞吐、pipeline 上限、多客户端 aggregate QPS | 是 |
| 进程内对比压测 | `kvstore_compare_bench` `run_compare_bench.sh` | 对比不同存储结构和持久化路径 | 是 |
| Demo 可达性验证 | `verify_demo_http.sh` | 验证答辩展示页面可访问 | 否 |
| 答辩展示 demo | `demo/defense_demo_server.py` | 可视化展示真实事件与实验结论 | 否 |

## 2. 单元/集成测试

命令：

```bash
ctest --test-dir build --output-on-failure
```

作用：

- 验证跳表、WAL、协议解析和基础正确性

## 3. 协议验证

### 3.1 手工协议冒烟

命令：

```bash
./bin/kvstore_server --no-wal
./bin/kvstore_client 127.0.0.1 6380
```

### 3.2 脚本化协议回归

命令：

```bash
./scripts/verify_protocol_regression.sh
```

作用：

- 自动验证 `PING -> PUT -> GET -> SCAN -> DEL -> QUIT` 主链路
- 同时检查请求构造和响应读取逻辑

协议格式与响应语义见 [protocol_reference.md](/home/haoo/code/study/KV-Store/docs/protocol_reference.md)。

### 3.3 外部协议验证

命令模板：

```bash
packetsender -A -t -4 -w 1000 127.0.0.1 6380 $'COMMAND\r\n'
```

作用：

- 提供一个独立于项目代码的外部 TCP 客户端
- 用于功能回归和报文级验证

## 4. 正式 benchmark

### 4.1 网络 benchmark

命令示例：

```bash
./bin/kvstore_bench 127.0.0.1 6380 10000 64
./bin/kvstore_bench 127.0.0.1 6380 500 8 put-get 4
./scripts/run_network_bench.sh
```

作用：

- 测单客户端吞吐
- 测 pipeline 上限
- 测多客户端 aggregate QPS

说明：

- `kvstore_bench` 是底层正式工具
- `run_network_bench.sh` 是包装脚本，适合批量跑一组网络 benchmark
- 参数细节见 [cli_reference.md](/home/haoo/code/study/KV-Store/docs/cli_reference.md)

### 4.2 进程内对比 benchmark

命令示例：

```bash
./bin/kvstore_compare_bench 500 8
./scripts/run_compare_bench.sh
```

作用：

- 对比 `kvstore_no_wal`
- 对比 `kvstore_with_wal`
- 对比 `std_map_mutex`
- 对比 `std_map_mutex_wal`
- 对比 `skiplist_sharded`
- 对比 `skiplist_sharded_wal`
- 对比 `std_map_sharded`
- 对比 `std_map_sharded_wal`

说明：

- `kvstore_compare_bench` 是底层正式工具
- `run_compare_bench.sh` 是包装脚本，适合批量输出或保存结果
- `*_wal` 是 compare benchmark 内部补充的实验性 WAL 包装对照
- 参数细节见 [cli_reference.md](/home/haoo/code/study/KV-Store/docs/cli_reference.md)

## 5. 答辩展示 demo

### 5.1 页面可达性验证

命令：

```bash
./scripts/verify_demo_http.sh
```

作用：

- 启动 demo HTTP 服务并拉取页面
- 避免页面资源损坏或本机代理影响回环访问

### 5.2 正式展示

命令：

```bash
python3 demo/defense_demo_server.py
```

访问：

```text
http://127.0.0.1:8765/defense_dashboard.html
```

边界：

- demo 使用真实事件映射驱动前端
- 但 demo 面板中的动态指标受展示节奏和前端逻辑影响，不能替代正式 benchmark 结果

## 6. 论文写作建议

论文中建议按下面口径引用：

- 正确性验证：引用测试和协议验证链路
- 性能结论：只引用 `kvstore_bench` 和 `kvstore_compare_bench`
- 展示效果：引用 demo 的交互方式和答辩价值，但不把 demo 指标当成正式实验结果

更细的 benchmark 统计口径见 [benchmark_methodology.md](/home/haoo/code/study/KV-Store/docs/benchmark_methodology.md)。
