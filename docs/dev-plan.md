# pi-cpp 开发计划（版本化路线图）

> **For agentic workers:** 本文档是主路线图。每个版本启动时，先用 writing-plans 流程把该版本拆解为任务级实施计划（TDD、bite-sized tasks），再进入实现。各版本内的功能清单用 checkbox 跟踪。

**目标：** 用 C++17 实现 pi agent（github.com/earendil-works/pi，TS monorepo）的核心功能——一个可交互的终端 coding agent CLI。行为语义基线固定为 pi `v0.80.0`；Tau `v0.4.1` 仅作为实现参考。项目目标不是一次性复制 pi `v0.80.0` 的全部 Provider/认证/扩展能力，而是对计划内实现的核心 Agent / Coding Agent 能力做到可验证的行为兼容。

**架构：** 三层移植，以 pi `v0.80.0` 定义行为语义，以 HuggingFace Tau `v0.4.1`（pi 架构的 Python 参考实现）辅助理解模块划分和实现方式：`agent`（≈tau_agent，消息/事件/主循环）→ `ai`（≈tau_ai，Provider/SSE）→ `coding`（≈tau_coding，工具/会话/配置）+ `cli`/`tui` 前端。单 executable artifact、错误即消息、append-only JSONL 树状会话。

**技术栈：** C++17 · CMake ≥3.25 + FetchContent · cpr(SSE) · nlohmann/json · reproc++ · fmt · doctest · FTXUI(后期) · cxxopts

---

## 1. 已确认的决策

| 决策项 | 选择 |
|---|---|
| 行为语义基线 | **pi `v0.80.0`**；计划内已实现能力的 API、CLI、事件、会话、工具调用及 wire 语义，以该 tag 的可观察行为为准 |
| 兼容目标 | **核心能力子集兼容**，不是 pi `v0.80.0` 全功能克隆；每项能力用 Compatibility Matrix 标注 Compatible / Partial / Planned / Out of Scope |
| Tau 参考版本 | **Tau `v0.4.1`**；仅参考模块拆分、算法和工程实现，不作为行为规范来源 |
| 语言标准 | **C++17**（编译器基线 GCC 9+ / Clang 7+ / VS2019+；取消链用自研 CancellationToken，接口形态尽量贴近 stop token，但不宣称与 `std::stop_token` 完全等价；格式化统一用 fmt） |
| UI 形态 | v0.1.0 print 模式 → v0.2.0–v0.4.0 行式 REPL + ANSI 流式输出 → v0.5.0 起迭代 FTXUI TUI |
| Provider 策略 | v0.0.2 先打通 OpenAI Chat Completions 兼容协议；v0.4.0 加 Anthropic Messages 原生协议；其余协议按需后续扩展 |
| 平台 | Linux / macOS / Windows 三平台第一天支持（CI matrix） |
| 依赖策略 | 轻依赖：cpr、nlohmann/json、reproc、fmt、cxxopts、doctest、FTXUI，全部 FetchContent + pin tag/commit |
| 分发形态 | 三平台各一个 `picpp` executable artifact；优先降低运行时依赖，不把“完全静态链接”作为跨平台硬约束 |
| 二进制名 | `picpp`；配置目录 `~/.picpp/`；C++ 命名空间 `pi` |

### 1.1 兼容基线与冲突裁决

pi `v0.80.0` 与 Tau `v0.4.1` 并非同期基线。本项目采用“**pi 定义行为，Tau 参考实现**”的双参考策略：

1. 对**本路线图明确实现的能力**，API、CLI、事件、会话、工具调用和 wire 格式以 pi `v0.80.0` 为规范来源。
2. C++ 类型设计、模块组织、并发模型、存储算法等可以参考 Tau `v0.4.1` 的实现方式。
3. 两者出现差异时，按 **pi `v0.80.0` 可观察行为 > 本项目兼容规范 > Tau `v0.4.1` 实现** 的顺序裁决。
4. Tau `v0.4.1` 中来自更高版本 pi 或其自行扩展的能力，只能进入扩展清单或后续版本，不能作为当前兼容基线的必需能力。
5. 兼容测试必须以 pi `v0.80.0` 的真实样本或差分行为为主；Tau 样本仅用于辅助互操作验证。
6. pi `v0.80.0` 中存在、但本路线图明确标记为 Planned / Out of Scope 的能力，不计为当前版本兼容缺陷。

特别说明：`packages/orchestrator` 在 pi `v0.80.0` 中尚不存在，后来才作为实验包加入，并在 pi `v0.81.0` 改名为 `packages/server`。即使 Tau `v0.4.1` 含有类似能力，也不属于当前基线的缺失项，应作为后续扩展单独规划。

