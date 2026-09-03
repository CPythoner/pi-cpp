# pi-cpp 开发计划（版本化路线图）

> **For agentic workers:** 本文档是主路线图。每个版本启动时，先把该版本拆解为任务级实施计划（TDD、bite-sized tasks），再进入实现。各版本内的功能清单用 checkbox 跟踪。

**目标：** 用 C++17 实现 pi agent（github.com/earendil-works/pi，TS monorepo）的核心功能，并把 `pi-ai`、`pi-agent-core`、`pi-coding-agent` 三层都建设成可独立供外部 C++ 项目调用的 SDK；`picpp` CLI/TUI 只是这些 SDK 的一个官方前端。行为语义基线固定为 pi `v0.80.0`；Tau `v0.4.1` 仅作为实现参考。

项目目标不是一次性复制 pi `v0.80.0` 的全部 Provider、认证和扩展实现，而是对路线图承诺的核心 Agent / Coding Agent 能力做到可验证的行为兼容，并在 `v0.3.0` 前完成核心功能面，在 `v1.0.0` 冻结稳定对外契约。

**技术栈：** C++17 · CMake ≥3.25 · cpr/curl · nlohmann/json · reproc++ · fmt · doctest · FTXUI（后期）· cxxopts

---

## 1. 已确认的决策

| 决策项 | 选择 |
|---|---|
| 行为语义基线 | **pi `v0.80.0`**；计划内能力的 API、CLI、事件、会话、工具调用及 wire 语义以该 tag 的可观察行为为准 |
| Tau 参考版本 | **Tau `v0.4.1`**；只参考模块拆分、算法和工程实现，不作为行为规范来源 |
| 兼容目标 | **核心能力子集兼容**，不是 pi `v0.80.0` 全功能克隆；Compatibility Matrix 标记 Compatible / Partial / Planned / Out of Scope |
| SDK 形态 | `pi-ai` → `pi-agent-core` → `pi-coding-agent` 三层都是独立 CMake library / SDK；CLI/TUI 只能依赖 SDK，不反向渗透业务逻辑 |
| 公共头文件 | 所有对外 API 放 `include/pi/...`；`src/` 只放实现与 private headers，不再把 `src/` 作为 PUBLIC include path |
| CMake target | `pi_ai` (`pi::ai`) → `pi_agent_core` (`pi::agent_core`) → `pi_coding_agent` (`pi::coding_agent`)；`picpp` PRIVATE link `pi::coding_agent` |
| 语言标准 | **C++17**；GCC 9+ / Clang 7+ / VS2019+；不依赖 coroutine/modules |
| UI 形态 | `v0.1.0` print MVP → `v0.1.1` REPL → `v0.2.0` TUI；CLI/TUI 始终只是 SDK consumer |
| Provider 策略 | `v0.0.2` OpenAI Chat Completions compatible；`v0.1.3` Anthropic Messages；其他协议按需后续扩展 |
| 平台 | Linux / macOS / Windows 从第一天支持 |
| 分发形态 | 三平台 `picpp` executable artifact + 可安装/可消费的 CMake SDK targets；不把“完全静态链接”作为跨平台硬约束 |
| 二进制名 | `picpp`；配置目录 `~/.picpp/`；C++ 顶层命名空间 `pi` |

### 1.1 兼容基线与冲突裁决

pi `v0.80.0` 与 Tau `v0.4.1` 并非同期基线。本项目采用“**pi 定义行为，Tau 参考实现**”策略：

1. 对路线图明确实现的能力，API、事件、session、tool、CLI/RPC wire 等可观察语义以 pi `v0.80.0` 为规范来源。
2. C++ 类型设计、模块组织、并发模型和存储算法可以参考 Tau `v0.4.1`。
3. 两者有差异时，按 **pi `v0.80.0` 可观察行为 > 本项目明确兼容规范 > Tau `v0.4.1` 实现** 裁决。
4. Tau 中来自更高版本 pi 或 Tau 自身扩展的能力，不自动进入当前基线。
5. 兼容测试优先使用 pi `v0.80.0` 真实样本与 differential tests；Tau fixture 只能作为辅助回归。
6. pi 中存在但本路线图标成 Planned / Out of Scope 的能力，不计为当前版本兼容缺陷。

特别说明：`packages/orchestrator` 在 pi `v0.80.0` 中尚不存在，后来才加入并在 pi `v0.81.0` 改名为 `packages/server`，因此不属于当前兼容基线。

### 1.2 Compatibility Matrix（持续维护）

状态：

- **Compatible**：已实现，并有 pi `v0.80.0` fixture 或 differential test 证明计划内语义兼容。
- **Partial**：主要路径已实现，但存在明确记录的偏差或未覆盖子能力。
- **Planned**：属于目标，但尚未到对应版本。
- **Out of Scope**：v1.0.0 前明确不做。

