# KV-Store WAL 恢复验证说明

本文档说明如何验证当前项目的 WAL 恢复能力。

## 1. 验证目标

确认以下链路成立：

1. 服务端启动并开启 WAL
2. 写入 key-value 数据
3. 服务端退出
4. 服务端重新启动
5. 数据仍能通过 `GET` 读回

## 2. 推荐脚本

```bash
./scripts/verify_wal_recovery.sh
```

## 3. 脚本执行逻辑

脚本会自动执行以下步骤：

1. 清理 `data/`
2. 编译项目
3. 启动开启 WAL 的 `kvstore_server`
4. 通过 `kvstore_client` 写入测试数据
5. 停止服务端
6. 再次启动服务端
7. 通过 `kvstore_client` 执行 `GET`
8. 检查是否返回预期值

## 4. 预期结果

脚本成功时会输出：

```text
WAL recovery verification passed
```

如果恢复失败，脚本会退出并返回非零状态。

## 5. 环境说明

- 脚本会重置项目根目录下的 `data/` 目录
- 因此执行前应确认没有需要保留的本地测试数据
- 脚本依赖本地可启动 `kvstore_server` 和 `kvstore_client`
- 本轮已在放开本地网络权限后实际跑通，输出为 `WAL recovery verification passed`
- 如果处于受限沙箱，仍可能因本地 TCP 连接限制失败；若出现 `socket failed: Operation not permitted`，应改在本地终端或沙箱外执行

## 6. 论文中的使用口径

论文中可把这条验证链路描述为：

- 该系统采用 WAL 记录写操作
- 系统重启后通过日志重放恢复内存状态
- 恢复能力不仅在单元测试中验证，也通过独立脚本完成端到端验证