### 1.2 Compatibility Matrix（持续维护）

状态含义：

- **Compatible**：已实现，并有 pi `v0.80.0` 真实样本或差分测试证明计划内语义兼容。
- **Partial**：已实现主要路径，但存在文档化偏差或未覆盖子能力。
- **Planned**：属于项目目标，但尚未到对应版本。
- **Out of Scope**：v1.0.0 前明确不做，不构成兼容缺陷。

| 能力面 | v1.0.0 目标状态 | 说明 |
|---|---|---|
| AgentMessage / AgentEvent wire | Compatible | 以 pi `v0.80.0` 真实 fixture + differential trace 为兼容门 |
| Agent runLoop | Compatible | steering / follow-up / tool batch / abort / error semantics 对齐 |
| 默认 coding tools：read/write/edit/bash | Compatible | 参数、结果、截断与关键错误语义对齐 |
| grep/find/ls | Compatible | 后置版本补齐 |
| Session JSONL / tree / compaction | Compatible | 先稳定完整 entry 模型，再分版本开放行为 |
| print / interactive / RPC | Compatible 或文档化 Partial | 仅对计划内命令与事件集承诺兼容 |
| Extensions | Partial | C++ 使用进程外协议替代 TS 动态加载；不追求实现机制相同 |
| Provider: OpenAI Chat Completions compatible | Compatible | 首个主协议 |
| Provider: Anthropic Messages | Compatible | v0.4.0 |
| 其他 pi 内置 Provider 协议 | Out of Scope | openai-responses / bedrock / vertex / mistral 等按需后续加入 |
| OAuth / credential store | Out of Scope | v1.0.0 前只支持 API key / 配置 |
| MCP | Out of Scope | pi `v0.80.0` 核心并非内置 MCP；后续可经扩展实现 |

该表不是静态声明：每个版本 closeout 必须同步更新状态和已知偏差。

---

## 2. 参考蓝本要点（两仓库分析结论）

**pi `v0.80.0`**（TS）：核心包包括 ai/agent/coding-agent/tui。核心是 `packages/agent/src/agent-loop.ts` 的 runLoop 双层循环（外层 follow-up 队列、内层工具迭代）+ AgentEvent 事件流 + JSONL 树状会话（id/parentId）。核心无内置 MCP；审批靠 `beforeToolCall` 钩子，集成主要靠扩展系统。默认 coding tools 是 read/write/edit/bash，同时代码库已有 grep/find/ls 等工具。

**Tau `v0.4.1`**（Python 参考实现）：模块组织与 pi 高度对应，手写 SSE 与较紧凑的实现便于参考 C++ 设计。其施工日志可用于参考增量实施顺序；但它与 pi `v0.80.0` 并非同期基线，任何 wire 或行为兼容结论都必须回到 pi `v0.80.0` 验证。

**关键移植原则（行为由 pi `v0.80.0` 确认，Tau `v0.4.1` 辅助实现）：**

1. 错误不让主循环因普通 Provider/Tool 错误崩溃，按 pi 可观察语义编码为 `stopReason: "error"` 等消息/结果。
2. 消息双层模型：会话里存 AgentMessage，仅在调 LLM 边界 `convertToLlm` 降维。
3. 会话是 append-only JSONL **树**（id/parentId），fork/branch/压缩都是追加 entry 而非改写历史。
4. 工具默认可并行执行；需要顺序语义的工具/操作必须显式声明或经 mutation queue 串行化。
5. steering / follow-up 是**两条可积压消息队列**，不是单槽；支持 one-at-a-time / all 的 drain 语义，排空点按 pi runLoop 行为实现。
6. 兼容性优先用**同输入双实现差分**证明，而不是只靠源码阅读推断。

---

## 3. 总体架构

### 3.1 分层与目录

