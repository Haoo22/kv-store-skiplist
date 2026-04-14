# KV-Store 系统架构说明

本文档用于描述当前最终答辩版本的系统结构。

## 1. 总体架构

当前项目可抽象为四层：

1. 接入层：客户端与文本协议
2. 网络与调度层：单线程 Reactor
3. 存储与持久化层：`KVStore + SkipList + WAL`
4. 验证层：tests、benchmark、恢复脚本

## 2. 结构图

```text
+---------------------------+
| kvstore_client            |
| kvstore_bench             |
+------------+--------------+
             |
             v
+---------------------------+
| Text Protocol             |
| PING PUT GET DEL SCAN     |
| QUIT                      |
+------------+--------------+
             |
             v
+---------------------------+
| Single-Thread Reactor     |
| epoll + non-block socket  |
| read / parse / execute    |
| write response            |
+------------+--------------+
             |
      +------+------+
      |             |
      v             v
+------------+  +----------------+
| SkipList   |  | WAL            |
| ordered KV |  | append + replay|
+------------+  +----------------+
      |             |
      +------+------+
             |
             v
+---------------------------+
| Tests & Benchmarks        |
| ctest / compare bench /   |
| WAL recovery scripts      |
+---------------------------+
```

## 3. 请求处理流程

1. 客户端通过 TCP 发送文本命令
2. 服务端单线程 Reactor 通过 `epoll` 感知连接读写事件
3. `LineCodec` 从字节流中按 `\r\n` 提取完整命令
4. `CommandProcessor` 解析命令并调用 `KVStore`
5. 写操作先记录 WAL，再更新跳表
6. 结果被编码成文本响应并写回客户端

## 4. 当前主线设计选择

当前主线固定为单线程 Reactor，原因如下：

- 它已经形成稳定的协议、WAL 和 benchmark 闭环
- 与开题报告相比，网络模型做了必要收敛，但核心目标仍保留
- 存储层把优化重点放在节点级锁跳表

## 5. 存储层与持久化关系

- `SkipList` 负责有序键值索引
- 跳表内部采用节点级锁控制局部并发访问
- `KVStore` 对外提供统一的 `Put/Get/Delete/Scan`
- `WAL` 负责写前追加日志与重启恢复

## 6. 论文建议口径

论文和答辩中建议固定写成：

- “系统采用单线程 Reactor 处理网络事件”
- “存储层采用节点级锁跳表实现细粒度并发控制”
- “系统通过 WAL 获得基础持久化与恢复能力”