| 能力面 | v1.0.0 目标状态 | 计划里程碑 |
|---|---|---|
| `pi-ai` public SDK | Compatible / Stable API | v0.0.2 建边界，v0.3.0 API freeze candidate |
| `pi-agent-core` public SDK | Compatible / Stable API | v0.0.3 完成主干，v0.3.0 freeze candidate |
| `pi-coding-agent` public SDK | Compatible / Stable API | v0.1.x–v0.2.x 完成主干，v0.3.0 freeze candidate |
| AgentMessage / AgentEvent wire | Compatible | v0.0.2+ differential gate |
| Agent runLoop | Compatible | v0.0.3 |
| read/write/edit/bash | Compatible | v0.0.4 |
| grep/find/ls | Compatible | v0.2.1 |
| Session JSONL/tree/compaction | Compatible | v0.1.0–v0.1.2 |
| OpenAI Chat Completions compatible | Compatible | v0.0.2 |
| Anthropic Messages | Compatible | v0.1.3 |
| print / REPL / TUI | Compatible 或文档化 Partial | v0.1.0 / v0.1.1 / v0.2.0 |
| RPC | Compatible 或文档化 Partial | v0.2.1 |
| Extensions | Partial | v0.2.2；C++ 进程外协议替代 TS 动态加载 |
| Skills / Prompt templates | Compatible 或 Partial | v0.2.3 |
| 其他 pi 内置 Provider 协议 | Out of Scope | openai-responses / bedrock / vertex / mistral 等 |
| OAuth / credential store | Out of Scope | v1.0 前只做 API key / 配置 |
| MCP | Out of Scope | 后续可通过 extension 适配 |

每个版本 closeout 都必须更新该矩阵与 known deviations。

### 1.3 版本号策略

`v1.0.0` 前采用“**成熟度阶段 + 阶段内里程碑**”的版本策略，避免每个技术主题都提升 minor 版本：

- `v0.0.x`：架构地基，SDK 边界、Provider、Agent Loop、基础工具。
- `v0.1.x`：**能用**。完成 print MVP、REPL、session tree/compaction、多 Provider。
- `v0.2.x`：**好用**。完成 TUI、RPC、只读工具、extensions、skills/resilience。
- `v0.3.0`：**核心完成**。pi `v0.80.0` 的计划内核心 Coding Agent 功能基本完成，开始兼容性/稳定性冻结。
- `v0.3.x`：不再扩主功能面，只做 compatibility fix、bugfix、性能、跨平台、SDK/API 打磨与 release candidate。
- `v1.0.0`：冻结计划内行为兼容契约和对外 SDK/API；不是“100% 复制 pi 全部功能”的含义。

在 `0.x` 阶段允许 patch 位承载同一成熟度阶段内的功能里程碑；进入 `v1.0.0` 后按稳定 SemVer 纪律管理 public API 与兼容性。

---

## 2. 参考蓝本与核心移植原则

**pi `v0.80.0`**：核心包包括 ai / agent / coding-agent / tui。Agent 核心是 runLoop 双层循环、AgentEvent 事件流和 JSONL 树状 session。默认 coding tools 是 read/write/edit/bash，同时已有 grep/find/ls 等能力。

**Tau `v0.4.1`**：Python 参考实现更紧凑，适合辅助理解 SSE、主循环和模块组织，但任何 wire/行为兼容结论必须回到 pi `v0.80.0` 验证。

关键原则：

1. 普通 Provider/Tool 错误不能让 Agent 主进程崩溃，应转成可观察的 error message/result。
2. 消息分层：LLM/provider 类型属于 `pi-ai`；AgentMessage/AgentEvent 属于 `pi-agent-core`；session/coding-domain 类型属于 `pi-coding-agent`。
3. session 是 append-only JSONL 树；fork/branch/compaction 追加 entry，不重写历史。
4. 工具默认可并行；需要顺序语义的工具或文件 mutation 显式串行。
5. steering / follow-up 是两条可积压队列，支持 one-at-a-time / all drain，不能实现成单槽。
6. 兼容性优先用“同输入双实现差分”证明，而不是只靠源码阅读。
7. **三层核心库必须先是 SDK，再是 CLI 的内部实现。** 任何 CLI/TUI 专属依赖不得进入下层 public API。

---

## 3. SDK 架构与源码组织

### 3.1 目标依赖方向

```text
external app / picpp CLI / picpp TUI
                 │
                 ▼
        pi-coding-agent SDK
                 │
                 ▼
         pi-agent-core SDK
                 │
                 ▼
             pi-ai SDK
```

依赖只能向下，不允许：

- `pi-ai` include `pi-agent-core` / `pi-coding-agent`。
- `pi-agent-core` include `pi-coding-agent`。
- SDK public header include CLI/TUI header。
- SDK public API 暴露 `src/...` private type。

### 3.2 目标目录结构