```text
pi-cpp/
├── CMakeLists.txt
├── cmake/deps.cmake
├── src/
│   ├── agent/
│   │   ├── message.hpp/.cpp
│   │   ├── events.hpp/.cpp
│   │   ├── agent_loop.cpp
│   │   ├── agent.hpp/.cpp
│   │   ├── tool.hpp
│   │   ├── tool_history.cpp
│   │   └── event_stream.hpp
│   ├── ai/
│   │   ├── provider.hpp
│   │   ├── openai_compatible.cpp
│   │   ├── anthropic.cpp
│   │   ├── stream_canon.cpp
│   │   ├── retry.cpp
│   │   ├── http.cpp
│   │   └── fake.cpp
│   ├── coding/
│   │   ├── tools/
│   │   ├── session/
│   │   │   ├── entry.hpp/.cpp       # v0.1.0 起稳定完整 SessionHeader/SessionEntry wire 模型
│   │   │   ├── storage.cpp
│   │   │   ├── tree.cpp
│   │   │   └── manager.cpp
│   │   ├── compaction.cpp
│   │   ├── system_prompt.cpp
│   │   ├── config.cpp
│   │   └── catalog.cpp
│   ├── cli/
│   │   ├── main.cpp
│   │   ├── print_mode.cpp
│   │   ├── repl.cpp
│   │   └── rpc_mode.cpp
│   ├── tui/
│   └── util/
├── tests/
│   ├── fixtures/pi-v0.80.0/          # 来自真实 pi 基线的输入/输出/会话样本
│   └── differential/                 # pi reference harness 与 picpp 的规范化 trace 对比
├── examples/
├── docs/
└── .github/workflows/ci.yml
```

### 3.2 并发模型（对应 pi 的单线程事件循环语义）

- **主线程**：REPL/TUI 事件循环，从 `EventStream` 队列取事件渲染。
- **网络线程**：每个活跃 LLM 流一个 worker `std::thread`，cpr 同步请求 + callback 将事件推入队列。
- **工具执行**：默认并行；工具可声明 sequential；edit/write 至少经 file-mutation-queue 保证相互冲突的写操作串行。
- **取消**：CancellationToken 贯穿 Agent → Provider → HTTP / Tool → reproc；取消回调不得在持有 token 内部 mutex 时执行用户代码。
- **消息队列**：steeringQueue / followUpQueue 各自持有 `deque<AgentMessage>`；默认 `one-at-a-time`，同时支持 `all` 模式；不得用 `optional<AgentMessage>` 单槽替代。

### 3.3 CancellationToken 约束

v0.0.1 已交付自研 CancellationToken；在进入真实 HTTP 前做一次语义加固：

1. `request()` 锁内只做 cancelled 状态转换与 callback snapshot/detach。
2. 用户 callback 在解锁后执行，避免 callback 重入 token 或做网络/进程取消时死锁。
3. unregister 与 callback 生命周期需有明确同步规则和竞态测试。
4. 文档只声明“接口目标接近 stop-token 取消模型”，除非逐项验证，否则不再写“与 `std::stop_token` 完全等价”。

### 3.4 技术选型

| 领域 | 选型 | 关键注意点 |
|---|---|---|
| HTTP+SSE | cpr/curl | 必须以所 pin 版本的真实 API 做 spike；流式请求避免整体 wall-clock timeout，保留 connect/low-speed/取消控制 |
| JSON | nlohmann/json | wire 类型需显式保证字段省略/null/未知字段策略与 pi fixture 一致 |
| 子进程 | reproc++ | Unix 进程组与 Windows Job Object 均需真实进程树测试 |
| TUI | FTXUI ≥7.x | v0.5.0 前 CJK/IME/跨线程 Post spike；失败则退回增强 REPL，TUI 技术栈重评 |
| 格式化 | fmt | C++17 无 std::format |
| 测试 | doctest | 单元与确定性集成测试 |
| CLI | cxxopts | 参数解析；wire/模式行为由业务层控制 |
| 构建 | CMake ≥3.25 | FetchContent Declare 前置、精确 pin、第三方 SYSTEM；发布目标是单 executable artifact，不强制所有平台完全静态 |
| CI | GitHub Actions matrix | ubuntu / macOS / Windows；PR CI 不依赖真实外部 API secrets |

---

## 4. 里程碑总览

| 版本 | 主题 | 交付标志 | 规模 |
|---|---|---|---|
| v0.0.1 | 骨架与核心类型 | 三平台 CI；基础 wire 类型与单测 | S |
| v0.0.2 | Provider 层与 SSE | OpenAI 兼容真实流 + differential test 基座 | M |
| v0.0.3 | Agent 主循环 | 多轮 tool loop + 多消息 steering/follow-up + abort | M |
| v0.0.4 | 工具四件套 | read/write/edit/bash 全部可用 | M |
| v0.1.0 | **print 模式 MVP** | `picpp -p` 端到端 + 完整 SessionEntry wire 模型落盘 | M |
| v0.2.0 | REPL + 会话恢复 | 三平台交互 + resume + session tree 基础 | M |
| v0.3.0 | 会话树与压缩 | fork/branch/tree + compaction | M |
| v0.4.0 | 多 Provider | Anthropic + pi-derived 模型元数据 + runtime switch | M |
| v0.5.0 | TUI | FTXUI 全屏交互；CJK/IME 通过 go/no-go 门 | L |
| v0.6.0 | RPC + 只读工具 | pi 计划内 JSONL RPC + grep/find/ls | M |
| v0.7.0 | 扩展协议 v1 | 进程外 extension tool/command/hook + 示例 | L |
| v0.8.0 | Skills / Prompts / Resilience | 用户扩展内容 + crash-safe session + 大会话加载 | M |
| v0.9.0 | Release Hardening | 性能、E2E matrix、打包、兼容矩阵收敛 | M |
| v1.0.0 | Compatibility Freeze | 不加新功能；冻结计划内兼容契约并正式发布 | S |

