# KV-Store 请求处理流程

本文档描述一条请求从网络接入到响应返回的执行路径。

## 1. 总体流程

```text
客户端发送命令
-> 服务端读取 socket
-> LineCodec 按 \r\n 提取完整行
-> CommandProcessor 解析命令
-> KVStore 执行读写
-> 写操作先写 WAL
-> 生成文本响应
-> Reactor 写回 socket
```

## 2. 网络接入

核心文件：

- [src/Server.cpp](../src/Server.cpp)
- [include/kvstore/Server.hpp](../include/kvstore/Server.hpp)

说明：

- 服务端使用 `epoll` 监听连接和读写事件
- 连接 socket 为非阻塞模式
- 读取、解析、执行和写回都在同一个 Reactor 线程中完成

## 3. 协议解析

核心文件：

- [src/Protocol.cpp](../src/Protocol.cpp)
- [include/kvstore/Protocol.hpp](../include/kvstore/Protocol.hpp)

说明：

- `LineCodec` 负责按 `\r\n` 切分完整命令
- 半包内容会保留在连接缓冲区中
- `CommandProcessor` 负责参数解析与命令分发

支持命令：

- `PING`
- `PUT <key> <value>`
- `GET <key>`
- `DEL <key>`
- `SCAN <start> <end>`
- `QUIT`

## 4. 存储执行

核心文件：

- [src/kvstore.cpp](../src/kvstore.cpp)
- [include/kvstore/kvstore.hpp](../include/kvstore/kvstore.hpp)
- [include/kvstore/SkipList.hpp](../include/kvstore/SkipList.hpp)

说明：

- `KVStore` 提供统一的 `Put`、`Get`、`Delete`、`Scan` 接口
- 跳表负责有序键值索引
- 单 key 操作基于节点级锁与原子前向指针完成更新
- `Scan` 在有序底层链表上执行范围遍历，并通过节点状态检查过滤未完全链接或已删除节点
- 删除节点会先从跳表链路中摘除，节点内存延迟到 `Clear` 或跳表析构时统一释放，以避免并发读遍历到已释放内存

## 5. WAL 处理

核心文件：

- [src/WAL.cpp](../src/WAL.cpp)
- [include/kvstore/WAL.hpp](../include/kvstore/WAL.hpp)

说明：

- `PUT` 和 `DEL` 在更新内存前先追加 WAL
- 重启时通过 replay 重建内存状态
- 不完整的尾部记录会被跳过

## 6. 响应返回

执行结果会编码成文本响应并写回客户端，常见响应包括：

- `PONG`
- `OK PUT`
- `OK UPDATE`
- `VALUE <value>`
- `NOT_FOUND`
- `RESULT ...`
- `OK DELETE`
- `BYE`