```text
pi-cpp/
├── CMakeLists.txt
├── cmake/
│   ├── deps.cmake
│   ├── picppConfig.cmake.in
│   └── install.cmake
├── include/
│   └── pi/
│       ├── ai/                         # pi-ai PUBLIC API
│       │   ├── message.hpp
│       │   ├── model.hpp
│       │   ├── provider.hpp
│       │   ├── events.hpp
│       │   ├── event_stream.hpp
│       │   └── cancellation.hpp
│       ├── agent/                      # pi-agent-core PUBLIC API
│       │   ├── message.hpp
│       │   ├── events.hpp
│       │   ├── tool.hpp
│       │   ├── agent.hpp
│       │   └── queue_mode.hpp
│       └── coding/                     # pi-coding-agent PUBLIC API
│           ├── coding_agent.hpp
│           ├── config.hpp
│           ├── model_catalog.hpp
│           ├── session.hpp
│           ├── session_entry.hpp
│           ├── tools.hpp
│           └── extension.hpp           # 到 v0.2.2 再开放
├── src/
│   ├── ai/                             # pi-ai implementation/private details
│   │   ├── openai_compatible.cpp
│   │   ├── anthropic.cpp
│   │   ├── stream_canon.cpp
│   │   ├── retry.cpp
│   │   ├── http.cpp
│   │   └── detail/
│   ├── agent/                          # pi-agent-core implementation
│   │   ├── agent.cpp
│   │   ├── agent_loop.cpp
│   │   ├── tool_history.cpp
│   │   └── detail/
│   └── coding/                         # pi-coding-agent implementation
│       ├── tools/
│       ├── session/
│       ├── compaction.cpp
│       ├── system_prompt.cpp
│       ├── config.cpp
│       ├── catalog.cpp
│       └── detail/
├── apps/
│   └── picpp/                          # 官方 CLI/TUI，只消费 pi::coding_agent
│       ├── main.cpp
│       ├── print_mode.cpp
│       ├── repl.cpp
│       ├── rpc_mode.cpp
│       └── tui/
├── tests/
│   ├── ai/
│   ├── agent/
│   ├── coding/
│   ├── consumer/                       # 外部消费者编译测试
│   ├── fixtures/pi-v0.80.0/
│   └── differential/
├── examples/
│   ├── ai_chat/
│   ├── agent_core/
│   └── coding_agent/
└── docs/
```

### 3.3 CMake target 与 SDK 消费契约

目标 target：

```cmake
pi_ai           -> alias pi::ai
pi_agent_core   -> alias pi::agent_core
pi_coding_agent -> alias pi::coding_agent
picpp           -> executable
```

依赖关系：

```cmake
pi_agent_core   PUBLIC pi::ai
pi_coding_agent PUBLIC pi::agent_core
picpp           PRIVATE pi::coding_agent
```

公共 include 目录必须使用：

```cmake
target_include_directories(<sdk> PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
```

**禁止把 `${PROJECT_SOURCE_DIR}/src` 放进 PUBLIC include path。** `src/` 可以作为 target PRIVATE include 路径。

SDK 至少支持两种消费方式：

1. 仓库内/源码方式：`add_subdirectory(pi-cpp)` 后 `target_link_libraries(app PRIVATE pi::coding_agent)`。
2. 安装包方式：`cmake --install` 后 `find_package(picpp CONFIG REQUIRED)`，再链接 `pi::ai` / `pi::agent_core` / `pi::coding_agent`。

### 3.4 Public API 纪律

- Public header 只放到 `include/pi/...`。
- `detail/`、HTTP parser、retry state、session storage implementation 等默认 private。
- public header 不得 include `src/...`。
- public header 只能依赖标准库、对外声明的第三方类型，或同层/下层 public header；尽量避免把 cpr/reproc/FTXUI 类型暴露到 SDK API。
- Provider、Agent、CodingAgent 的异步/取消/事件接口优先用项目自己的稳定抽象，避免第三方库成为 ABI/API 契约。
- `0.x` 不承诺 ABI 稳定，但每个 public API 破坏性调整必须在 CHANGELOG 标明；`v0.3.0` 起进入 API freeze candidate。
- `v1.0.0` 后 public API 变化按 SemVer 管理。

### 3.5 当前源码组织的迁移结论

`v0.0.1` 的 `pi_types` + `src/` PUBLIC include 是骨架阶段临时结构，不作为长期架构。**在 v0.0.2 开始 Provider 开发前必须先迁移 SDK 边界**：

1. 创建 `include/pi/ai`、`include/pi/agent`、`include/pi/coding`。
2. 把已属于 public contract 的 v0.0.1 types 从 `src/` 迁入对应 `include/pi/...`。
3. 拆除 `pi_types`，形成 `pi_ai` / `pi_agent_core`，并预建 `pi_coding_agent` target。
4. `src/` 从 PUBLIC include path 移除。
5. 增加三个最小 consumer build tests，证明各 SDK 能独立被外部 target include/link。
6. 后续新增 API 必须先判断 public/private，再决定进入 `include/` 还是 `src/`。

---

## 4. 并发与基础设施约束

### 4.1 并发模型

- **调用方线程/前端线程**：CLI/TUI 或外部 SDK consumer 驱动 Agent/CodingAgent，并订阅事件。
- **网络 worker**：每个活跃 LLM stream 使用受控 worker；HTTP callback 推送标准化事件。
- **工具执行**：默认并行；工具可声明 sequential；edit/write 冲突写操作至少由 file-mutation queue 串行化。
- **取消**：CancellationToken 从 public SDK API 贯穿 Agent → Provider → Tool → process。
- **消息队列**：steering/followUp 各持有 `deque<AgentMessage>`，默认 one-at-a-time，同时支持 all。

### 4.2 CancellationToken 约束

v0.0.1 已交付自研 CancellationToken；进入真实 HTTP 前必须加固：

1. `request()` 锁内只做状态转换和 callback snapshot/detach。
2. 用户 callback 解锁后执行，避免重入死锁。
3. unregister / request / destructor 生命周期有明确同步规则与竞态测试。
4. 只声明“接口目标接近 stop-token 取消模型”，未经逐项验证不宣称与 `std::stop_token` 完全等价。