每个版本都是可发布状态：构建绿、测试绿、上一个版本功能不回退。

---

## 5. 各版本详细计划

### v0.0.1 — 骨架与核心类型（已完成）

**目标：** 项目能三平台编译、CI 就位；建立消息/事件类型系统和 C++17 基础设施。

本版本已按 `docs/design/v0.0.1.md` 完成并发布；历史实现细节与偏差以该设计文档的实现回顾和 CHANGELOG 为准。本路线图不追溯修改已完成版本的验收事实。

**已确认遗留：**

- 当前 fixtures 含手工构造样本，不能单独作为“pi `v0.80.0` 真实兼容”的证明。
- CancellationToken 在进入真实 HTTP 前需完成 callback 锁外执行的语义加固。
- EventStream 在 v0.0.2 实现。

---

### v0.0.2 — Provider 层与 SSE

**目标：** 打通 OpenAI Chat Completions 兼容协议的真实流式对话，并建立后续所有版本共用的 pi `v0.80.0` 差分测试基座。

**前置兼容任务（T0）：**

- [ ] 从 pi `v0.80.0` 固定 tag 生成/收集真实 message/event/session fixtures，存入 `tests/fixtures/pi-v0.80.0/`，记录来源脚本与基线 commit。
- [ ] 增加最小 TS reference harness：给定 FakeProvider/固定输入，输出规范化 `trace.jsonl`。
- [ ] C++ 侧对相同输入输出规范化 trace；比较事件顺序、消息字段、stopReason、toolCall 聚合等稳定字段；UUID/时间戳/耗时等非确定字段只做规范化后比较。
- [ ] 把“真实 pi fixture + differential trace”写入兼容门；Tau fixture 只做辅助回归。

**功能清单：**

- [ ] 加固 `CancellationToken`：callback snapshot 后锁外执行；补 callback 重入、unregister/request 并发、CombinedCancellation 析构竞态测试。
- [ ] `ai/provider.hpp`：`streamResponse(model, system, messages, tools, stopToken, sessionId)` 单接口，产出 L1 事件流。
- [ ] `agent/event_stream.hpp`：线程安全事件队列（push/close/wait/result semantics），网络线程生产、消费侧拉取。
- [ ] `ai/http.cpp`：按实际 pin 的 cpr API 做流式封装；Bearer 鉴权；connect/low-speed/取消语义明确。
- [ ] `ai/openai_compatible.cpp`：请求构造、SSE 事件解析、delta 合并、tool_calls index 聚合、finish_reason → StopReason、usage 捕获。
- [ ] `ai/stream_canon.cpp`：文本/thinking/tool-call 通道 Start/Delta/End 配对归一化。
- [ ] `ai/retry.cpp`：指数退避 + jitter + Retry-After/可重试错误判定；普通网络/Provider 错误转 AssistantMessage error，而非让 agent 进程退出。
- [ ] `ai/fake.cpp`：FakeProvider 事件脚本回放。
- [ ] `examples/chat_demo.cpp`：最小真实端点流式示例。

**验收标准：**

1. DeepSeek 或 GLM/OpenAI-compatible endpoint 能逐增量输出完整回答。
2. 断网/401/429/5xx 的错误路径行为有测试；预期可恢复错误不导致进程崩溃。
3. SSE 测试覆盖跨 chunk、多个 `data:` 行、注释/空事件、`[DONE]`、畸形 JSON 与取消。
4. 至少一组 pi `v0.80.0` reference trace 与 picpp normalized trace 一致。
5. CancellationToken callback 可安全重入只读/查询接口，不因内部锁死锁。

**参考蓝本：** pi `v0.80.0` 的 `packages/ai/src/api/openai-completions.ts`、agent EventStream/stream 相关实现；Tau `v0.4.1` 的 openai compatible / stream / retry / fake 实现只作结构参考。

---

### v0.0.3 — Agent 主循环

**目标：** 移植系统心脏 runLoop。此版本真实 coding tools 尚未实现，用测试桩工具验证循环语义。

**功能清单：**

