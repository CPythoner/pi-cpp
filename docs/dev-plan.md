# pi-cpp 开发计划（版本化路线图）

> **For agentic workers:** 本文档是主路线图。每个版本启动时，先完成对应 `docs/design/vX.Y.Z.md`，再按 TDD / bite-sized tasks 实现。每个 tag 都必须处于可构建、可测试、可回归状态。

**目标：** 用 C++17 实现 pi `v0.80.0` 的核心 Agent / Coding Agent 能力，并把 `ai`、`agent`、`coding-agent` 三层建设成可供外部 C++ 项目独立调用的 SDK。`picpp` CLI/TUI 只是官方前端，不拥有核心业务状态机。

**行为语义基线：** pi `v0.80.0`。  
**实现参考：** Tau `v0.4.1`，仅参考模块拆分、算法和工程实现，不作为行为规范来源。

项目不追求在 v1.0 前复制 pi 的全部 Provider、认证和外围集成，而是对路线图明确承诺的核心子集做到可验证的行为兼容。

---

## 1. 已确认的架构决策

| 决策项 | 选择 |
|---|---|
| 语言 | C++17；GCC 9+ / Clang 7+ / VS2019+ |
| 行为规范 | **pi `v0.80.0`** |
| Tau | **`v0.4.1` 只作实现参考** |
| SDK 分层 | `ai` → `agent` → `coding-agent` |
| Public headers | `include/pi/ai`、`include/pi/agent`、`include/pi/coding-agent` |
| Implementation | `src/ai`、`src/agent`、`src/coding-agent`；`src/` 不作为 SDK PUBLIC include path |
| CMake targets | `pi::ai` → `pi::agent` → `pi::coding-agent` |
| C++ namespaces | `pi::ai`、`pi::agent`、`pi::coding_agent` |
| 官方前端 | `apps/picpp`；PRIVATE link `pi::coding-agent` |
| Provider 首发 | OpenAI Chat Completions compatible |
| 第二协议 | Anthropic Messages |
| Session | append-only JSONL tree，id/parentId |
| 错误模型 | 普通 Provider/Tool 错误转消息/结果，不让 Agent 因普通错误退出进程 |
| 平台 | Linux / macOS / Windows 从第一版开始支持 |
| 分发 | 三平台 executable artifact + 可安装的 CMake SDK targets；不强制完全静态链接 |

### 1.1 兼容冲突裁决

pi `v0.80.0` 与 Tau `v0.4.1` 并非同期基线，统一按以下顺序裁决：

> **pi `v0.80.0` 可观察行为 > pi-cpp 明确兼容规范 > Tau `v0.4.1` 实现**

具体规则：

1. API、event、message、session、tool、CLI/RPC wire 等计划内行为以 pi `v0.80.0` 为准。
2. C++ 类型、并发、模块组织、存储算法可以参考 Tau。
3. Tau 中来自更高版本 pi 或 Tau 自身的扩展，不自动进入当前目标。
4. 兼容证据优先使用真实 pi fixture 和 differential tests；Tau fixture 只能辅助回归。
5. pi 中被明确标为 Out of Scope 的能力，不构成当前版本兼容缺陷。

`packages/orchestrator` 不属于 pi `v0.80.0` 基线：它后来才加入并在 pi `v0.81.0` 改名为 `packages/server`，因此不进入当前兼容范围。

### 1.2 版本号策略

v1.0 前不让每个技术主题都提升 minor：

- `v0.0.x`：**地基**——SDK 边界、Provider、Agent Loop、基础 tools。
- `v0.1.x`：**能用**——print MVP、REPL、session、第二 Provider。
- `v0.2.x`：**好用**——TUI、RPC、extensions、skills/resilience。
- `v0.3.0`：**核心完成**——计划内 pi Coding Agent 主功能基本完成，进入 API/compatibility freeze candidate。
- `v0.3.x`：只做 hardening / compatibility / performance / packaging / RC。
- `v1.0.0`：冻结计划内行为契约和 public SDK/API；不代表复制 pi 全部功能。