### 4.3 技术选型原则

| 领域 | 选型 | 关键注意点 |
|---|---|---|
| HTTP+SSE | cpr/curl | cpr 只存在于 `pi-ai` implementation；不要泄漏到 public API；流式请求避免整体 wall-clock timeout |
| JSON | nlohmann/json | wire 类型字段省略/null/未知字段策略必须有 fixture 验证 |
| 子进程 | reproc++ | 只存在于 coding implementation；Unix process group + Windows Job Object |
| TUI | FTXUI | 只属于 `apps/picpp`；绝不能成为 `pi-coding-agent` SDK 依赖 |
| 格式化 | fmt | 可用于实现，但 public API 尽量不暴露 fmt 类型 |
| 测试 | doctest | SDK 单元测试 + consumer build tests + differential tests |
| CLI | cxxopts | 只属于 `apps/picpp` |
| 构建 | CMake ≥3.25 | 精确 pin 第三方；SDK targets 可 build/install/export |
| CI | GitHub Actions | Linux/macOS/Windows；PR 不依赖真实外部 API secret |

---

## 5. 里程碑总览

| 版本 | 成熟度 | 主题 | 交付标志 |
|---|---|---|---|
| v0.0.1 | 地基 | 骨架与核心类型 | 三平台 CI；基础 message/event types（已完成） |
| v0.0.2 | 地基 | **SDK 边界 + Provider/SSE** | `pi::ai` / `pi::agent_core` 基础 target；OpenAI-compatible 真实流；differential 基座 |
| v0.0.3 | 地基 | Agent 主循环 | `pi::agent_core` 可独立使用；多轮 tool loop + steering/followUp + abort |
| v0.0.4 | 地基 | Coding 工具四件套 | `pi::coding_agent` 基础可用；read/write/edit/bash |
| v0.1.0 | **能用** | print MVP + Session schema | `picpp -p` 端到端；完整 SessionEntry；三层 SDK 可安装/消费 |
| v0.1.1 | 能用 | REPL + resume | 三平台交互、会话恢复 |
| v0.1.2 | 能用 | Session tree + compaction | fork/branch/tree + 长会话压缩 |
| v0.1.3 | 能用 | Anthropic + model catalog | 第二 Provider 协议 + runtime model switch |
| v0.2.0 | **好用** | TUI | FTXUI 全屏交互；SDK 不依赖 UI |
| v0.2.1 | 好用 | RPC + grep/find/ls | headless 集成 + 完整只读工具 |
| v0.2.2 | 好用 | Extensions v1 | 进程外 tool/command/hook extension |
| v0.2.3 | 好用 | Skills / Prompts / Resilience | skills、prompt templates、crash-safe session、大会话加载 |
| v0.3.0 | **核心完成** | Compatibility / API Freeze Candidate | pi 计划内核心 Coding Agent 能力基本完成；不再扩主功能面 |
| v0.3.x | 稳定化 | Hardening / RC | compatibility fix、SDK/API 打磨、性能、E2E、打包 |
| v1.0.0 | 稳定 | Stable Contract | 冻结计划内行为兼容与 public SDK/API；正式发布 |

---

## 6. 各版本详细计划

### v0.0.1 — 骨架与核心类型（已完成）

**目标：** 三平台编译与 CI；建立 message/event 类型和 C++17 基础设施。

历史实现以 `docs/design/v0.0.1.md` 和 CHANGELOG 为准，不反向修改已完成版本事实。

**已确认遗留：**

- fixtures 中有手工样本，不能单独证明 pi `v0.80.0` 真实兼容。
- CancellationToken 进入真实 HTTP 前需加固 callback 锁外执行。
- 当前 `pi_types` + `src/` PUBLIC include 是临时结构，必须在 v0.0.2 迁移到三层 SDK 边界。

---

### v0.0.2 — SDK 边界 + Provider 层与 SSE

**目标：** 在继续增加功能前先建立正确的公开 API 边界；完成 `pi-ai` 第一条真实 Provider 流并建立 differential 基座。

**T0：SDK 边界重构**

- [ ] 建立 `include/pi/ai`、`include/pi/agent`、`include/pi/coding`。
- [ ] 将 v0.0.1 已公开类型迁移到正确 public header；实现留在 `src/`。
- [ ] 拆 `pi_types`：建立 `pi_ai` (`pi::ai`) 与 `pi_agent_core` (`pi::agent_core`)；建立空/最小 `pi_coding_agent` target 为后续预留。
- [ ] 移除 `src/` 的 PUBLIC include；private headers 只通过 PRIVATE include 使用。
- [ ] 加 `tests/consumer/ai`、`agent`、`coding` 最小编译/链接测试。
- [ ] 明确 namespace 与 include 形式：`#include <pi/ai/...>`、`<pi/agent/...>`、`<pi/coding/...>`。

**T1：兼容测试基座**

- [ ] 从 pi `v0.80.0` 固定 tag 生成/采集真实 message/event/session fixtures，记录来源 commit。
- [ ] 最小 TS reference harness 输出规范化 `trace.jsonl`。
- [ ] C++ 对相同输入输出 normalized trace，比较事件顺序、消息字段、stopReason、tool-call 聚合等稳定字段。
- [ ] Tau fixture 只做辅助回归。

