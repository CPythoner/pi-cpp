# pi-cpp 文档导航

本文档目录把内容分成四类：长期架构、工程方法、版本设计和版本验收。新增文档时应优先判断它属于哪一类，避免把可复用知识持续堆进某个版本文档。

## 1. 阅读入口

如果第一次阅读项目，建议顺序：

1. [项目开发计划](dev-plan.md)：了解版本路线、兼容基线和范围边界。
2. [pi 上游架构分析](pi-architecture.md)：了解被重写目标的结构和语义来源。
3. [v0.0.1 设计](design/v0.0.1.md)：了解三层 SDK 边界与核心类型。
4. [v0.0.2 设计](design/v0.0.2.md)：了解 Provider / SSE / differential 基座的版本实现。
5. 再按主题阅读 `architecture/` 与 `engineering/`。

## 2. 文档分类

### `architecture/` — 长期稳定的项目设计

这类文档回答：**pi-cpp 长期应该怎样设计，以及哪些语义不能被后续实现随意改变。**

- [Cancellation 并发与生命周期](architecture/cancellation.md)
- [EventStream 事件流语义](architecture/event-stream.md)
- [Provider Runtime 分层](architecture/provider-runtime.md)

后续 Agent runLoop、tool execution、Session、TUI/RPC 如果形成跨版本稳定约束，也应在这里沉淀，而不是只写进某个版本设计文档。

### `engineering/` — 可复用的工程方法

这类文档回答：**遇到同类工程问题时应该怎么验证、怎么避免踩坑。**

- [Differential Compatibility Testing](engineering/differential-compatibility-testing.md)

后续适合继续沉淀：

- streaming protocol parsing；
- retry / error semantics；
- CMake SDK boundaries；
- install/export/find_package consumer validation。

### `design/` — 版本设计与实现决策

这类文档回答：**某个版本为什么这样做、做了哪些取舍。**

- [v0.0.1](design/v0.0.1.md)
- [v0.0.2](design/v0.0.2.md)

设计文档可以引用 `architecture/` 和 `engineering/`，但不应复制整篇长期知识。

### `closeout/` — 版本最终验收证据

这类文档回答：**某个版本最终交付了什么、通过了哪些 gate、哪些内容明确不属于本版本。**

- [v0.0.1 Final Closeout](closeout/v0.0.1.md)
- [v0.0.2 Final Closeout](closeout/v0.0.2.md)

## 3. 兼容性文档原则

项目当前行为语义基线固定为：

```text
pi v0.80.0
commit f08e968c83d92bce5f5fd2f7f20ef37f8cf04a39
```

Tau `v0.4.1` 只作为实现参考。发生冲突时：

> pi v0.80.0 可观察行为 > pi-cpp 明确兼容规范 > Tau v0.4.1 实现方式

版本设计文档负责记录“本版证明到了哪里”；工程方法文档负责说明“如何建立可重复的证明”。

## 4. 文档维护规则

- public API、并发语义、事件语义、生命周期等跨版本约束优先写入 `architecture/`；
- 测试方法、协议调试、兼容性验证、构建边界等可复用经验优先写入 `engineering/`；
- 版本状态、范围、实现顺序放在 `design/`；
- 最终 SHA、CI、Compatibility、已知限制放在 `closeout/`；
- 不把“当前实现碰巧如此”直接写成长期架构约束；
- 不把未经 real differential 或 focused test 证明的兼容结论写成已完成事实；
- 文档中的源码路径、类型名和测试名在 closeout 前应做一次机械审计。