0.x 阶段允许 patch 位承载同一成熟度阶段内的功能里程碑；v1.0 后按稳定 SemVer 管理 public API。

---

## 2. SDK-first 架构

### 2.1 依赖方向

```text
external C++ app / picpp CLI / TUI
                 │
                 ▼
            coding-agent
                 │
                 ▼
               agent
                 │
                 ▼
                ai
```

禁止：

- `ai` include `agent` / `coding-agent`；
- `agent` include `coding-agent`；
- SDK public header include CLI/TUI header；
- public API 暴露 `src/...` private 类型；
- public API 泄漏 cpr/reproc/FTXUI 等实现库类型。

### 2.2 当前与目标目录

从 `v0.0.1` 起即采用：

```text
pi-cpp/
├── include/
│   └── pi/
│       ├── ai/
│       │   ├── message.hpp
│       │   ├── events.hpp
│       │   └── cancellation.hpp
│       ├── agent/
│       │   ├── message.hpp
│       │   └── events.hpp
│       └── coding-agent/
│           └── fwd.hpp
├── src/
│   ├── ai/
│   ├── agent/
│   ├── coding-agent/
│   └── util/                  # private support
├── apps/
│   └── picpp/
├── tests/
│   ├── agent/
│   ├── util/
│   ├── consumer/
│   └── fixtures/
└── docs/
```

后续只在既有目录内扩展，不再做一次“大搬家”。

到 v1.0 前目标 public surface：

```text
include/pi/ai/
  message.hpp
  model.hpp
  provider.hpp
  events.hpp
  event_stream.hpp
  cancellation.hpp

include/pi/agent/
  message.hpp
  events.hpp
  tool.hpp
  agent.hpp
  queue_mode.hpp

include/pi/coding-agent/
  coding_agent.hpp
  tools.hpp
  config.hpp
  model_catalog.hpp
  session.hpp
  session_entry.hpp
  extension.hpp
```

### 2.3 CMake targets

```cmake
pi_ai           -> pi::ai
pi_agent        -> pi::agent
pi_coding_agent -> pi::coding-agent
picpp           -> executable
```

依赖：

```cmake
pi_agent        PUBLIC pi::ai
pi_coding_agent PUBLIC/INTERFACE pi::agent
picpp           PRIVATE pi::coding-agent
```

SDK public include 只允许：

```cmake
$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
$<INSTALL_INTERFACE:include>
```

`src/` 只能 PRIVATE 使用。

### 2.4 SDK 验收门

每一层新增 public API 时必须有 external-consumer 视角：

- public header self-contained；
- `pi::ai` consumer 不依赖 agent/coding-agent；
- `pi::agent` consumer 不依赖 coding-agent；
- `pi::coding-agent` consumer 不需要 CLI/TUI；
- CLI/TUI 只通过 SDK public API 驱动业务。

`v0.1.0` 补齐安装消费：

```cmake
find_package(picpp CONFIG REQUIRED)
target_link_libraries(app PRIVATE pi::coding-agent)
```

---

## 3. Compatibility Matrix

状态：

- **Compatible**：已有真实 pi fixture / differential test 证明计划内语义兼容。
- **Partial**：主要路径已实现，但存在文档化偏差。
- **Planned**：计划内但尚未实现。
- **Out of Scope**：v1.0 前明确不做。