**T2：CancellationToken 加固**

- [ ] callback snapshot 后锁外执行。
- [ ] callback 重入、unregister/request 并发、CombinedCancellation 析构竞态测试。
- [ ] 把 cancellation 作为 `pi-ai` 可复用 public abstraction，不暴露 cpr/curl 类型。

**T3：pi-ai Provider/SSE**

- [ ] `<pi/ai/provider.hpp>`：Provider public interface。
- [ ] `<pi/ai/events.hpp>` / `<pi/ai/event_stream.hpp>`：L1 event stream contract。
- [ ] `src/ai/http.cpp`：cpr/curl private adapter。
- [ ] `src/ai/openai_compatible.cpp`：请求、SSE、delta merge、tool_calls、finish_reason、usage。
- [ ] `src/ai/stream_canon.cpp`：Start/Delta/End 归一化。
- [ ] `src/ai/retry.cpp`：backoff/jitter/Retry-After/错误分类。
- [ ] FakeProvider 作为 SDK 测试设施或 test support target。
- [ ] `examples/ai_chat/`：只链接 `pi::ai` 的外部调用示例。

**验收：**

1. 三层 target 的最小外部 consumer 都能 build/link。
2. `src/` 不再是任何 SDK target 的 PUBLIC include path。
3. OpenAI-compatible endpoint 可逐增量输出。
4. SSE/错误/取消路径有完整单测。
5. 至少一组 pi reference trace 与 C++ normalized trace 一致。

---

### v0.0.3 — pi-agent-core / Agent 主循环

**目标：** 完成不依赖 coding tools 具体实现的通用 Agent SDK。

**功能：**

- [ ] `<pi/agent/tool.hpp>`：AgentTool / AgentToolResult / executionMode。
- [ ] `<pi/agent/agent.hpp>`：prompt / steer / followUp / abort / subscribe / waitForIdle。
- [ ] `<pi/agent/message.hpp>` / events：Agent 层 public contract。
- [ ] runLoop 双层循环；顺序以 pi differential trace 为门。
- [ ] `PendingMessageQueue` 使用 `deque<AgentMessage>`；支持 OneAtATime / All。
- [ ] steering/followUp drain timing 对齐 pi。
- [ ] tool batch 默认 parallel，支持 per-tool/global sequential。
- [ ] abort/error 后可正确收尾并再次 prompt。
- [ ] tool history repair。
- [ ] `examples/agent_core/`：FakeProvider + test tool 构建独立 Agent，不依赖 coding-agent。

**验收：** `pi::agent_core` 可被外部 app 直接调用；多消息 queue、parallel/sequential、abort/error 差分测试通过。

---

### v0.0.4 — pi-coding-agent / 工具四件套

**目标：** 在 `pi-agent-core` 上构建 coding domain SDK，达到真正修改代码的能力。

**功能：**

- [ ] `<pi/coding/coding_agent.hpp>`：高层 CodingAgent facade，组合 Provider/Agent/tools/session/config。
- [ ] `<pi/coding/tools.hpp>`：默认 tool factory / registry 的 public API；具体实现保持 private。
- [ ] read：行范围、行号、截断、二进制策略。
- [ ] write：整文件写入、父目录创建。
- [ ] edit：唯一匹配、行尾保持、失败上下文、diff。
- [ ] bash：reproc++ private implementation；timeout/cancel；POSIX process group + Windows Job Object。
- [ ] file mutation queue。
- [ ] beforeToolCall / afterToolCall hook。
- [ ] tool schema validation。
- [ ] `examples/coding_agent/`：外部 target 只链接 `pi::coding_agent` 完成 read→edit→bash。

**验收：** CodingAgent SDK 能脱离 `picpp` executable 独立使用；CLI 以后不得绕过 SDK 直接调用 private coding implementation。

---

### v0.1.0 — print MVP + Stable Session Wire

**阶段含义：** **能用。** 第一个真实可用版本。

**目标：** `picpp -p` 端到端，同时稳定 SessionHeader / SessionEntry wire 模型，并完成三层 SDK 的基础安装/导出能力。

**功能：**

- [ ] system prompt：tools/guidelines/AGENTS.md/cwd/date。
- [ ] config：settings、custom OpenAI-compatible models、API key env。
- [ ] `<pi/coding/session_entry.hpp>`：计划内完整 SessionEntry union：message、thinking_level_change、model_change、compaction、branch_summary、custom、custom_message、label、session_info 等。
- [ ] 尚未开放行为的 entry 至少能 parse/preserve/dump。
- [ ] append-only JSONL storage。
- [ ] `apps/picpp/print_mode.cpp`：`picpp -p`、`--model`、JSON event mode、退出码。
- [ ] `install(TARGETS ...)` + `install(DIRECTORY include/)`。
- [ ] `picppConfig.cmake` + exported targets，使 `find_package(picpp CONFIG REQUIRED)` 可用。
- [ ] installed-tree consumer test：在临时外部 CMake 项目中分别链接 `pi::ai`、`pi::agent_core`、`pi::coding_agent`。

**验收：** 临时项目用 CLI 可完成“改文件+测试”；另一个纯 C++ consumer 不使用 CLI 也能通过 SDK 完成同类流程；真实 pi session fixtures 可读取关键 entry。

