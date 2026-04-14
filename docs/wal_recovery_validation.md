# KV-Store WAL 恢复验证

本文档说明如何验证项目的 WAL 恢复能力。

## 1. 验证目标

确认以下链路成立：

1. 启动启用 WAL 的服务端
2. 写入键值数据
3. 停止服务端
4. 重启服务端
5. 通过 `GET` 读回写入结果

## 2. 推荐命令

```bash
./scripts/verify_wal_recovery.sh
```

## 3. 脚本逻辑

脚本会自动执行：

1. 清理 `data/`
2. 编译项目
3. 启动启用 WAL 的 `kvstore_server`
4. 通过 `kvstore_client` 写入测试数据
5. 停止服务端
6. 再次启动服务端
7. 通过 `kvstore_client` 执行 `GET`
8. 校验结果

## 4. 成功条件

脚本成功时会输出：

```text
WAL recovery verification passed
```

## 5. 注意事项

- 脚本会重置项目根目录下的 `data/` 目录
- 执行前应确认没有需要保留的本地测试数据
- 脚本依赖本地 TCP 回环和服务端进程启动能力