| 能力 | v1.0 目标 | 里程碑 |
|---|---|---|
| ai public SDK | Stable / Compatible | v0.0.1 建边界；v0.3.0 freeze candidate |
| agent public SDK | Stable / Compatible | v0.0.1 建类型边界；v0.0.3 主干完成 |
| coding-agent public SDK | Stable / Compatible | v0.0.1 建 target；v0.0.4 起填充 |
| Message / Event wire | Compatible | v0.0.2 起 differential gate |
| Agent runLoop | Compatible | v0.0.3 |
| read/write/edit/bash | Compatible | v0.0.4 |
| Session JSONL/tree/compaction | Compatible | v0.1.0–v0.1.2 |
| Anthropic Messages | Compatible | v0.1.3 |
| TUI | Compatible 或文档化 Partial | v0.2.0 |
| RPC | Compatible 或文档化 Partial | v0.2.1 |
| grep/find/ls | Compatible | v0.2.1 |
| Extensions | Partial | v0.2.2；C++ 进程外模型 |
| Skills / Prompt templates | Compatible 或 Partial | v0.2.3 |
| 其他 pi Provider 原生协议 | Out of Scope | v1.0 前不做 |
| OAuth / credential store | Out of Scope | v1.0 前不做 |
| MCP 内置 | Out of Scope | 后续可 extension 适配 |

每个版本 closeout 更新矩阵和 known deviations。

---

## 4. 里程碑总览

| 版本 | 成熟度 | 主题 | 交付标志 |
|---|---|---|---|
| `v0.0.1` | 地基 | **骨架 + 核心类型 + SDK 边界** | 三层 target/public include；message/event JSON；三平台骨架 |
| `v0.0.2` | 地基 | **Provider / SSE + Differential 基座** | OpenAI-compatible 真实流；真实 pi fixture/reference harness |
| `v0.0.3` | 地基 | Agent 主循环 | tool loop + steering/followUp + abort |
| `v0.0.4` | 地基 | coding-agent 工具四件套 | read/write/edit/bash |
| `v0.1.0` | **能用** | print MVP + Stable Session Wire | `picpp -p`；完整 SessionEntry；install/export SDK |
| `v0.1.1` | 能用 | REPL + resume | 三平台交互、session resume |
| `v0.1.2` | 能用 | Session tree + compaction | fork/tree/branch/long-context |
| `v0.1.3` | 能用 | Anthropic + model catalog | 第二原生 Provider + runtime model switch |
| `v0.2.0` | **好用** | TUI | FTXUI/full-screen，SDK 不依赖 UI |
| `v0.2.1` | 好用 | RPC + grep/find/ls | headless integration + read-only tools |
| `v0.2.2` | 好用 | Extensions v1 | subprocess tool/command/hook |
| `v0.2.3` | 好用 | Skills / Prompts / Resilience | skills、templates、crash-safe session |
| `v0.3.0` | **核心完成** | Compatibility/API Freeze Candidate | 计划内核心功能基本完成 |
| `v0.3.x` | 稳定化 | Hardening / RC | compatibility、性能、跨平台、打包 |
| `v1.0.0` | 稳定 | Stable Contract | 冻结计划内 SDK/API/behavior contract |

---

## 5. 各版本详细计划

### v0.0.1 — 骨架 + 核心类型 + SDK 边界

**目标：** 在第一版就把后续代码生长的边界固定正确。

已完成/重打 tag 前验收：

- [x] `include/pi/ai` / `agent` / `coding-agent`。
- [x] `src/ai` / `agent` / `coding-agent`。
- [x] `apps/picpp/main.cpp`。
- [x] `pi::ai`、`pi::agent`、`pi::coding-agent` target。
- [x] `src/` 不作为 SDK PUBLIC include path。
- [x] ai Message/ContentBlock/Usage/StopReason 类型。
- [x] agent AgentMessage 类型。
- [x] ai L1 12 event + agent L2 10 event。
- [x] CancellationToken / CombinedCancellation 基础抽象。
- [x] ThreadGuard / string helpers。
- [x] 三个手工 JSONL fixture round-trip。
- [x] canonical `<pi/...>` consumer smoke source。

**仍不宣称：** 手工 fixture 能证明真实 pi compatibility。

**进入 v0.0.2 前唯一已知基础设施债：** CancellationToken callback-under-lock，需要在真实 HTTP 前加固。