---

### v0.1.1 — REPL + Session Resume

**目标：** 日常可用的行式交互；session 可恢复。

**功能：**

- [ ] REPL + ANSI streaming + tool blocks。
- [ ] spinner / Ctrl+C。
- [ ] `/help` `/model` `/clear` `/new` `/save` `/exit`。
- [ ] Windows VT + UTF-8；non-TTY 去色。
- [ ] session id/parentId 基础 tree query。
- [ ] `--continue` / `--resume <id>`。
- [ ] REPL 只消费 `pi::coding_agent` public API / events，不复制 agent state machine。

**验收：** 三平台中文输入输出；interrupt→resume 完整；CLI 层无业务状态机复制。

---

### v0.1.2 — Session Tree + Compaction

**目标：** 长会话生存能力与分支探索。

**功能：**

- [ ] `/tree` + `/fork <entryId>`。
- [ ] branch summary entry。
- [ ] compaction：token 估算、手动/自动触发、CompactionEntry、LLM context 裁剪。
- [ ] `/cost` `/context`。
- [ ] 大 session 流式 tree/path 查询基础。
- [ ] 对外 session API 通过 `<pi/coding/session.hpp>` 暴露，不让调用方依赖 storage implementation。

**验收：** 超长对话继续；两条分支独立；旧 JSONL 不重写；pi compaction/branch fixture 可兼容读取。

---

### v0.1.3 — Anthropic + Model Catalog

**目标：** 第二条原生 Provider 协议与数据驱动模型元数据。

**功能：**

- [ ] `pi-ai` Anthropic Messages SSE、thinking/signature、stop reason、usage。
- [ ] thinking level 内部归一化，但 Provider wire 各自保持真实语义。
- [ ] `pi-coding-agent` model catalog；**元数据以 pi `v0.80.0` 为第一来源**，Tau 仅参考文件组织。
- [ ] 用户 catalog override。
- [ ] runtime model switch + model_change entry。
- [ ] compat flags 数据驱动化。

**验收：** Anthropic + 至少两个 OpenAI-compatible endpoint；同 session 切模型；Provider 实现变化不破坏 `pi::agent_core` public API。

---

### v0.2.0 — TUI

**阶段含义：** **好用。** 从“能用”进入日常工具形态。

**目标：** 全屏组件化前端，但 TUI 完全位于 `apps/picpp`，核心 SDK 不增加 UI 依赖。

**功能：**

- [ ] FTXUI CJK/IME/Windows Terminal/conhost/WezTerm go/no-go spike。
- [ ] 不达标时降级增强 REPL/raw mode，不强行污染 SDK。
- [ ] message stream、tool fold、Markdown basic rendering。
- [ ] multiline input/history/paste。
- [ ] model/context/cost/branch status。
- [ ] beforeToolCall approval UI。
- [ ] theme + `--no-tui`。

**验收：** `pi_ai` / `pi_agent_core` / `pi_coding_agent` targets 均不 link FTXUI；所有核心行为仍能在非 TUI consumer tests 回归。

---

### v0.2.1 — RPC + grep/find/ls

**目标：** headless 集成与补齐计划内只读工具。

**功能：**

- [ ] `picpp --mode rpc` stdin/stdout JSONL。
- [ ] prompt/steer/followUp/abort/fork/compact/status/exit 等计划内命令。
- [ ] Agent/Coding events 以通知形式输出。
- [ ] grep/find/ls。
- [ ] ignore/path/truncation 行为按 pi 对齐；偏差则标 Partial。
- [ ] RPC adapter 只调用 `pi::coding_agent` public API，不访问 private `src/coding`。

**验收：** harness 经 RPC 驱动完整流程；stdout 协议纯净、日志 stderr；大目录/取消测试通过。

---

### v0.2.2 — Extensions v1

**目标：** 建立 C++ 进程外扩展模型，不复制 TS 动态加载机制。

**功能：**

- [ ] extension manifest。
- [ ] subprocess handshake + protocolVersion + capabilities。
- [ ] register_tool / register_command。
- [ ] beforeToolCall / afterToolCall 等首批 hook。
- [ ] spawn/ready/timeout/cancel/crash/restart/disable 生命周期。
- [ ] stdout protocol / stderr log 隔离。
- [ ] permission-gate、subagent examples。
- [ ] `<pi/coding/extension.hpp>` 只暴露稳定 host-side API；协议 parser private。
- [ ] Extension Compatibility Matrix。

**验收：** extension crash 不拖垮主 agent；permission gate 能 block；subagent cancel 可控。

---

### v0.2.3 — Skills / Prompts / Resilience

**目标：** 补齐高频用户能力和 session 韧性，为 v0.3.0 功能冻结做准备。

**功能：**

- [ ] Skills discovery / expansion。
- [ ] Prompt templates → slash commands。
- [ ] JSONL 半行/崩溃恢复。
- [ ] configurable retry policy。
- [ ] 大 session 流式读取/低峰值内存。
- [ ] old fixture / corrupted tail / unknown entry-field compatibility tests。
- [ ] skills/prompts 功能在 CLI/TUI/RPC 和纯 SDK consumer 中共享同一 coding layer。