- [ ] `agent/tool.hpp`：AgentTool（name/description/schema/execute/executionMode）+ AgentToolResult。
- [ ] `agent/agent_loop.cpp`：双层 while——外层 follow-up 队列检查、内层 steering/tool 迭代；顺序以 pi `v0.80.0` differential trace 为门。
- [ ] `agent/agent.hpp/.cpp`：Agent 门面（prompt/steer/followUp/abort/subscribe/waitForIdle）。
- [ ] `PendingMessageQueue`：内部为 `deque<AgentMessage>`，支持 `QueueMode::OneAtATime` / `QueueMode::All`；steering/follow-up 各自独立；连续 enqueue 多条不得覆盖。
- [ ] steering / follow-up drain timing 与 pi 对齐：steering 在相应 turn 间隙注入；agent 本可停止时才检查 follow-up。
- [ ] tool execution 支持默认 parallel 和 per-tool/global sequential 语义；结果消息最终顺序可确定。
- [ ] 取消链从 Agent.abort() 贯穿 provider 与 tool；aborted 收尾不丢已产出内容。
- [ ] timing：TTFT/total duration 等本项目扩展字段不得改变 pi 核心行为。
- [ ] `agent/tool_history.cpp`：repair_tool_history。
- [ ] provider error → agent 结束当前 run → 下轮可继续。

**验收标准：** FakeProvider + differential harness 覆盖：

1. 多轮工具调用收敛。
2. `steer(A), steer(B), steer(C)` 不丢消息；one-at-a-time/all 两种模式与 pi 语义一致。
3. 多条 follow-up 按配置 drain。
4. parallel / sequential tool batch 行为正确。
5. 任意时刻 abort 无死锁、无后台线程泄漏。
6. error 后再次 prompt 可恢复。

---

### v0.0.4 — 工具四件套

**目标：** 实现 read/write/edit/bash，达到“能真正改代码”的能力。

**功能清单：**

- [ ] `coding/tools/read.cpp`：行范围、行号、截断与二进制策略。
- [ ] `coding/tools/write.cpp`：整文件写入、父目录创建。
- [ ] `coding/tools/edit.cpp`：精确字符串替换、唯一匹配校验、行尾风格保持、失败上下文、diff 展示。
- [ ] `coding/tools/bash.cpp`：reproc++；Unix shell / Windows cmd；超时；CancellationToken；POSIX 进程组 + Windows Job Object；stdout/stderr 截断；退出码。
- [ ] file mutation queue：edit/write 的冲突写操作串行化。
- [ ] `beforeToolCall` / `afterToolCall` 钩子，按 pi 行为返回 block/reason/result mutation/terminate。
- [ ] 参数 schema 校验：错误作为 ToolResult 返回，不让 agent crash。

**验收标准：** 工具单测 + FakeProvider “read→edit→bash verify” 完整链路；bash timeout/abort 后无残留进程；计划内工具的关键输入/输出与 pi fixture/differential trace 对齐。

---

### v0.1.0 — print 模式 MVP

**目标：** 第一个可真实使用的端到端版本，同时一次性稳定 SessionHeader / SessionEntry 的完整 wire 类型模型，避免 v0.2–v0.8 反复迁移磁盘格式。

**功能清单：**

- [ ] `coding/system_prompt.cpp`：工具清单、guidelines、AGENTS.md 发现注入、cwd/date；可观察内容按 pi `v0.80.0` 对照。
- [ ] `coding/config.cpp`：settings / custom OpenAI-compatible models / environment API key。
- [ ] `coding/session/entry.hpp/.cpp`：定义 pi `v0.80.0` 计划内完整 entry union：
  - SessionHeader（version/id/timestamp/cwd/parentSession 等）
  - message
  - thinking_level_change
  - model_change
  - compaction
  - branch_summary
  - custom
  - custom_message
  - label
  - session_info
- [ ] 对尚未开放行为的 entry，至少做到 parse/preserve/dump；不要因为当前版本暂时不用就省略 wire 类型。
- [ ] `coding/session/storage.cpp`：append-only JSONL + crash-aware flush 基础；`--session-dir`。
- [ ] `cli/main.cpp` + `cli/print_mode.cpp`：`picpp -p`；`--model`；JSON event mode；退出码语义。
- [ ] README 快速开始。

**验收标准：** 临时项目端到端改文件+运行测试成功；真实 pi session fixtures 能读写关键 entry；未知字段保留/忽略策略有明确兼容规则；旧 entry 不被重写。

---

### v0.2.0 — REPL 交互 + 会话恢复

**目标：** 日常可用的交互式行模式；会话可恢复；SessionEntry 类型不再扩 schema，只增加行为。

**功能清单：**