---

### v0.0.2 — Provider / SSE + Differential 基座

**状态：✅ Final closeout 完成（2026-09-03）**

**目标：** 在已经稳定的 `ai` SDK 边界内打通第一条真实 Provider 流，同时建立后续版本共同使用的 pi `v0.80.0` compatibility gate。

#### T0：真实兼容基座

- [x] 固定 pi `v0.80.0` tag/commit：`f08e968c83d92bce5f5fd2f7f20ef37f8cf04a39`。
- [x] `tests/fixtures/pi-v0.80.0/README.md` 记录 upstream、生成命令与 normalization 边界。
- [x] exact pi source runner：确定性 local HTTP/SSE 输入 → normalized JSONL。
- [x] C++ public `OpenAICompatibleProvider` 使用同一输入 → normalized JSONL。
- [x] canonical `text-stop` / `tool-call` / `length-stop` traces 固化。
- [x] checked-in canonical fixture == fresh pi v0.80.0 == pi-cpp 三方 strict gate。
- [x] event ordering、partial message state、stopReason、usage、tool-call 聚合等稳定字段严格比较。
- [x] 非确定字段只允许显式 allow-list normalization；稳定业务字段不得隐藏。
- [x] Tau fixture 仅辅助回归，不作为 canonical compatibility evidence。

**范围说明：** v0.0.2 的 real differential canonical proof 聚焦 OpenAI-compatible L1 streaming。Session wire/tree/compaction 仍按 v0.1.0–v0.1.2 计划实现，不为了勾选 T0 在本版本伪造 session fixtures。

#### T1：CancellationToken 加固

- [x] request 锁内只做 cancelled transition + callback snapshot/detach。
- [x] 用户 callback 锁外执行。
- [x] register-after-cancel 不在内部锁下调用用户代码。
- [x] unregister/request race 明确定义并测试。
- [x] CombinedCancellation source request/destructor stress 无 UAF / deadlock。

#### T2：ai Provider API / EventStream

- [x] `<pi/ai/provider.hpp>`：Provider streaming interface。
- [x] `<pi/ai/event_stream.hpp>`：thread-safe producer/consumer event stream。
- [x] FakeProvider test support。
- [x] `<pi/ai/openai_compatible.hpp>` public Provider facade + PImpl。
- [x] 第三方 HTTP 类型不进入 public API。

#### T3：OpenAI-compatible SSE

- [x] `src/ai/http.cpp`：CPR/libcurl private streaming adapter。
- [x] connect timeout / total timeout / low-speed / cancellation semantics。
- [x] request builder + 独立 SSE parser + streaming JSON parser。
- [x] delta merge：text / thinking / tool_calls index/id/name/partial arguments。
- [x] finish_reason → StopReason。
- [x] usage / response id / response model capture。
- [x] malformed JSON、跨 chunk、多 data 行、`[DONE]`、取消测试。
- [x] retry/backoff/jitter/Retry-After/`retry-after-ms`/`x-should-retry`/错误分类。
- [x] 408 / 409 / 429 / 5xx / transport / cancel 路径有确定性测试。

**验收：**

1. [x] OpenAI-compatible public Provider 通过真实本地 HTTP/SSE 增量链路验证。
2. [x] 401/429/5xx/transport/cancel 以 terminal result/error 收尾，不因普通 Provider 错误终止进程。
3. [x] 三组 canonical trace 与 fresh exact pi `v0.80.0`、pi-cpp 三方一致。
4. [x] `pi::ai` 可被独立 consumer 使用，不依赖 agent/coding-agent。
5. [x] Linux/macOS/Windows CI 与 exact pi Compatibility workflow 全绿。

---

### v0.0.3 — Agent 主循环

**目标：** 实现通用 `agent` SDK，不依赖 coding-domain 具体工具。

