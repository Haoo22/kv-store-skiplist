# KV-Store 系统架构说明

本文档把当前项目的系统结构、请求数据流和验证支撑层抽成论文可复用的架构说明。

## 1. 总体架构

当前项目可抽象为四层：

1. 接入层：客户端与文本协议
2. 网络与调度层：单线程 Reactor
3. 存储与持久化层：`KVStore + SkipList + WAL`
4. 验证与展示层：tests、benchmark、demo

## 2. 结构图

```text
+---------------------------+
|   kvstore_client          |
|   packetsender            |
|   kvstore_bench           |
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
| Verification & Demo       |
| tests / compare bench /   |
| defense dashboard         |
+---------------------------+
```

## 3. 请求处理数据流

当前主线请求处理过程如下：

1. 客户端通过 TCP 发送文本命令
2. 服务端的单线程 Reactor 通过 `epoll` 感知连接读写事件
3. `LineCodec` 从字节流中按 `\r\n` 提取完整命令
4. `CommandProcessor` 解析命令并调用 `KVStore`
5. 写操作先记录 WAL，再更新内存索引
6. 读写结果被编码成文本响应并写回客户端

## 4. 主线设计选择

当前主线固定为单线程 Reactor，原因不是“没有考虑并发”，而是：

- 单线程版本已经形成稳定的协议、WAL、benchmark 和 demo 闭环
- 线程池方案已做过端到端实验，但 benchmark 显示明显退化
- 因此主线设计保留单线程 Reactor，把线程池方案作为反例分析材料保留

## 5. 存储与持久化关系

存储层和 WAL 的关系可概括为：

- `SkipList` 负责有序键值索引
- `KVStore` 负责对外提供统一读写接口
- `WAL` 负责把写操作追加到日志
- 系统重启后通过日志重放恢复内存状态

这条链路直接支撑论文中的“轻量级但具备基本可恢复能力”。

## 6. 验证与展示支撑层

当前项目并不是只有主线代码，还配了一层完整的验证与展示支撑：

- `ctest`：基础正确性
- `kvstore_bench`：正式网络 benchmark
- `kvstore_compare_bench`：进程内对比实验
- `kvstore_client` / `packetsender`：协议验证
- `demo/defense_demo_server.py` + `defense_dashboard.html`：答辩展示

## 7. 论文写作建议

论文中建议把这个架构图作为：

- 系统总体结构图
- 请求处理流程图的基础
- “为什么主线保留单线程 Reactor”的说明背景