- [ ] 行式输入 + ANSI 流式输出 + 工具调用块。
- [ ] spinner；Ctrl+C 中止当前轮；明确单击/双击语义。
- [ ] `/help` `/model` `/clear` `/new` `/save` `/exit`。
- [ ] Windows VT + UTF-8；非 TTY 自动降级。
- [ ] `coding/session/tree.cpp`：id/parentId 建树与 path query。
- [ ] `--continue` / `--resume <id>`：从 SessionEntry 重建上下文、thinking/model 状态。
- [ ] REPL 只消费 AgentEvent，不复制一套 agent 状态机。

**验收标准：** macOS/Linux/Windows Terminal 人工走查；中文输入输出；interrupt→resume 完整；Ctrl+C 不死锁不脏屏。

---

### v0.3.0 — 会话树与上下文压缩

**目标：** 长会话生存能力。

**功能清单：**

- [ ] `/tree` + `/fork <entryId>`；分叉只追加 entry。
- [ ] branch summary 使用已稳定的 BranchSummaryEntry wire 类型。
- [ ] compaction：token 估算、手动/自动触发、CompactionEntry、LLM context 裁剪；原历史不改写。
- [ ] `/cost` `/context`。
- [ ] compaction 阈值使用模型元数据，但策略不能绑定 Tau catalog 格式。

**验收标准：** 超长对话继续工作；分支独立；旧 JSONL 字节不变；pi session fixture 的 compaction/branch 关键字段可兼容读取。

---

### v0.4.0 — 多 Provider（Anthropic + 模型目录）

**目标：** 补齐第二条原生协议并数据驱动模型元数据。

**功能清单：**

- [ ] `ai/anthropic.cpp`：Anthropic Messages SSE、thinking/signature、stop reason、usage。
- [ ] thinking level 内部归一化，但 wire/Provider 请求以各协议真实语义为准。
- [ ] `coding/catalog.cpp`：**元数据以 pi `v0.80.0` 为第一来源**；可以借鉴 Tau `catalog.toml` 的文件组织方式，但不能直接把 Tau 数据当兼容规范。
- [ ] 用户覆盖层：`~/.picpp/catalog.toml` 或等价稳定配置格式。
- [ ] `/model` runtime switch；写 model_change entry。
- [ ] compat flags 数据驱动化。

**验收标准：** Anthropic 原生 + 至少两个 OpenAI-compatible endpoint；同 session 切模型；加普通兼容 Provider 主要通过配置完成；context/cost 元数据有来源说明。

---

### v0.5.0 — TUI（FTXUI）

**目标：** 从行模式升级为全屏组件化交互；UI 不改变 agent/session 核心语义。

**功能清单：**

- [ ] 前置 go/no-go spike：Windows Terminal/conhost/WezTerm 的 CJK 全宽、IME、粘贴、跨线程事件投递。
- [ ] 若 spike 不达标，版本降级为增强 REPL/raw-mode line editor，FTXUI 不强行上线。
- [ ] 消息流区、工具折叠块、Markdown 基础渲染。
- [ ] 多行输入、历史、粘贴。
- [ ] 状态栏：model/context/cost/branch。
- [ ] beforeToolCall approval UI。
- [ ] 主题基础。
- [ ] `--no-tui` 保留。

**验收标准：** CJK/IME 正确；长会话不卡顿；所有核心行为可经非 TUI 模式回归验证。

---

### v0.6.0 — RPC 模式 + 只读工具

**目标：** 先稳定 headless 集成协议，不把扩展进程协议同时塞入本版本。

**功能清单：**

- [ ] `picpp --mode rpc`：stdin/stdout JSONL。
- [ ] 计划内命令：prompt/steer/followUp/abort/fork/compact/status/exit 等；实际命令集以 pi `v0.80.0` 为来源并在 Compatibility Matrix 标注覆盖情况。
- [ ] AgentEvent 以通知形式回流。
- [ ] `grep` / `find` / `ls` 工具。
- [ ] ignore/path/truncation 行为尽量对齐 pi，不自创“gitignore 子集”语义后再称 Compatible；若必须偏差则明确标 Partial。

**验收标准：** harness 经 RPC 驱动 prompt→tools→fork→exit；stdout 始终保持协议纯净，日志走 stderr；grep/find/ls 有大目录和取消测试。

---

### v0.7.0 — 扩展协议 v1

**目标：** 在 RPC 稳定后再设计 C++ 的进程外扩展模型；实现方式允许不同于 pi TS extensions，但行为面明确映射。

**功能清单：**

