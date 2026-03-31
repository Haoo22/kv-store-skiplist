# KV-Store

基于跳表的高性能键值存储引擎，使用现代 C++、Linux 原生 Socket API、`epoll` 和 CMake 实现。当前版本已经完成跳表索引、WAL 持久化恢复、基于 Reactor 的服务端、交互式客户端和基础压力测试工具。

## 1. 项目概述

本项目面向毕业设计场景，核心目标是实现一个具备以下特性的轻量级键值存储系统：

- 内存索引采用跳表，支持 `Put`、`Get`、`Delete`、`Scan`
- 持久化采用预写日志 WAL，支持追加写和重启恢复
- 网络模型采用 `epoll + 非阻塞 socket + 单线程 Reactor`
- 协议采用 `\r\n` 结尾的文本命令，能够处理 TCP 粘包和半包
- 提供配套客户端和基础压测工具，便于实验展示和性能分析

## 2. 目录结构

```text
KV-Store/
├── CMakeLists.txt
├── README.md
├── include/kvstore/
│   ├── Protocol.hpp
│   ├── Server.hpp
│   ├── SkipList.hpp
│   ├── WAL.hpp
│   └── kvstore.hpp
├── src/
│   ├── Protocol.cpp
│   ├── Server.cpp
│   ├── WAL.cpp
│   ├── benchmark_main.cpp
│   ├── client_main.cpp
│   ├── kvstore.cpp
│   └── server_main.cpp
├── tests/
│   └── test_main.cpp
└── bin/
```

## 3. 核心模块说明

### 3.1 跳表索引

- 文件：[SkipList.hpp](/home/haoo/code/study/KV-Store/include/kvstore/SkipList.hpp)
- 采用泛型模板实现，支持不同键值类型
- 使用随机层级生成算法，平均时间复杂度为 `O(log N)`
- 支持范围查询 `Scan(start, end)`，适合有序键遍历
- 节点使用智能指针管理，符合 RAII 风格

### 3.2 WAL 持久化

- 文件：[WAL.hpp](/home/haoo/code/study/KV-Store/include/kvstore/WAL.hpp)、[WAL.cpp](/home/haoo/code/study/KV-Store/src/WAL.cpp)
- 采用 append-only 二进制日志格式
- 写入顺序为：先写 WAL，再更新内存索引
- 每条记录包含操作类型、键长度、值长度和校验信息
- 系统重启时自动重放日志恢复内存数据
- 对日志尾部不完整记录进行跳过处理，避免异常断电后恢复失败

### 3.3 网络层与 Reactor

- 文件：[Server.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Server.hpp)、[Server.cpp](/home/haoo/code/study/KV-Store/src/Server.cpp)
- 使用 `epoll` 进行 I/O 多路复用
- 使用非阻塞监听 socket 和连接 socket
- 使用单线程 Reactor 循环处理连接、读写事件和命令响应
- 协议解析与业务处理分离，便于维护和测试

### 3.4 协议层

- 文件：[Protocol.hpp](/home/haoo/code/study/KV-Store/include/kvstore/Protocol.hpp)、[Protocol.cpp](/home/haoo/code/study/KV-Store/src/Protocol.cpp)
- `LineCodec` 负责按照 `\r\n` 提取完整命令
- `CommandProcessor` 负责命令分发与响应生成
- 已支持命令：
  - `PING`
  - `PUT <key> <value>`
  - `GET <key>`
  - `DEL <key>`
  - `SCAN <start> <end>`
  - `QUIT`

## 4. 构建方法

在项目根目录执行：

```bash
cmake -S . -B build
cmake --build build -j
```

编译后生成的主要可执行文件位于 `bin/`：

- `kvstore_server`
- `kvstore_client`
- `kvstore_bench`
- `kvstore_tests`

## 5. 测试方法

执行单元/集成测试：

```bash
ctest --test-dir build --output-on-failure
```

当前测试覆盖了以下内容：

- 跳表插入、更新、删除、范围查询
- WAL 追加写、重放恢复
- 截断日志尾部容错
- 协议分包、命令解析和响应正确性

## 6. 启动与使用

### 6.1 启动服务端

```bash
./bin/kvstore_server
```

默认监听：

- 地址：`0.0.0.0`
- 端口：`6380`

启动后会看到类似输出：

```text
KVStore server listening on 0.0.0.0:6380, WAL path: data/wal.log
```

### 6.2 启动客户端

```bash
./bin/kvstore_client 127.0.0.1 6380
```

示例交互：

```text
> PUT user alice
OK PUT
> GET user
VALUE alice
> SCAN a z
RESULT 1 user=alice
> DEL user
OK DELETE
> QUIT
BYE
```

### 6.3 运行基础压测

```bash
./bin/kvstore_bench 127.0.0.1 6380 1000
```

参数含义：

- 第 1 个参数：服务端 IP
- 第 2 个参数：服务端端口
- 第 3 个参数：每个阶段的操作次数

输出示例：

```text
PUT      ops=1000 elapsed=3.8123s throughput=262.31 ops/s avg_latency=3812.30 us
GET      ops=1000 elapsed=0.0561s throughput=17825.31 ops/s avg_latency=56.10 us
```

## 7. 文本协议说明

所有请求与响应均以 `\r\n` 结束。

### 请求格式

