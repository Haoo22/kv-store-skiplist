# KV-Store 开发日志索引

本文档用于给 [README.md](/home/haoo/code/study/KV-Store/README.md) 中 `15. 开发日志` 提供一个快速导航，避免后续论文写作、答辩准备或问题回顾时直接在长日志里盲找。

## 1. 使用原则

- `1 ~ 14` 节仍是主说明书，应优先引用
- `15. 开发日志` 保留真实问题链路，适合回答“为什么最后这样做”
- 如果论文正文只需要结论，应优先引用独立摘要文档，而不是直接引用长日志

## 2. 推荐取材顺序

1. 先看 [thesis_materials.md](/home/haoo/code/study/KV-Store/docs/thesis_materials.md)
2. 再看 [benchmark_methodology.md](/home/haoo/code/study/KV-Store/docs/benchmark_methodology.md)
3. 如果需要线程池反例，优先看 [thread_pool_findings.md](/home/haoo/code/study/KV-Store/docs/thread_pool_findings.md)
4. 只有在需要完整问题链路时，再回到 README 的 `15. 开发日志`

## 3. README 开发日志分段索引

### 3.1 `15.0 2026-04-09 当前仓库状态复核`

适合回答：

- 当前仓库状态是什么
- 本轮论文驱动重构到底改到了哪里
- 当前环境有哪些验证卡点

推荐用途：

- 开工前快速复核
- 答辩前确认当前仓库事实

### 3.2 `15.1 2026-04-02 线程池版本复测记录`

适合回答：

- 为什么线程池没有进入主线
- 线程池退化是“略差”还是“明显退化”
- 退化判断是怎么一步一步形成的

推荐用途：

- 问题链路追溯
- 论文附录
- 答辩问答时补充背景

摘要替代文档：

- [thread_pool_findings.md](/home/haoo/code/study/KV-Store/docs/thread_pool_findings.md)

### 3.3 `15.2 2026-04-06 外部压测工具接入：packetsender`

适合回答：

- 为什么 `packetsender` 最终只定位成外部协议验证工具
- 为什么主性能工具仍然保留 `kvstore_bench`

推荐用途：

- 解释工具选型取舍
- 回忆外部工具接入过程

### 3.4 `15.3 2026-04-07 自带 benchmark 增强：补齐协议级验证`

适合回答：

- 为什么 `kvstore_bench` 既能做性能测试，又能做脚本化协议回归
- `scenario=full` 是怎么引入的

推荐用途：

- 论文中的协议验证链路说明
- benchmark 演进说明

### 3.5 `15.4 2026-04-07 packetsender CLI 实测`

适合回答：

- `packetsender` 在当前环境里到底验证了什么
- 为什么它不适合作为主 benchmark 工具

推荐用途：

- 外部工具适用性说明

### 3.6 `15.5 2026-04-07 自带 benchmark 增强：内建多客户端 aggregate QPS`

适合回答：

- 为什么多客户端 aggregate QPS 被收回到 `kvstore_bench` 内部
- 当前多客户端 benchmark 的统计口径是什么

推荐用途：

- 正式 benchmark 方法说明
- 论文实验方法章节

摘要替代文档：

- [benchmark_methodology.md](/home/haoo/code/study/KV-Store/docs/benchmark_methodology.md)

## 4. 引用建议

- 主线实现与事实描述：优先引用 README 前 14 节和 `docs/` 独立文档
- 实验统计口径：优先引用 [benchmark_methodology.md](/home/haoo/code/study/KV-Store/docs/benchmark_methodology.md)
- 线程池反例结论：优先引用 [thread_pool_findings.md](/home/haoo/code/study/KV-Store/docs/thread_pool_findings.md)
- 真实问题链路：最后再引用 README `15. 开发日志`

## 5. 当前定位

- README 开发日志：保留全过程
- `docs/` 摘要文档：服务论文与答辩复用
- 本索引：负责把两者连接起来