**验收：** 大 session 可恢复；崩溃尾行不破坏历史；无 CLI-only 业务实现。

---

### v0.3.0 — Core Complete / Compatibility & API Freeze Candidate

**阶段含义：** **pi `v0.80.0` 的计划内核心 Coding Agent 功能基本完成。** 不是 pi 全功能克隆。

**目标：** 从“继续补功能”切换成“收敛契约”。本版本之后原则上不再新增主功能面。

**准入条件：**

- [ ] `pi-ai`：计划内 Provider/API/event/cancellation 主干完成。
- [ ] `pi-agent-core`：runLoop、queue、tool execution、abort/error 主干 Compatible。
- [ ] `pi-coding-agent`：默认工具、只读工具、session tree/compaction、config/catalog、extensions/skills 计划内能力完成或明确 Partial。
- [ ] print/REPL/TUI/RPC 已有稳定适配层。
- [ ] 三层 SDK 均有 build-tree + install-tree external consumer tests。

**功能冻结工作：**

- [ ] Compatibility Matrix 全量审计；无未解释 Planned 项。
- [ ] public headers 审计：命名、ownership、include hygiene、异常/错误/生命周期约定。
- [ ] CMake export/install/package config 审计。
- [ ] 对 public API 做第一轮 freeze candidate，删除明显不合理的临时 API。
- [ ] SDK examples/API reference 文档。
- [ ] sanitizer / stress / cancellation race / session corruption tests 补齐。

**验收：** `v0.3.0` 后新增需求默认进入 post-v1 backlog，除非属于兼容缺失或阻塞 v1 的必要修复。

---

### v0.3.x — Hardening / Release Candidate

**目标：** 不扩功能，只提升稳定性与发布质量。

允许：

- compatibility fix
- bugfix
- public API freeze 修正
- performance/memory
- cross-platform fix
- packaging/install/export fix
- tests/docs/release tooling

不允许：

- 新 Provider 大类
- 新 UI 模式
- 新扩展体系
- 与 v1 目标无关的大规模 feature

重点工作：

- [ ] E2E matrix：平台 × Provider 协议 × print/REPL/TUI/RPC。
- [ ] SDK consumer matrix：build tree / install tree / static/shared（若支持）/三平台。
- [ ] 冷启动、空闲内存、大 session、长 TUI 等基准。
- [ ] release artifact + checksums + install guide。
- [ ] known deviations / migration / API reference 完整。

---

### v1.0.0 — Stable Contract

**目标：** 冻结路线图承诺范围内的行为兼容契约与 C++ public SDK/API。**不新增功能。**

**准入条件：**

- [ ] `v0.3.x` RC 稳定。
- [ ] Compatibility Matrix 无未解释 Planned 项。
- [ ] 三平台 executable artifact 可从干净机器运行。
- [ ] `find_package(picpp CONFIG REQUIRED)` 在三平台干净 consumer 工程通过。
- [ ] `pi::ai`、`pi::agent_core`、`pi::coding_agent` public headers/API 文档完整。
- [ ] deterministic differential tests 全绿。
- [ ] E2E smoke matrix 全绿。
- [ ] CHANGELOG / README / migration / known deviations 完整。

**v1.0.0 的含义：** 对“我们承诺实现的 pi `v0.80.0` 核心子集”稳定负责，而不是宣称复制 pi 的所有 Provider、认证、扩展实现和外围功能。

---

## 7. 测试策略

1. **SDK 单元测试**：按 `ai/agent/coding` 分层；序列化、SSE、delta merge、session tree、cancel races、tools 等。
2. **Public consumer tests**：每层至少一个独立 target，只 include `<pi/...>`，不允许添加 `src/` include path。
3. **Install-tree tests**：安装后新建干净 CMake consumer，用 `find_package(picpp CONFIG REQUIRED)` 编译运行。
4. **pi 真实黄金样本**：固定 `v0.80.0` tag，记录生成来源与 commit。
5. **Differential tests**：同一确定性输入分别跑 pi reference harness 与 picpp，规范化后比较 event/message/tool ordering/stop/session/RPC semantics。
6. **FakeProvider tests**：确定性驱动 runLoop/tool/cancel/steering/followUp。
7. **Tau fixtures**：只作辅助互操作与实现回归。
8. **真实 endpoint smoke**：本地或可选 scheduled workflow；普通 PR CI 不依赖 secret。
9. **跨平台门**：Linux/macOS/Windows CI；终端/进程树额外真实走查。
10. **版本 closeout**：更新 Compatibility Matrix、SDK API changes、known deviations。

### 7.1 差分规范化原则

可以规范化：UUID、绝对时间、真实 duration、临时目录绝对路径，以及 pi 本身不承诺一致的平台换行。

不能通过规范化隐藏：event ordering、message role/type、tool call/result pairing、stopReason、queue drain、session parentId、RPC semantics、用户可见错误分类。

### 7.2 SDK 架构测试门

每个 PR 至少满足：

- public header self-contained：单独 include 可编译。
- `pi::ai` consumer 不链接 agent/coding。
- `pi::agent_core` consumer 不链接 coding。
- `pi::coding_agent` consumer 不需要 CLI/TUI。
- `apps/picpp` 不 include `src/ai` / `src/agent` / `src/coding` private headers。
- 安装/导出相关改动必须跑 install-tree smoke test。