- [ ] `<pi/agent/tool.hpp>`：AgentTool / result / executionMode。
- [ ] `<pi/agent/agent.hpp>`：prompt / steer / followUp / abort / subscribe / waitForIdle。
- [ ] runLoop 双层循环按 pi differential trace 对齐。
- [ ] `PendingMessageQueue` = `deque<AgentMessage>`。
- [ ] QueueMode：OneAtATime / All。
- [ ] steering / follow-up 两条独立队列，不允许单槽覆盖。
- [ ] 默认 parallel tool batch；支持 per-tool/global sequential。
- [ ] 结果消息顺序稳定。
- [ ] abort/error 后正确收尾，可再次 prompt。
- [ ] tool history repair。
- [ ] FakeProvider + fake tool 独立 agent consumer example/test。

**验收重点：** 多条 steer/followUp、不丢消息、工具顺序、取消、错误恢复与 pi 一致。

---

### v0.0.4 — coding-agent 工具四件套

**目标：** 在 agent SDK 上建立 coding domain SDK，真正能修改项目。

- [ ] `<pi/coding-agent/coding_agent.hpp>` facade。
- [ ] `<pi/coding-agent/tools.hpp>` tool registry/factory。
- [ ] read：范围/行号/截断/二进制策略。
- [ ] write：整文件/父目录。
- [ ] edit：唯一匹配、行尾保持、失败上下文、diff。
- [ ] bash：reproc++、timeout/cancel、POSIX process group、Windows Job Object。
- [ ] file mutation queue。
- [ ] beforeToolCall / afterToolCall hook。
- [ ] schema validation 错误转 ToolResult。

**验收：** 纯 C++ consumer 只链接 `pi::coding-agent` 完成 `read → edit → bash verify`。

---

### v0.1.0 — print MVP + Stable Session Wire

**阶段：能用。**

- [ ] system prompt：tools/guidelines/AGENTS.md/cwd/date。
- [ ] settings/custom OpenAI-compatible models/API key env。
- [ ] 完整 SessionHeader / SessionEntry wire union：message、thinking_level_change、model_change、compaction、branch_summary、custom、custom_message、label、session_info。
- [ ] 尚未开放行为的 entry 至少 parse/preserve/dump。
- [ ] append-only JSONL storage。
- [ ] `picpp -p` / `--model` / JSON event mode。
- [ ] `install(TARGETS ...)` / headers / export targets。
- [ ] `picppConfig.cmake`。
- [ ] install-tree clean consumer tests。

**验收：** CLI 可端到端改文件跑测试；外部 C++ consumer 不使用 CLI 也可完成等价流程。

---

### v0.1.1 — REPL + Resume

- [ ] line REPL + ANSI streaming + tool blocks。
- [ ] Ctrl+C 当前轮取消。
- [ ] `/help /model /clear /new /save /exit`。
- [ ] Windows VT/UTF-8；non-TTY fallback。
- [ ] session tree 基础 path query。
- [ ] `--continue` / `--resume <id>`。
- [ ] REPL 只消费 coding-agent public API/event。

---

### v0.1.2 — Session Tree + Compaction

- [ ] `/tree` / `/fork <entryId>`。
- [ ] BranchSummaryEntry。
- [ ] manual/auto compaction + CompactionEntry。
- [ ] token/context/cost state。
- [ ] 长 session 流式读取基础。
- [ ] 不重写历史 JSONL。

---

### v0.1.3 — Anthropic + Model Catalog

- [ ] Anthropic Messages SSE / thinking/signature / usage/stop reason。
- [ ] model metadata 以 pi `v0.80.0` 为第一来源。
- [ ] Tau catalog 只参考组织方式，不定义 canonical metadata。
- [ ] user catalog override。
- [ ] runtime model switch + model_change entry。
- [ ] provider compat flags 数据驱动。

---

### v0.2.0 — TUI

**阶段：好用。**