- [ ] `~/.picpp/extensions/*.json` 声明式扩展。
- [ ] 子进程 handshake + protocolVersion + capabilities。
- [ ] register_tool / register_command。
- [ ] beforeToolCall / afterToolCall 等首批 hook。
- [ ] 生命周期：spawn/ready/timeout/cancel/crash/restart/disable。
- [ ] stdout 协议与 stderr 日志隔离。
- [ ] examples：permission-gate、subagent。
- [ ] Extension Compatibility Matrix：逐个标 pi extension event/hook 的 Supported / Planned / Not Applicable。

**验收标准：** extension crash 不拖垮主 agent；permission gate 真正阻断危险调用并返回 reason；subagent 生命周期和取消可控。

---

### v0.8.0 — Skills / Prompts / Resilience

**目标：** 把之前堆在 v1.0 的真实用户能力和存储韧性提前完成。

**功能清单：**

- [ ] Skills discovery / expansion。
- [ ] Prompt templates → slash commands。
- [ ] JSONL 半行/崩溃恢复策略。
- [ ] 可配置 retry policy。
- [ ] 大 session 流式读取/低峰值内存。
- [ ] session format compatibility tests：旧 fixture、损坏尾行、未知 entry/字段。

**验收标准：** 真实大 session 可恢复；崩溃尾行不破坏此前历史；skills/prompts 在 REPL/TUI/RPC 共享同一业务层。

---

### v0.9.0 — Release Hardening

**目标：** 不再扩主功能面，集中处理性能、打包、跨平台、E2E 和兼容缺口。

**功能清单：**

- [ ] Compatibility Matrix 全量审计：所有计划内项必须变成 Compatible 或有明确且接受的 Partial 偏差。
- [ ] 性能基线：冷启动、空闲内存、大 session 加载、长 TUI 滚动；阈值以实际 release build 基准后再锁定，避免路线图提前写不现实数字。
- [ ] GitHub Releases：Windows/macOS/Linux executable artifact + checksums + 安装说明。
- [ ] E2E matrix：平台 × Provider 协议 × print/REPL/TUI/RPC。
- [ ] 文档：架构、配置、session 格式、RPC、extensions、兼容矩阵、FAQ。
- [ ] release candidate 阶段只接收 bugfix / compatibility fix / docs。

---

### v1.0.0 — Compatibility Freeze

**目标：** 正式冻结 v1 的计划内用户契约。**本版本不新增功能。**

**准入条件：**

- [ ] v0.9.0 Release Hardening 全部完成。
- [ ] 计划内 Compatibility Matrix 无未解释的 Planned 项。
- [ ] 三平台 release artifact 从干净机器可运行。
- [ ] 全部 deterministic compatibility/differential tests 通过。
- [ ] E2E smoke matrix 通过。
- [ ] CHANGELOG / README / migration / known deviations 完整。

**验收标准：** tag `v1.0.0` 只包含 release/bugfix/docs 类改动；用户可根据 Compatibility Matrix 准确知道与 pi `v0.80.0` 哪些地方兼容、哪些地方明确不同。

---

## 6. 测试策略

1. **单元测试（doctest）**：序列化、SSE parser、delta merger、diff/truncate、session tree、cancel races 等纯逻辑。
2. **pi 真实黄金样本**：固定 `v0.80.0` tag 生成/采集，保留来源；会话/消息/wire fixture 是兼容门。
3. **Differential tests**：同一确定性输入分别跑 pi reference harness 与 picpp，规范化后比较 event trace / message / tool ordering / stop semantics。它是 Agent/Session/RPC 兼容的最高优先级测试。
4. **FakeProvider 集成测试**：确定性驱动 runLoop/tool/cancel/steering/follow-up。
5. **Tau fixtures**：只做辅助互操作/实现回归，不得单独证明 pi compatibility。
6. **真实端点冒烟**：本地或可选 scheduled workflow；不在普通 PR CI 强制依赖 secrets。
7. **跨平台回归门**：每个版本 Linux/macOS/Windows CI 全绿；涉及终端/进程树的版本另加真实人工/VM 走查。
8. **版本 closeout**：更新 Compatibility Matrix、记录设计偏差和新增 known deviations。

### 6.1 差分测试规范化原则

允许规范化的典型非确定字段：UUID、绝对时间戳、真实 duration、临时目录绝对路径、平台特有换行（仅在 pi 本身不承诺差异时）。

不得通过规范化隐藏：事件顺序、message role/type、tool call/result pairing、stopReason、queue drain 顺序、session parentId 关系、RPC command semantics、用户可见错误分类。

---

## 7. 非目标（v1.0.0 前明确不做）

