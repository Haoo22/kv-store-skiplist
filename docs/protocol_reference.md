# KV-Store 文本协议参考

本文档固定记录当前主线服务端的文本协议行为，作为 README、答辩演示和论文协议章节的统一依据。

## 1. 传输格式

- 传输层为 TCP
- 每条命令以 `\r\n` 结尾
- 服务端按行解析，`LineCodec` 会在连接级别缓存半包并按 `\r\n` 提取完整命令
- 命令字大小写不敏感，服务端会先转换为大写再分发
- 命令行首尾空白会被裁剪

实现依据：

- [Protocol.hpp](../include/kvstore/Protocol.hpp)
- [Protocol.cpp](../src/Protocol.cpp)

## 2. 基本语义

- key 不能包含空白字符，因为协议按空白分隔命令参数
- `PUT` 的 value 取 key 之后的整段剩余文本，因此 value 可以包含中间空格
- 由于服务端会先整体 `Trim`，value 的首尾空白不会被保留
- 空命令会返回 `ERROR empty command`
- 未识别命令会返回 `ERROR unknown command`

## 3. 命令与响应

### 3.1 `PING`

请求：

```text
PING\r\n
```

成功响应：

```text
PONG\r\n
```

错误响应：

```text
ERROR usage: PING\r\n
```

说明：

- `PING` 不接受额外参数

### 3.2 `PUT <key> <value>`

请求：

```text
PUT user alice\r\n
PUT title hello world\r\n
```

成功响应：

```text
OK PUT\r\n
OK UPDATE\r\n
```

说明：

- 第一次插入 key 返回 `OK PUT`
- 覆盖已有 key 返回 `OK UPDATE`
- value 可以包含空格，但 key 不能包含空格

错误响应：

```text
ERROR usage: PUT <key> <value>\r\n
```

### 3.3 `GET <key>`

请求：

```text
GET user\r\n
```

成功响应：

```text
VALUE alice\r\n
```

未命中响应：

```text
NOT_FOUND\r\n
```

错误响应：

```text
ERROR usage: GET <key>\r\n
```

### 3.4 `DEL <key>`

请求：

```text
DEL user\r\n
```

成功响应：

```text
OK DELETE\r\n
```

未命中响应：

```text
NOT_FOUND\r\n
```

错误响应：

```text
ERROR usage: DEL <key>\r\n
```

### 3.5 `SCAN <start> <end>`

请求：

```text
SCAN a z\r\n
```

成功响应：

```text
RESULT 2 apple=1 book=2\r\n
```

说明：

- `SCAN` 返回格式为 `RESULT <count> <key>=<value> ...`
- 当前跳表实现对范围边界采用闭区间语义，即返回满足 `start <= key <= end` 的键值对
- 如果 `end < start`，服务端会返回 `RESULT 0\r\n`

错误响应：

```text
ERROR usage: SCAN <start> <end>\r\n
```

### 3.6 `QUIT`

请求：

```text
QUIT\r\n
```

成功响应：

```text
BYE\r\n
```

错误响应：

```text
ERROR usage: QUIT\r\n
```

说明：

- 服务端返回 `BYE` 后会关闭当前连接

## 4. 粘包与半包处理

- 客户端可连续写入多条命令
- 服务端每次读 socket 后都会将字节流追加到 `LineCodec` 缓冲区
- 只有在发现完整 `\r\n` 时才会交给 `CommandProcessor`
- 因此该协议可处理常见的 TCP 粘包和半包问题

## 5. 与验证工具的关系

- 手工协议验证：使用 [client_main.cpp](../src/client_main.cpp) 对应的 `kvstore_client`
- 脚本化协议回归：使用 `./bin/kvstore_bench 127.0.0.1 6380 20 1 full`
- 外部协议验证：使用 `packetsender`
- 正式性能结论：只引用 `kvstore_bench` 与 `kvstore_compare_bench`，不引用本协议文档中的示例交互
