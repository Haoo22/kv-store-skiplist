# KV-Store 文本协议参考

本文档定义服务端当前支持的文本协议。

## 1. 传输格式

- 传输层为 TCP
- 每条命令以 `\r\n` 结尾
- 命令字大小写不敏感
- 参数按空白分隔
- 服务端会裁剪命令两端空白

## 2. 通用规则

- key 不能包含空白字符
- `PUT` 的 value 为 key 之后的剩余文本
- value 中间可以包含空格
- value 首尾空白不会保留
- 空命令返回 `ERROR empty command`
- 未识别命令返回 `ERROR unknown command`

## 3. 命令定义

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

错误响应：

```text
ERROR usage: PUT <key> <value>\r\n
```

说明：

- 新插入返回 `OK PUT`
- 覆盖已有 key 返回 `OK UPDATE`

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

- 返回满足 `start <= key <= end` 的键值对
- 返回格式为 `RESULT <count> <key>=<value> ...`
- 当 `end < start` 时返回 `RESULT 0\r\n`

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

### 3.7 `CHECKPOINT`

请求：

```text
CHECKPOINT\r\n
```

成功响应：

```text
OK CHECKPOINT\r\n
```

错误响应：

```text
ERROR usage: CHECKPOINT\r\n
ERROR checkpoint unavailable\r\n
ERROR checkpoint failed\r\n
```

说明：

- 该命令会把当前内存数据写成快照文件
- 快照成功后会截断当前 WAL 文件
- 当服务端以 `--no-wal` 运行时，返回 `ERROR checkpoint unavailable\r\n`

## 4. TCP 粘包与半包

服务端使用 `LineCodec` 缓冲连接上的字节流，只有在检测到完整 `\r\n` 结尾后才会交给命令处理器，因此能够正确处理常见的 TCP 粘包与半包。
