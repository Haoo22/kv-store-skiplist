# KV-Store 系统架构

本文档说明项目的模块划分和主要组件关系。

## 1. 分层结构

项目可以分为四层：

1. 接入层
2. 网络与调度层
3. 存储与持久化层
4. 测试与验证层

## 2. 架构图

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
| ctest / scripts / bench   |
+---------------------------+
```

## 3. 接入层

接入层包括：

- `kvstore_client`
- `kvstore_bench`
- 其他基于 TCP 的外部客户端

这一层负责建立 TCP 连接并发送文本协议命令。

## 4. 网络与调度层

核心文件：

- [src/Server.cpp](../src/Server.cpp)
- [include/kvstore/Server.hpp](../include/kvstore/Server.hpp)
- [src/Protocol.cpp](../src/Protocol.cpp)
- [include/kvstore/Protocol.hpp](../include/kvstore/Protocol.hpp)

职责：

- 接受连接
- 管理 `epoll` 事件
- 读取和写回 socket
- 处理粘包与半包
- 将文本命令分发给存储层

## 5. 存储与持久化层

核心文件：

- [src/kvstore.cpp](../src/kvstore.cpp)
- [include/kvstore/kvstore.hpp](../include/kvstore/kvstore.hpp)
- [include/kvstore/SkipList.hpp](../include/kvstore/SkipList.hpp)
- [src/WAL.cpp](../src/WAL.cpp)
- [include/kvstore/WAL.hpp](../include/kvstore/WAL.hpp)

职责：

- 使用跳表维护有序键值索引
- 提供 `Put`、`Get`、`Delete`、`Scan`
- 对写操作进行 WAL 追加写
- 在服务启动时重放 WAL 恢复状态

## 6. 测试与验证层

包括：

- `ctest`
- `kvstore_tests`
- `verify_protocol_regression.sh`
- `verify_wal_recovery.sh`
- `kvstore_bench`
- `kvstore_compare_bench`

这一层用于验证协议、恢复逻辑、并发行为和性能表现。