---

## 8. 非目标（v1.0.0 前明确不做）

- MCP 内置实现；需要时优先通过 extension 适配。
- OAuth 完整流程和 credential 加密存储。
- 内置 container/VM sandbox。
- terminal images、HTML export、telemetry、self-update。
- C++20 coroutine/modules。
- pi `v0.80.0` 中 OpenAI Chat Completions compatible 和 Anthropic Messages 之外的其他 Provider 原生协议，例如 openai-responses / bedrock / vertex / mistral 等。
- 与 pi TS extension system “实现机制完全相同”；只对声明的行为面负责。
- v1.0 前 ABI 稳定承诺；v1.0 重点先冻结 source/API compatibility，ABI 是否承诺需在 v0.3.x 单独评估。

---

## 9. 风险与对策

| 风险 | 等级 | 对策 |
|---|---|---|
| SDK 边界晚拆导致 CLI 与核心耦合 | 高 | v0.0.2 第一优先级迁移 `include/` + 三 target；后续 consumer test 强制守边界 |
| `src/` 被当 public API 继续扩散 | 高 | 禁止 PUBLIC include src；public-header lint/consumer build test |
| 三层依赖形成循环 | 高 | 固定 `pi-ai → pi-agent-core → pi-coding-agent` 单向依赖；CMake target graph 检查 |
| public API 泄漏 cpr/reproc/FTXUI | 高 | 第三方类型默认 private；public abstraction 独立设计 |
| “兼容 pi”范围失控 | 高 | Compatibility Matrix + Planned/Out-of-Scope |
| 只靠源码阅读误判语义 | 高 | v0.0.2 起固定 differential harness |
| Session wire 反复迁移 | 高 | v0.1.0 一次性定义计划内完整 SessionEntry union |
| steering/followUp 丢消息 | 高 | deque + QueueMode + 多消息差分测试 |
| CancellationToken deadlock/UAF | 高 | v0.0.2 锁外 callback + race tests |
| Windows terminal/process 差异 | 高 | 三平台 CI；Job Object；TUI spike；真实走查 |
| Provider 碎片化 | 中 | compat flags；只对实测/声明范围承诺 Compatible |
| FTXUI CJK/IME 不达标 | 中 | go/no-go spike；增强 REPL 退路 |
| Extensions 范围膨胀 | 中 | v0.2.2 冻结最小 protocol v1 |
| 0.x 版本号看起来增长过快 | 低 | 使用 0.1.x/0.2.x 阶段内里程碑；0.3.0 才代表核心功能完成 |
| 单人周期过长 | 中 | 每个 tag 可运行；0.1.0 后始终保持可用；0.3.0 后冻结功能 |

---

## 10. 开发方法论

本仓库是学习型项目：一边研究 pi 架构，一边形成 C++17 的可复用 SDK。每个版本产出 **设计文章、代码、兼容证据、SDK consumer 示例和 tag**。

### 10.1 每版本工作流

1. **基线取证**：先确定 pi `v0.80.0` 对应源码、fixture 和可观察行为。
2. **Public API 识别**：设计阶段先划分哪些类型/API 属于 `pi-ai`、`pi-agent-core`、`pi-coding-agent` public contract，哪些必须 private。
3. **设计先行**：完成 `docs/design/vX.Y.Z.md`，写清机制、候选方案、pi 行为、C++ SDK/API、已知偏差。
4. **TDD 拆解**：兼容任务先写 differential red test；SDK API 先写 consumer compile test。
5. **实现**：小步提交；业务功能只能进入 SDK，CLI/TUI 只做 adapter/rendering/input。
6. **验收**：unit + consumer + FakeProvider + differential + 三平台 CI。
7. **回顾**：记录 API 变化、实现偏差、踩坑、Compatibility Matrix 变化。
8. **收尾 tag**：更新 CHANGELOG、README、Compatibility Matrix、SDK examples/API docs。

### 10.2 设计文档要求

- 自包含，说明当前版本在整体成熟度中的位置。
- 先写 pi `v0.80.0` 可观察行为，再写 Tau 参考和 C++ 设计。
- 必须有“SDK 边界”章节：public headers、target dependencies、生命周期、错误模型。
- 展示真实 C++ public interface / JSON / trace 示例。
- 候选方案与取舍透明。
- checklist、差分样本、API mapping 放附录。

### 10.3 SDK-first 原则

- 新能力先问“外部 C++ 调用者如何用”，再问 CLI 怎么展示。
- `picpp` 能做的核心业务操作，原则上都应有对应 `pi::coding_agent` API。
- `pi-agent-core` 应能脱离 coding tools 独立驱动通用 Agent。
- `pi-ai` 应能脱离 Agent 独立做 Provider streaming。
- CLI/TUI/RPC 都是 adapter，不拥有核心状态机。

### 10.4 版本纪律

- 主干始终可构建；CI 红为最高优先级。
- 每个版本都可发布，上版功能不回退。
- 已发布历史事实不反向改写，发现问题写后续 fix/known deviation。
- `v0.3.0` 起冻结主功能面；`v0.3.x` 只做稳定化；`v1.0.0` 不新增功能。
- `v1.0.0` 后 public SDK/API 按 SemVer 管理。