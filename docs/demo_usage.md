# KV-Store Demo 使用说明

本文档说明答辩展示 demo 的定位、启动方式、演示模式和使用边界。

## 1. 定位

当前 demo 不是纯静态页面，而是“真实事件映射到前端”的本地展示面板。

它的作用是：

- 让老师快速看到系统正在运行
- 把协议交互、并发客户端、WAL 恢复和实验结论可视化
- 服务答辩和阶段汇报

它不承担的作用是：

- 不替代正式 benchmark
- 不作为论文性能结论的直接数据来源
- 不把页面中的动态 QPS、延迟和柱状图直接当成正式实验表格

## 2. 组成

文件：

- [defense_dashboard.html](/home/haoo/code/study/KV-Store/demo/defense_dashboard.html)
- [defense_demo_server.py](/home/haoo/code/study/KV-Store/demo/defense_demo_server.py)
- [defense_demo_prompt.md](/home/haoo/code/study/KV-Store/docs/defense_demo_prompt.md)

说明：

- 前端页面使用原生 HTML/CSS/JavaScript
- 后端使用本地 HTTP + SSE 服务推送真实事件
- 页面不依赖 npm、CDN 或构建工具

## 3. 启动方式

在项目根目录执行：

```bash
python3 demo/defense_demo_server.py
```

然后访问：

```text
http://127.0.0.1:8765/defense_dashboard.html
```

如需做命令行抓取验证，可执行：

```bash
curl --noproxy '*' http://127.0.0.1:8765/defense_dashboard.html
```

## 4. 演示模式

当前支持三种模式：

### 4.1 协议演示

重点展示：

- `PING`
- `PUT`
- `GET`
- `SCAN`
- `DEL`
- `QUIT`

### 4.2 并发演示

重点展示：

- 多客户端同时发送命令
- 客户端进度条变化
- 指标卡和动态图表联动

说明：

- 这一模式强调“系统在运行、事件在发生、指标在变化”
- 页面中的吞吐和延迟属于展示型指标，正式性能结论仍应引用 `kvstore_bench`

### 4.3 WAL 恢复

重点展示：

- 写入数据
- 模拟服务退出
- 通过 WAL 恢复内存状态

## 5. 推荐答辩讲解顺序

1. 先用“协议演示”说明这不是静态图，而是系统真实交互
2. 再切到“并发演示”展示多客户端和指标变化
3. 最后用“WAL 恢复”收束到系统可用性和数据安全

## 6. 使用边界

需要明确告诉老师或写入答辩材料的点：

- demo 中的动态 QPS 和延迟是展示型指标
- 正式性能结论应以 `kvstore_bench` 和 `kvstore_compare_bench` 为准
- demo 的价值在于把系统运行状态与实验结论可视化，而不是替代实验本身