- MCP 内置实现；需要时优先考虑 extension 适配。
- OAuth 完整流程与 credential 加密存储。
- 内置容器/VM 沙箱。
- 终端图片、HTML 导出、遥测、自更新。
- C++20 coroutine/modules。
- pi `v0.80.0` 中除 OpenAI Chat Completions compatible 与 Anthropic Messages 外的其他 Provider 原生协议，例如 openai-responses / bedrock / vertex / mistral 等。
- 与 pi TS 扩展系统“实现机制完全相同”；本项目只对文档中承诺的扩展行为面负责。

---

## 8. 风险与对策

| 风险 | 等级 | 对策 |
|---|---|---|
| “兼容 pi”范围失控 | 高 | Compatibility Matrix + Planned/Out-of-Scope 明示；只对已承诺子集做兼容声明 |
| 仅靠源码阅读产生错误兼容结论 | 高 | v0.0.2 起建立固定 pi reference harness + differential test |
| Session wire 随版本反复迁移 | 高 | v0.1.0 一次性定义完整计划内 SessionEntry union；后续只增加行为 |
| steering/follow-up 丢消息 | 高 | deque + QueueMode；多消息差分测试；禁止单槽实现 |
| Windows 终端/编码/进程模型 | 高 | 从第一天 CI；Job Object；v0.5.0 CJK/IME spike；真实 Windows 走查 |
| CancellationToken callback 死锁/UAF | 高 | v0.0.2 前锁外执行 callback；unregister/request/destructor race 单测 |
| Provider 兼容服务器碎片化 | 中 | compat flags 数据驱动；只对实测/声明范围承诺 Compatible |
| FTXUI CJK/IME 不达标 | 中 | go/no-go spike；增强 REPL 作为稳定退路 |
| 扩展协议范围膨胀 | 中 | RPC 与 extensions 分版本；v0.7.0 先冻结 protocol v1 最小事件面 |
| variant 深嵌套可读性 | 中 | visit helper + domain facade；禁止业务代码散落裸 `std::get` |
| 单人项目周期过长 | 中 | 每版本独立可用；v0.1.0 后任意 tag 都保持可运行；v1.0 不再塞新功能 |

---

## 9. 开发方法论（学习驱动 · 文章级设计文档 · 版本 tag）

本仓库是**学习型项目**：一边学习 pi 架构一边用 C++17 实现。每个版本固定节奏，产出设计文章、代码、兼容证据和 tag。

### 9.1 每版本标准工作流

1. **基线取证**：先确定该版本对应的 pi `v0.80.0` 源码、fixture 和可观察行为；不得先看 Tau 就直接定兼容语义。
2. **设计先行**：完成 `docs/design/vX.Y.Z.md`，写清原理、候选方案、pi 行为、C++ 设计与已知偏差。
3. **任务拆解**：TDD bite-sized tasks；兼容任务优先写 reference fixture / differential red test。
4. **实现**：小步提交；feat/fix/test/docs；关键路径引用 pi tag 文件路径/行为来源。
5. **验收**：单元 + FakeProvider + differential + 三平台 CI；涉及真实终端/进程/Provider 的版本再做专项走查。
6. **回顾**：设计文档末尾记录实际偏差、踩坑、遗留问题和 Compatibility Matrix 变化。
7. **收尾打 tag**：更新 `CHANGELOG.md`、README 路线图与 Compatibility Matrix，打 annotated tag。

### 9.2 设计文档 = 可发表文章

- **自包含**：读者无上下文也能理解项目、本版本位置与问题背景。
- **先行为后实现**：先写 pi `v0.80.0` 可观察行为，再写 Tau 参考和 C++ 实现。
- **讲原理**：SSE 分帧、JSONL 树、进程组、取消并发等必须讲机制，不只列 API。
- **展示真实代码/wire**：关键 C++ interface、JSON、trace 示例进正文。
- **决策透明**：候选方案、取舍、兼容影响。
- **正文/附录分离**：checklist、任务拆解、差分样本表、蓝本映射放附录。

### 9.3 学习导向原则

- 不熟悉的机制先做 spike/最小实验，再写设计，不凭印象定 API。
- “Tau 更容易抄”不能成为偏离 pi 行为的理由。
- 每版本沉淀 C++17 技术点、pi 架构原理、差分测试发现的语义细节。

### 9.4 版本纪律

- 主干始终可构建；CI 红为最高优先级。
- 每个版本都是可发布状态：构建绿、测试绿、上版功能不回退。
- 已发布版本的历史事实不在路线图中反向改写；发现问题记录为后续修复/known deviation。
- **v0.9.0 起冻结功能面；v1.0.0 不新增功能，只允许兼容修复、bugfix、release 与文档改动。**