- [ ] FTXUI CJK/IME/Windows Terminal/conhost/WezTerm go/no-go spike。
- [ ] 不达标时增强 REPL fallback，不污染 SDK。
- [ ] message stream / tool fold / Markdown basic render。
- [ ] multiline input/history/paste。
- [ ] model/context/cost/branch status。
- [ ] approval UI / theme / `--no-tui`。

FTXUI 只能属于 `apps/picpp`。

---

### v0.2.1 — RPC + grep/find/ls

- [ ] `picpp --mode rpc` stdin/stdout JSONL。
- [ ] prompt/steer/followUp/abort/fork/compact/status/exit。
- [ ] Agent/Coding events 通知。
- [ ] grep/find/ls。
- [ ] stdout protocol pure；logs stderr。
- [ ] RPC adapter 只调 public SDK。

---

### v0.2.2 — Extensions v1

- [ ] subprocess manifest + protocolVersion/capabilities。
- [ ] register_tool / register_command。
- [ ] beforeToolCall / afterToolCall 首批 hooks。
- [ ] spawn/ready/timeout/cancel/crash/restart/disable。
- [ ] permission-gate / subagent examples。
- [ ] Extension Compatibility Matrix。

实现机制允许不同于 TS 动态加载，但承诺的行为面必须明确。

---

### v0.2.3 — Skills / Prompts / Resilience

- [ ] Skills discovery / expansion。
- [ ] Prompt templates → slash commands。
- [ ] JSONL 半行/崩溃恢复。
- [ ] configurable retry policy。
- [ ] 大 session 流式读取 / 低峰值内存。
- [ ] old fixture / corrupted tail / unknown entry-field tests。

---

### v0.3.0 — Core Complete / API Freeze Candidate

**含义：** pi `v0.80.0` 的**计划内核心 Coding Agent 功能基本完成**，不是全功能克隆。

准入：

- [ ] ai Provider/event/cancellation 主干完成。
- [ ] agent runLoop/queue/tool execution/abort/error 主干 Compatible。
- [ ] coding-agent tools/session/config/catalog/extensions/skills 计划内项完成或明确 Partial。
- [ ] print/REPL/TUI/RPC 适配稳定。
- [ ] 三层 SDK build-tree + install-tree consumer tests。

冻结工作：

- [ ] Compatibility Matrix 全量审计。
- [ ] public header naming/ownership/include hygiene/lifetime/error model 审计。
- [ ] 删除明显临时 public API。
- [ ] sanitizer/stress/cancellation/session-corruption tests。
- [ ] SDK examples/API reference。

v0.3.0 后新增主功能默认进入 post-v1 backlog。

---

### v0.3.x — Hardening / RC

允许：compatibility fix、bugfix、public API freeze 修正、性能/内存、跨平台、打包、tests/docs。

不允许：新 Provider 大类、新 UI 模式、新扩展体系、与 v1 无关的大功能。

重点：

- E2E matrix：平台 × Provider protocol × print/REPL/TUI/RPC。
- SDK consumer matrix：build tree / install tree / 三平台。
- 冷启动、idle memory、大 session、长 TUI benchmark。
- release artifact + checksum + install guide。
- known deviations / migration / API reference。

---

### v1.0.0 — Stable Contract

**不新增功能。**

准入：

- [ ] v0.3.x RC 稳定。
- [ ] Compatibility Matrix 无未解释 Planned 项。
- [ ] 三平台 executable artifact 干净环境可运行。
- [ ] `find_package(picpp CONFIG REQUIRED)` 三平台 consumer 通过。
- [ ] `pi::ai` / `pi::agent` / `pi::coding-agent` API 文档完整。
- [ ] deterministic differential tests 全绿。
- [ ] E2E smoke matrix 全绿。
- [ ] CHANGELOG / README / migration / known deviations 完整。

---

## 6. 测试策略

