# KV-Store 请求处理流程说明

本文档用于描述当前主线系统中，一条客户端请求如何从网络接入一路走到存储执行、WAL 写入和响应返回。

## 1. 总体流程

在当前单线程 Reactor 主线中，一条请求的大致处理顺序为：

```text
客户端发送命令
-> socket 接收字节流
-> LineCodec 提取完整行
-> CommandProcessor 解析命令
-> KVStore 执行读写
-> 写操作先追加 WAL
-> 生成文本响应
-> Reactor 写回 socket
```

## 2. 网络接入阶段

核心模块：

- [Server.cpp](/home/haoo/code/study/KV-Store/src/Server.cpp)
- [Server.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Server.hpp)

说明：

- 服务端使用 `epoll` 监听连接事件和读写事件
- 连接 socket 为非阻塞模式
- 当前主线中，请求读取、协议解析、命令执行和响应写回都在同一个 Reactor 线程中完成

## 3. 协议解析阶段

核心模块：

- [Protocol.cpp](/home/haoo/code/study/KV-Store/src/Protocol.cpp)
- [Protocol.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Protocol.hpp)

说明：

- `LineCodec` 按 `\r\n` 切分完整命令
- 通过缓冲区保留半包内容，避免 TCP 粘包和半包导致命令错乱
- `CommandProcessor` 负责把文本命令映射为具体存储操作

当前支持的命令包括：

- `PING`
- `PUT <key> <value>`
- `GET <key>`
- `DEL <key>`
- `SCAN <start> <end>`
- `QUIT`

## 4. 存储执行阶段

核心模块：

- [kvstore.cpp](/home/haoo/code/study/KV-Store/src/kvstore.cpp)
- [kvstore.hpp](/home/haoo/code/study/KV-Store/include/kvstore/kvstore.hpp)
- [SkipList.hpp](/home/haoo/code/study/KV-Store/include/kvstore/SkipList.hpp)

说明：

- `KVStore` 对外提供统一的 `Put/Get/Delete/Scan`
- 内部索引结构为跳表
- `Scan` 利用有序键空间支持范围查询

## 5. WAL 处理阶段

核心模块：

- [WAL.cpp](/home/haoo/code/study/KV-Store/src/WAL.cpp)
- [WAL.hpp](/home/haoo/code/study/KV-Store/include/kvstore/WAL.hpp)

说明：

- 写操作在更新内存索引前先写 WAL
- WAL 采用追加写
- 系统启动时通过 replay 重建内存状态
- 若日志尾部存在不完整记录，会跳过尾部损坏数据

## 6. 响应返回阶段

说明：

- 存储执行完成后，结果会被编码成文本响应
- 常见响应包括：
  - `PONG`
  - `OK PUT`
  - `OK UPDATE`
  - `VALUE <value>`
  - `RESULT ...`
  - `OK DELETE`
  - `BYE`

## 7. 为什么当前主线保留单线程 Reactor

从请求流程角度看，当前实现的关键特点是：

- 入口读取在 Reactor 线程
- 协议解析在 Reactor 线程
- 响应写回也在 Reactor 线程

因此，如果并行化方案只是把中间一小段命令执行切给线程池，而没有改变入口读取和最终写回的串行结构，就可能引入额外调度成本，却没有真正消除关键串行点。

这也是当前线程池方案退化、主线保留单线程 Reactor 的重要背景。

## 8. 论文写作建议

论文中可将本文档内容直接用于：

- 请求处理时序说明
- 协议层与存储层的交互描述
- “为什么线程池方案没有进入主线”的背景解释
