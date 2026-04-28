# KV-Store RESP-like 协议参考

本文档定义服务端当前支持的 RESP-like 长度前缀协议。

## 1. 传输格式

- 传输层为 TCP
- 请求使用 RESP-like 数组编码
- 每个 bulk string 长度与内容之间使用 `\r\n` 分隔
- 命令字大小写不敏感
- 参数按数组元素传递，不再依赖空白切分

## 2. 通用规则

- key 建议使用不含空白与控制字符的可打印字符串
- value 作为独立 bulk string 传递，可以包含空格
- 空请求返回 `ERROR empty command`
- 未识别命令返回 `ERROR unknown command`

基础示例：

```text
*1\r\n
$4\r\n
PING\r\n
```

## 3. 命令定义

### 3.1 `PING`

请求：

```text
*1\r\n
$4\r\n
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
*3\r\n
$3\r\n
PUT\r\n
$4\r\n
user\r\n
$5\r\n
alice\r\n

*3\r\n
$3\r\n
PUT\r\n
$5\r\n
title\r\n
$11\r\n
hello world\r\n
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
- value 通过长度前缀传递，可以安全包含空格

### 3.3 `GET <key>`

请求：

```text
*2\r\n
$3\r\n
GET\r\n
$4\r\n
user\r\n
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
*2\r\n
$3\r\n
DEL\r\n
$4\r\n
user\r\n
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
*3\r\n
$4\r\n
SCAN\r\n
$1\r\n
a\r\n
$1\r\n
z\r\n
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
*1\r\n
$4\r\n
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
*1\r\n
$10\r\n
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

服务端使用 `RequestCodec` 缓冲连接上的字节流，只有在检测到完整数组头及全部 bulk string 内容后才会交给命令处理器，因此能够正确处理常见的 TCP 粘包与半包。