1. **SDK unit tests**：按 ai/agent/coding-agent 责任分层。
2. **Public consumer tests**：只 include `<pi/...>`；不允许依赖 `src/`。
3. **Install-tree tests**：v0.1.0 起 `find_package` clean consumer。
4. **真实 pi fixtures**：固定 `v0.80.0` tag/commit，记录来源。
5. **Differential tests**：同输入跑 pi reference harness 与 picpp，规范化后比较。
6. **FakeProvider tests**：runLoop/tool/cancel/queues 的确定性测试。
7. **Tau fixtures**：只辅助回归。
8. **真实 endpoint smoke**：本地或可选 scheduled；普通 PR 不依赖 secret。
9. **跨平台门**：Linux/macOS/Windows；终端/进程树另做真实走查。

允许 normalization：UUID、绝对时间、duration、临时绝对路径、pi 不承诺一致的平台换行。

禁止 normalization 掩盖：event ordering、role/type、tool pairing、stopReason、queue drain、session parentId、RPC semantics、用户可见错误分类。

---

## 7. v1.0 前明确非目标

- MCP 内置实现。
- OAuth 完整流程和 credential 加密存储。
- 内置 container/VM sandbox。
- terminal images / HTML export / telemetry / self-update。
- C++20 coroutine/modules。
- OpenAI Chat Completions compatible 与 Anthropic Messages 之外的 pi 原生 Provider 大类，例如 openai-responses / bedrock / vertex / mistral。
- 与 pi TS extension system 实现机制完全相同。
- v1.0 前 ABI 稳定承诺；先冻结 source/API compatibility，ABI 单独评估。

---

## 8. 风险与对策

| 风险 | 等级 | 对策 |
|---|---|---|
| SDK 边界被破坏 | 高 | v0.0.1 即建立 include/三 target；consumer test 守边界 |
| public API 泄漏第三方类型 | 高 | cpr/reproc/FTXUI 默认 private |
| “兼容 pi”范围失控 | 高 | Compatibility Matrix + Out of Scope |
| 仅靠源码阅读误判 | 高 | v0.0.2 起固定 reference harness/differential |
| Cancellation deadlock/UAF | 高 | v0.0.2 在 HTTP 前完成锁外 callback 与 race tests |
| Session wire 反复迁移 | 高 | v0.1.0 一次稳定完整计划内 SessionEntry union |
| steering/followUp 丢消息 | 高 | deque + QueueMode + 多消息 differential |
| Windows terminal/process 差异 | 高 | 三平台 CI + Job Object + TUI spike |
| Provider server fragmentation | 中 | compat flags 数据驱动；只承诺实测范围 |
| FTXUI CJK/IME | 中 | go/no-go spike + REPL fallback |
| Extensions 范围膨胀 | 中 | v0.2.2 冻结最小 protocol v1 |
| 单人周期过长 | 中 | 每个 tag 可发布；0.3.0 后冻结功能 |

---

## 9. 开发方法论

每个版本固定流程：

1. **基线取证**：先看 pi `v0.80.0` 对应源码和可观察行为。
2. **Public API 识别**：先判断属于 ai / agent / coding-agent 还是 private。
3. **设计先行**：写 `docs/design/vX.Y.Z.md`。
4. **TDD 拆解**：兼容任务优先写 fixture/differential red test；SDK API 优先写 consumer compile test。
5. **实现**：核心能力进入 SDK；CLI/TUI 只做 adapter/render/input。
6. **验收**：unit + consumer + FakeProvider + differential + 三平台 CI。
7. **回顾**：记录 API 变化、偏差、Compatibility Matrix。
8. **closeout**：README / CHANGELOG / docs / tag 一致。

### 9.1 版本纪律

- 主干始终应可构建，CI 红优先处理。
- 已发布 tag 不回退上版功能。
- `v0.3.0` 起冻结主功能面。
- `v0.3.x` 只做稳定化。
- `v1.0.0` 不新增功能。
- v1.0 后 public SDK/API 按 SemVer 管理。