```text
PING\r\n
PUT key value\r\n
GET key\r\n
DEL key\r\n
SCAN start end\r\n
QUIT\r\n
```

### 响应格式

```text
PONG\r\n
OK PUT\r\n
OK UPDATE\r\n
VALUE <value>\r\n
OK DELETE\r\n
NOT_FOUND\r\n
RESULT <count> k1=v1 k2=v2 ...\r\n
BYE\r\n
ERROR ...\r\n
```

## 8. 当前实现的技术特点

- 使用 RAII 管理文件描述符和资源释放
- 使用智能指针管理核心对象生命周期
- 内存索引与持久化解耦，便于后续扩展 SSTable 或 LSM
- 协议层与事件循环解耦，便于单独测试
- 工程结构清晰，便于继续扩展多线程、配置文件和更完整的压测框架

## 9. 复杂对象序列化约束与接入说明

当前版本的 KV-Store 只直接支持：

- `std::string -> std::string` 键值存储
- 文本协议下的 `PUT <key> <value>` 命令
- WAL 中对字符串键值的持久化记录

这意味着系统本身不负责复杂 C++ 对象的自动序列化，也没有内建对象映射框架。复杂对象如果要存入本系统，必须先由业务层自行序列化成字符串或字节串，再写入 KV-Store。

### 9.1 当前没有内建支持的对象类型

以下内容不应直接序列化后写入本系统：

- 裸指针、智能指针中的地址值
- 引用、函数指针、虚函数表相关运行时状态
- `std::mutex`、`std::thread`、条件变量等同步原语
- 文件描述符、socket、进程句柄等操作系统资源
- 依赖当前进程地址空间的对象内部状态

原因很直接：这些内容在进程重启、机器重启或跨机器传输后都没有稳定语义，存储它们的“值”通常没有可恢复意义。

### 9.2 推荐的接入方式

推荐使用下面的业务层流程：

1. 将业务对象转换为稳定格式
2. 把结果编码成 `std::string`
3. 调用 `Put(key, serialized_value)` 写入
4. 读取时用 `Get(key, &value)` 拿到字符串
5. 再由业务层反序列化为对象

推荐的稳定格式包括：

- JSON
- Protocol Buffers
- MessagePack
- 自定义二进制结构

### 9.3 一个简单的接入示例

假设业务对象为：

```cpp
struct User {
    std::string name;
    int age;
};
```

一种简单做法是业务层自行实现：

```cpp
std::string SerializeUser(const User& user);
User DeserializeUser(const std::string& data);
```

然后以如下方式使用：

```cpp
User user {"alice", 20};
std::string encoded = SerializeUser(user);
store.Put("user:1001", encoded);

std::string raw;
if (store.Get("user:1001", &raw)) {
    User restored = DeserializeUser(raw);
}
```

### 9.4 设计约束建议

如果你要把本项目写进毕业设计，建议在论文或接口文档中明确声明：

- 存储引擎层只保证字符串键值的存储与恢复
- 对象序列化与反序列化由上层业务负责
- 不支持直接持久化进程态资源和内存地址
- 复杂对象的兼容性、版本迁移和字段演进由序列化协议负责

这样接口边界会非常清楚，也更符合 KV 存储引擎的职责划分。

## 10. 可用于毕业设计论文的实验设计建议

可以从以下几个方向组织实验章节：

### 10.1 功能正确性实验

- 测试 `Put/Get/Delete/Scan` 是否正确
- 测试服务端是否能处理多个连续命令
- 测试 WAL 是否能在重启后恢复数据
- 测试异常截断日志后系统是否仍能恢复有效记录

### 10.2 性能实验

- 固定 value 长度，逐步增加操作次数，统计吞吐量和平均时延
- 对比 `PUT` 和 `GET` 的性能差异
- 对比冷启动和日志恢复后的性能变化
- 扩展后可对比单线程版本与多线程版本

### 10.3 协议与网络实验

- 构造粘包、半包场景，验证 `\r\n` 分隔策略有效性
- 记录 `epoll` 单线程下的请求处理能力
- 分析高并发下瓶颈位置，如日志刷盘和单连接串行请求

## 11. 当前版本的局限性

- 当前为单线程 Reactor，尚未实现多线程工作池
- 仅支持文本协议，未实现二进制协议
- 压测工具为基础串行压测，尚未实现多连接并发压测
- WAL 只有日志追加与重放，尚未实现日志压缩和快照
- 服务端启动参数暂未做配置文件化

## 12. 后续可扩展方向

- 引入 `shared_mutex` 提升读多写少场景性能
- 实现线程池或主从 Reactor 模型
- 增加配置文件和命令行参数解析
- 增加更完整的 benchmark 统计，如 P95/P99 延迟
- 引入快照、SSTable 或 compaction 机制
- 增加客户端自动脚本和更系统的实验报告数据采集

## 13. Git 提交记录

当前项目关键迭代如下：

```text
4acb5c4 chore: bootstrap KV-Store project skeleton and skip list
251f4ff feat: add WAL persistence and recovery
171cbcb feat: add epoll reactor server and text protocol
d435fda feat: add client CLI and benchmark tool
```

如果你要把这个项目继续打磨成更完整的毕业设计版本，建议下一步优先补充：

1. 配置文件和命令行参数
2. 并发压测工具
3. 更多实验数据采集脚本
4. 论文中的系统设计图和时序图
