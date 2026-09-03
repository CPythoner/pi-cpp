# pi-cpp 开发计划（版本化路线图）

> **For agentic workers:** 本文档是主路线图。每个版本启动时，先用 writing-plans 流程把该版本拆解为任务级实施计划（TDD、bite-sized tasks），再进入实现。各版本内的功能清单用 checkbox 跟踪。

**目标：** 用 C++17 实现 pi agent（github.com/earendil-works/pi，TS monorepo）的核心功能——一个可交互的终端 coding agent CLI。行为与协议兼容基线固定为 pi `v0.80.0`；Tau `v0.4.1` 仅作为实现参考。

**架构：** 三层移植，以 pi `v0.80.0` 定义行为语义，以 HuggingFace Tau `v0.4.1`（pi 架构的 Python 参考实现）辅助理解模块划分和实现方式：`agent`（≈tau_agent，消息/事件/主循环）→ `ai`（≈tau_ai，Provider/SSE）→ `coding`（≈tau_coding，工具/会话/配置）+ `cli`/`tui` 前端。单二进制、错误即消息、append-only JSONL 树状会话。

**技术栈：** C++17 · CMake ≥3.25 + FetchContent · cpr(SSE) · nlohmann/json · reproc++ · fmt · doctest · FTXUI(后期) · cxxopts

---

## 1. 已确认的决策

| 决策项 | 选择 |
|---|---|
| 行为兼容基线 | **pi `v0.80.0`**；API、CLI、事件、会话、工具调用及 wire 语义均以该 tag 的可观察行为为准 |
| Tau 参考版本 | **Tau `v0.4.1`**；仅参考模块拆分、算法和工程实现，不作为行为规范来源 |
| 语言标准 | **C++17**（编译器基线放宽到 GCC 9+ / Clang 7+ / VS2019+；取消链用自研 CancellationToken，语义对齐 std::stop_token，未来可平替；格式化统一用 fmt） |
| UI 形态 | v0.1.0 print 模式 → v0.2.0–v0.4.0 行式 REPL + ANSI 流式输出 → v0.5.0 起迭代 FTXUI TUI |
| Provider 策略 | v0.0.2 先打通 OpenAI 兼容协议；v0.4.0 加 Anthropic 原生协议 |
| 平台 | Linux / macOS / Windows 三平台第一天支持（CI matrix） |
| 依赖策略 | 轻依赖：cpr、nlohmann/json、reproc、fmt、cxxopts、doctest、FTXUI，全部 FetchContent + pin tag |
| 二进制名 | `picpp`；配置目录 `~/.picpp/`；C++ 命名空间 `pi` |

### 1.1 兼容基线与冲突裁决

pi `v0.80.0` 与 Tau `v0.4.1` 并非同期基线。本项目采用“**pi 定义行为，Tau 参考实现**”的双参考策略：

1. API、CLI、事件、会话、工具调用和 wire 格式以 pi `v0.80.0` 为规范来源。
2. C++ 类型设计、模块组织、并发模型、存储算法等可以参考 Tau `v0.4.1` 的实现方式。
3. 两者出现差异时，按 **pi `v0.80.0` 可观察行为 > 本项目兼容规范 > Tau `v0.4.1` 实现** 的顺序裁决。
4. Tau `v0.4.1` 中来自更高版本 pi 或其自行扩展的能力，只能进入扩展清单或后续版本，不能作为当前兼容基线的必需能力。
5. 兼容测试必须以 pi `v0.80.0` 的真实样本或差分行为为主；Tau 样本仅用于辅助互操作验证。

特别说明：`packages/orchestrator` 在 pi `v0.80.0` 中尚不存在，后来才作为实验包加入，并在 pi `v0.81.0` 改名为 `packages/server`。即使 Tau `v0.4.1` 含有类似能力，也不属于当前基线的缺失项，应作为后续扩展单独规划。

## 2. 参考蓝本要点（两仓库分析结论）

**pi `v0.80.0`**（TS，~10.6 万行核心）：四包 ai/agent/coding-agent/tui。核心是 `packages/agent/src/agent-loop.ts:155` 的 runLoop 双层循环（外层 followUp 队列、内层工具迭代）+ AgentEvent 事件流 + JSONL 树状会话（id/parentId）。核心无权限系统、无 MCP——审批靠 `beforeToolCall` 钩子，集成靠扩展系统。默认工具就 4 个：read/write/edit/bash。

**Tau `v0.4.1`**（HuggingFace 官方 Python 复刻，核心仅 ~7,400 行）：模块组织与 pi 高度对应、零 SDK 依赖（全部手写 SSE，便于参考 C++ 实现）。其 `dev-notes/` 有 phase-1…28 施工日志，可用于参考增量实施顺序；但它与 pi `v0.80.0` 并非同期基线，任何 wire 兼容结论都必须回到 pi `v0.80.0` 验证。

**关键移植原则（行为由 pi `v0.80.0` 确认，Tau `v0.4.1` 辅助实现）：**
1. 错误不抛异常，编码为 `stopReason: "error"` 的 AssistantMessage（循环韧性的核心）
2. 消息双层模型：会话里存 AgentMessage（含 bash 执行记录等应用消息），仅在调 LLM 边界 convertToLlm 降维
3. 会话是 append-only JSONL **树**（id/parentId），fork/branch/压缩都是追加 entry 而非改写历史
4. 工具默认并行执行，文件写工具经 mutation queue 串行化
5. steering/followUp 双队列，排空点固定在 turn 结束处

## 3. 总体架构

### 3.1 分层与目录

```
pi-cpp/
├── CMakeLists.txt
├── cmake/deps.cmake            # FetchContent，全部 Declare 前置 + pin tag + SYSTEM
├── src/
│   ├── agent/                  # ≈ tau_agent（无 UI 的可复用 agent 运行时）
│   │   ├── message.hpp/.cpp    # 7 角色 AgentMessage + 4 种 ContentBlock + camelCase JSON
│   │   ├── events.hpp/.cpp     # L1 provider 事件 + L2 agent 事件（std::variant）
│   │   ├── agent_loop.cpp      # runLoop 双层循环（对照 tau loop.py:52）
│   │   ├── agent.hpp/.cpp      # Agent 门面：prompt/steer/followUp/abort/subscribe
│   │   ├── tool.hpp            # AgentTool 接口 + AgentToolResult
│   │   ├── tool_history.cpp    # repair_tool_history（悬空 tool_call 配对修复）
│   │   └── event_stream.hpp    # 线程安全事件队列（EventStream<T> 等价物）
│   ├── ai/                     # ≈ tau_ai（Provider 层）
│   │   ├── provider.hpp        # streamResponse() 单接口
│   │   ├── openai_compatible.cpp  # chat completions + SSE + delta 合并
│   │   ├── anthropic.cpp       # v0.4.0：anthropic-messages 原生协议
│   │   ├── stream_canon.cpp    # 事件归一化状态机（对照 tau stream.py:88）
│   │   ├── retry.cpp           # 指数退避（0.25s 起步）
│   │   ├── http.cpp            # cpr 封装：ConnectTimeout+LowSpeed，禁用整体 Timeout
│   │   └── fake.cpp            # FakeProvider（录制/回放，测试基石）
│   ├── coding/                 # ≈ tau_coding（应用层）
│   │   ├── tools/              # read/write/edit/bash（v0.0.4）、grep/find/ls（v0.6.0）
│   │   ├── session/            # jsonl storage + tree + manager
│   │   ├── compaction.cpp      # 上下文估算 + 手动/自动压缩
│   │   ├── system_prompt.cpp   # 工具清单 + guidelines + AGENTS.md + cwd/date
│   │   ├── config.cpp          # ~/.picpp/ 配置 + models.json + 环境变量
│   │   └── catalog.cpp         # provider/模型目录（数据驱动，v0.4.0）
│   ├── cli/
│   │   ├── main.cpp            # cxxopts 解析
│   │   ├── print_mode.cpp      # picpp -p（v0.1.0）
│   │   ├── repl.cpp            # 行式交互（v0.2.0）
│   │   └── rpc_mode.cpp        # stdio JSONL RPC（v0.6.0）
│   ├── tui/                    # v0.5.0：FTXUI 前端
│   └── util/                   # cancel_token.hpp、thread_guard.hpp、string.hpp、ansi.hpp、diff.cpp、truncate.cpp、fs.cpp
├── tests/                      # doctest；fixtures/ 含 pi/tau 会话黄金样本
├── examples/                   # chat_demo 等最小可执行示例
├── docs/
└── .github/workflows/ci.yml    # ubuntu-24.04 / macos-15 / windows-2022
```

### 3.2 并发模型（对应 pi 的单线程事件循环）

- **主线程**：REPL/TUI 事件循环，从 `EventStream` 队列取事件渲染
- **网络线程**：每个活跃 LLM 流一个 worker `std::thread`（ThreadGuard RAII 自动 join），cpr 同步请求 + WriteCallback 把事件推入队列；CancellationToken 贯穿取消链（对应 AbortSignal）
- **工具执行**：默认提交到 worker 线程并行；edit/write 经 file-mutation-queue 串行化
- **取消**：CancellationToken 一路传到 cpr 回调检查退出 + reproc 进程树 kill

### 3.3 技术选型（全部经官方来源核实）

| 领域 | 选型 | 关键注意点 |
|---|---|---|
| HTTP+SSE | cpr（内置构建 curl） | 流式请求只设 ConnectTimeout + LowSpeed{1B/s,30s}，**禁用整体 Timeout**；Windows 默认 Schannel 免 OpenSSL |
| JSON | nlohmann/json | SSE 逐完整事件 parse，无需增量解析器；NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE 生成 camelCase 序列化 |
| 子进程 | reproc++ | Unix: setpgid+kill(-pgid) 杀进程树；Windows: 自补 ~60 行 Job Object；POSIX 全局 `signal(SIGPIPE, SIG_IGN)` |
| TUI | FTXUI ≥7.0.3 | v0.5.0 前先做 1 天 CJK/Windows Terminal spike；7.0 有 API 改名，以 7.x 文档为准 |
| 格式化 | fmt | C++17 无 std::format；fmt 全平台行为一致 |
| 测试 | doctest | 单头、编译快 |
| CLI 解析 | cxxopts | 单头 |
| 构建 | CMake ≥3.25 | FetchContent 全 Declare 前置、pin 到 commit、`SYSTEM` 隔离警告；静态链接出单二进制 |
| CI | GH Actions matrix | ubuntu-24.04 / macos-15(arm64) / windows-2022，显式钉版本；Windows 零 OpenSSL 步骤 |

## 4. 里程碑总览

| 版本 | 主题 | 交付标志 | 规模 |
|---|---|---|---|
| v0.0.1 | 骨架与核心类型 | 三平台 CI 绿；pi 兼容 wire 格式类型 + 单测 | S |
| v0.0.2 | Provider 层与 SSE | OpenAI 兼容端点真实流式对话跑通 | M |
| v0.0.3 | Agent 主循环 | FakeProvider 驱动的多轮工具循环 + steering + 取消 | M |
| v0.0.4 | 工具四件套 | read/write/edit/bash 全部可用，bash 进程树 kill | M |
| v0.1.0 | **print 模式 MVP** | `picpp -p "改这个文件"` 端到端完成，会话落盘 | S |
| v0.2.0 | REPL + 会话持久化 | 三平台交互式对话 + 斜杠命令 + resume | M |
| v0.3.0 | 会话树与压缩 | fork/branch//tree、自动 compaction | M |
| v0.4.0 | 多 Provider | Anthropic 原生协议 + 目录驱动模型切换 | M |
| v0.5.0 | TUI | FTXUI 全屏交互，CJK 正确 | L |
| v0.6.0 | RPC + 扩展生态 | pi 兼容 JSONL RPC、进程外扩展、grep/find/ls | M |
| v1.0.0 | 发布打磨 | 三平台单二进制 Release、skills、韧性 | M |

每个版本都是可发布状态：构建绿、测试绿、上一个版本的功能不回退。

---

## 5. 各版本详细计划

### v0.0.1 — 骨架与核心类型

**目标：** 项目能三平台编译、CI 就位；把 pi 兼容的类型系统（消息/事件）用 C++ 定义清楚并用黄金样本验证序列化。这是全项目地基，质量优先。

**功能清单：**
- [ ] CMakePresets（debug/release）+ 顶层 CMakeLists（C++17、`-Wall -Wextra -Werror`）
- [ ] `cmake/deps.cmake`：nlohmann/json、fmt、doctest FetchContent（Declare 前置、pin commit、SYSTEM）
- [ ] `.github/workflows/ci.yml`：三平台 matrix（configure → build → ctest）
- [ ] `util/` 基础设施（C++17 补齐项，v0.0.2/v0.0.3 取消链的地基）：`cancel_token.hpp`（CancellationToken：atomic_bool + condition_variable，语义对齐 std::stop_token——request/wait/注册取消回调）+ `thread_guard.hpp`（RAII 自动 join，替代 jthread）+ `string.hpp`（startsWith/endsWith/contains）；单测覆盖取消唤醒、超时等待、析构安全
- [ ] `agent/message.hpp/.cpp`：
  - ContentBlock = `variant<TextContent, ThinkingContent, ImageContent, ToolCall>`
  - AgentMessage = `variant<UserMessage, AssistantMessage, ToolResultMessage, BashExecutionMessage, CustomMessage, BranchSummaryMessage, CompactionSummaryMessage>`
  - AssistantMessage 溯源元数据：api/provider/model/usage(input,output,cost)/timing/stopReason
  - StopReason 枚举：`stop | length | toolUse | error | aborted`
  - camelCase JSON 序列化（NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE，别名与 tau `messages.py` 的 wire 别名逐一对照）
- [ ] `agent/events.hpp/.cpp`：L1（AssistantStart/Done/Error + Text/Thinking/ToolCall 各 Start/Delta/End）+ L2（agent_start/end、turn_start/end、message_start/update/end、tool_execution_start/update/end）两套 variant
- [ ] `tests/fixtures/`：以 pi `v0.80.0` 会话 JSONL 作为主黄金样本，Tau `v0.4.1` 样本仅作辅助互操作验证；执行 round-trip 测试（parse→dump→byte 级或语义级一致）

**验收标准：** 三平台 CI 全绿；消息/事件 round-trip 单测通过（含 pi 真实会话样本）；`picpp` 空壳 `--version` 可运行。

**参考蓝本：** `tau/src/tau_agent/messages.py`、`tau/src/tau_agent/events.py`、`tau/src/tau_agent/provider_events.py`（字段与别名逐一对抄）；`pi/packages/agent/src/types.ts:305-428`（契约语义）。

**风险与对策：** C++ variant 嵌套较深 → 为 AgentMessage 提供 `visit()` 辅助与类型别名，避免库代码里出现裸 variant 访问。

---

### v0.0.2 — Provider 层与 SSE

**目标：** 打通 OpenAI 兼容协议的流式对话（DeepSeek/GLM/Kimi/OpenAI 等可直接用），建立可回放的测试范式。

**功能清单：**
- [ ] `ai/provider.hpp`：`streamResponse(model, system, messages, tools, stopToken, sessionId)` 单接口，产出 L1 事件流
- [ ] `agent/event_stream.hpp`：线程安全事件队列（push/close/wait），网络线程生产、消费侧拉取
- [ ] `ai/http.cpp`：cpr 封装——流式请求 ConnectTimeout + LowSpeed，明确不设整体 Timeout；Bearer 鉴权
- [ ] `ai/openai_compatible.cpp`：
  - 请求构造（messages/tools 的 function 定义、stream:true、stream_options include_usage）
  - cpr `ServerSentEventCallback` 逐事件 → `json::parse(event.data)`
  - **delta 合并状态机**：text 增量拼接、tool_calls 按 index 聚合（id/name/arguments 部分字符串累积）、finish_reason → StopReason 映射、usage 捕获
- [ ] `ai/stream_canon.cpp`：事件归一化（通道切换时强制 End/Start 配对，对照 tau stream.py:88 的 212 行状态机）
- [ ] `ai/retry.cpp`：指数退避（0.25s 起、上限、jitter），可重试判定（429/5xx/网络错），错误编码为 `stopReason:error` 的 AssistantMessage——不抛异常
- [ ] `ai/fake.cpp`：FakeProvider——事件脚本回放（测试基石）
- [ ] `examples/chat_demo.cpp`：最小 CLI，API key 走环境变量，真实端点流式打印 token

**验收标准：** `chat_demo` 对 DeepSeek 或 GLM 端点逐 token 流式输出完整回答；断网/401/429 场景错误成为消息而非崩溃；SSE 解析单测覆盖半行跨 chunk、多 data 行拼接、畸形行兜底。

**参考蓝本：** `tau/src/tau_ai/openai_compatible.py:72`（双解析器中的 chat completions 部分）、`tau/src/tau_ai/stream.py:88`、`tau/src/tau_ai/retry.py`、`tau/src/tau_ai/fake.py:11`；pi `packages/ai/src/api/openai-completions.ts`。

**风险与对策：** 各家 OpenAI 兼容服务器差异（reasoning_content 字段、developer role）→ 预留 compat 开关（supportsDeveloperRole 等），首版只保证 DeepSeek/GLM/OpenAI 三家实测。

---

### v0.0.3 — Agent 主循环

**目标：** 移植系统心脏 runLoop。此版本工具尚未实现，用测试桩工具验证循环正确性。

**功能清单：**
- [ ] `agent/tool.hpp`：AgentTool（name/description/JSON Schema/execute(toolCallId, args, stopToken, onUpdate)）+ AgentToolResult（content/details/terminate/addedToolNames）
- [ ] `agent/agent_loop.cpp`：双层 while——外层 followUp 队列检查、内层「steering 注入 → LLM 流式 → 提取 toolCall → 执行工具批 → turn_end → prepareNextTurn 钩子 → 再取 steering」
- [ ] `agent/agent.hpp/.cpp`：Agent 门面（prompt/steer/followUp/abort/subscribe/waitForIdle），PendingMessageQueue（steering/followUp 各一条）
- [ ] 取消链：CancellationToken（v0.0.1 已交付）从 Agent.abort() 贯穿到 provider 流退出与工具返回；aborted 收尾不丢已产出内容
- [ ] 计时埋点：time_to_first_output / total_duration 写入 AssistantMessage.timing
- [ ] `agent/tool_history.cpp`：repair_tool_history——发请求前修复悬空 tool_call 配对（各 provider 对历史容忍度不同，刚需）
- [ ] 错误即消息全链路验证：provider error → 循环优雅收尾 → 下轮可继续

**验收标准：** FakeProvider 脚本化集成测试全部通过——①多轮工具调用收敛 ②工作中 steer 插话被注入 ③followUp 触发续跑 ④任意时刻 abort 无死锁无泄漏 ⑤错误后再次 prompt 可恢复。

**参考蓝本：** `tau/src/tau_agent/loop.py:52`（376 行，逐段对照）；`pi/packages/agent/src/agent-loop.ts:155-268`（runLoop 语义）；`tau/src/tau_agent/harness.py:62`；`tau/src/tau_agent/tool_history.py`。

---

### v0.0.4 — 工具四件套

**目标：** 实现 read/write/edit/bash，达到「能真正改代码」的能力。

**功能清单：**
- [ ] `coding/tools/read.cpp`：行范围参数、行号标注、截断（默认 50KB/2000 行上限，头尾保留策略）
- [ ] `coding/tools/write.cpp`：整文件写入，父目录创建
- [ ] `coding/tools/edit.cpp`：精确字符串替换（oldString 唯一匹配校验、行尾风格保持）、失败时返回带上下文的错误（供模型自纠错）；`util/diff.cpp` 实现 ~100 行 LCS 行级 unified diff 用于结果展示
- [ ] `coding/tools/bash.cpp`：
  - reproc++：Unix `/bin/sh -c`、Windows `cmd.exe /c`
  - 超时（默认 120s，参数可调）；CancellationToken 触发 kill
  - 进程树清理：POSIX setpgid + kill(-pgid)；Windows 自补 Job Object（~60 行）
  - stdout/stderr 合并捕获 + 头尾截断；退出码与超时信息进入结果
  - POSIX 全局 `signal(SIGPIPE, SIG_IGN)`
- [ ] file mutation queue：edit/write 串行化（防并发写冲突），其余工具默认并行
- [ ] `beforeToolCall`（可 block+reason，reason 作为工具错误喂回模型）/ `afterToolCall`（可改写结果/终止）钩子
- [ ] 工具参数校验：JSON Schema 必填项检查，参数错误返回结构化错误而非崩溃

**验收标准：** 每工具单测（含边界：空文件、二进制嗅探、超长行、编码）；集成测试——FakeProvider 驱动「读取→编辑→bash 验证」完整链路于临时目录；bash 超时后无残留进程（ps 验证进程树全灭）。

**参考蓝本：** `tau/src/tau_coding/tools.py:363/454/537/700`（四件套逐一对照，含 TruncationResult 语义与进程树 kill）；`pi/packages/coding-agent/src/core/tools/bash.ts`、`edit.ts`；`pi/packages/coding-agent/src/core/tools/file-mutation-queue.ts`。

---

### v0.1.0 — print 模式 MVP（第一个可用版本 🎉）

**目标：** 无交互 UI 的端到端闭环。这是第一个给人用的版本，对齐 `pi -p`。

**功能清单：**
- [ ] `coding/system_prompt.cpp`：工具清单+使用 guidelines + AGENTS.md 发现注入（向上查找至 git 根）+ cwd/日期
- [ ] `coding/config.cpp`：`~/.picpp/settings.json`（默认模型/重试策略）+ `~/.picpp/models.json`（自定义 OpenAI 兼容端点：baseUrl+model+apiKeyEnv）+ 环境变量 key 解析
- [ ] `coding/session/storage.cpp`（最小版）：JSONL append——SessionHeader（id/cwd/时间戳）+ message entry；`--session-dir` 可重定向
- [ ] `cli/main.cpp` + `cli/print_mode.cpp`：`picpp -p "prompt"` 单发执行；`--model` 覆盖；`--mode json` 输出 L2 事件流（JSON lines，供脚本消费）；退出码语义（error/aborted 非零）
- [ ] README 快速开始（真实端点配置示例）

**验收标准：** 在临时项目中 `picpp -p "把 main.py 里的 foo 改成 bar 并运行测试"` 端到端成功（真实 API key）；会话 JSONL 与 tau 字段结构对照通过；`--mode json` 事件流可被 python 一行脚本解析。

**参考蓝本：** `pi/packages/coding-agent/src/modes/print-mode.ts`（159 行，证明无 UI 路径极薄）；`tau/src/tau_coding/system_prompt.py`；`tau/src/tau_agent/session/storage.py:38`。

---

### v0.2.0 — REPL 交互 + 会话持久化

**目标：** 日常可用的交互式行模式；会话可恢复。Windows 终端全兼容。

**功能清单：**
- [ ] `cli/repl.cpp`：
  - 行式输入 + ANSI 流式输出（token 即时打印、Markdown 代码块着色、工具执行块渲染：调用参数摘要+结果摘要）
  - spinner/忙碌指示；Ctrl+C 中止当前轮（双击退出）
  - 斜杠命令：`/help` `/model` `/clear` `/new` `/save` `/exit`
- [ ] Windows 终端初始化：`ENABLE_VIRTUAL_TERMINAL_PROCESSING` + `SetConsoleOutputCP(CP_UTF8)`，管道重定向时自动降级去色
- [ ] `coding/session/tree.cpp`：entry 的 id/parentId 链构建与查询（path_to_entry）
- [ ] 会话恢复：`picpp --continue`（最近）/ `--resume <id>`，重放历史重建上下文
- [ ] REPL 主循环 = EventStream 消费循环；网络/工具事件经队列投递

**验收标准：** macOS Terminal、Linux、Windows Terminal 三处人工走查（含中文输入输出）；中断→resume 上下文完整；Ctrl+C 中止后 REPL 不死锁不脏屏。

**参考蓝本：** `tau/src/tau_coding/cli.py:103`（print 模式过渡形态）；`pi/packages/coding-agent/src/config.ts:515`（目录布局）；`tau/src/tau_agent/session/tree.py:22`。

---

### v0.3.0 — 会话树与上下文压缩

**目标：** 长会话生存能力——分支探索与自动压缩。

**功能清单：**
- [ ] `/tree` 树状导航 + `/fork <entryId>` 从任意历史点分叉（新分支写 append，不改写历史）
- [ ] BranchSummaryMessage：fork 时生成分支摘要 entry
- [ ] `coding/compaction.cpp`：
  - 字符≈token 启发式估算（对照 tau context_window.py，无需 tokenizer）
  - 手动 `/compact` + 自动触发（contextTokens > window − reserve，默认 reserve 16384）
  - 压缩= 追加 CompactionSummaryMessage entry + convertToLlm 边界裁剪，历史文件不动
- [ ] `/cost` `/context` 状态查看（累计成本、当前上下文占用）

**验收标准：** 集成测试：灌入超长对话自动触发 compaction 且循环继续正常工作；fork 两条分支各自独立演进；会话文件仍是 append-only（校验旧 entry 字节不变）。

**参考蓝本：** `pi/packages/coding-agent/src/core/session-manager.ts`（树状 entry 类型定义 :45-80）；`pi/packages/coding-agent/src/core/compaction/`；`tau/src/tau_coding/context_window.py`。

---

### v0.4.0 — 多 Provider（Anthropic 原生 + 目录驱动）

**目标：** 补齐第二线协议；模型/端点全面数据驱动化，运行中可切换。

**功能清单：**
- [ ] `ai/anthropic.cpp`：anthropic-messages SSE（message_start / content_block_delta / message_delta）、thinking 块 + thinkingSignature 回放、stop_reason 映射、`anthropic-version` 头
- [ ] thinking level 归一化：OpenAI reasoning_effort ↔ Anthropic thinking budget 统一为内部 thinking level 概念
- [ ] `coding/catalog.cpp`：provider/模型目录数据驱动——打包 tau 的 `catalog.toml` 精简版为内嵌资源 + `~/.picpp/catalog.toml` 用户覆盖；模型元数据（窗口/价格/maxOutput）
- [ ] `/model` 交互式模型选择器（目录过滤+自定义合并）；运行中切模型产生 model_change entry，下轮生效
- [ ] compat 开关完善（reasoning 字段差异、developer/system role 差异）按端点配置

**验收标准：** Claude（原生协议）与 DeepSeek/GLM（OpenAI 兼容）双通道真实对话通过；同一会话中途切模型历史无失配；新增一个 OpenAI 兼容小厂商只需改 TOML 不改代码。

**参考蓝本：** `tau/src/tau_ai/anthropic.py:73`（771 行含 SSE 手解析 :224，可直接对照翻译）；`tau/src/tau_coding/data/catalog.toml`（28 provider 元数据，直接复用）；`tau/src/tau_coding/provider_config.py`。

---

### v0.5.0 — TUI（FTXUI）

**目标：** 从行模式升级为全屏组件化交互。先 spike 验证再全量投入。

**功能清单：**
- [ ] **前置 spike（1 天，go/no-go 门）**：FTXUI 在 Windows Terminal/conhost/WezTerm 下的 CJK 全宽渲染、中文 IME 输入回显、`App::Post` 跨线程投递。不达标则本版本降级为「增强版 REPL + raw mode 行编辑」，TUI 换 notcurses 重新评估
- [ ] `tui/` 框架：App 主循环、工作线程经 Post 投递事件、差分渲染
- [ ] 消息流区：frame 滚动 + 自动跟随、流式增量渲染、Markdown 基础渲染、工具调用折叠块
- [ ] 输入区：多行编辑、历史（上/下键）、粘贴
- [ ] 状态栏：模型/上下文占用/累计成本/会话分支
- [ ] 审批 UI：beforeToolCall 触发的确认弹窗（y/n/a(always)）——首个真正的权限门
- [ ] 主题基础（暗色默认）+ `~/.picpp/themes/` 自定义
- [ ] REPL 模式保留为 `--no-tui` 退路（也是管道/CI 场景所需）

**验收标准：** 完整交互会话全部功能在 TUI 下可用；CJK 对齐无错位（含 emoji 边界）；长会话（200+ 消息）滚动不卡顿；`--no-tui` 行为与 v0.4.0 一致。

**参考蓝本：** `pi/packages/tui/src/tui.ts`（差分渲染与同步输出的行为基准——不必复刻实现，FTXUI 已内置）；交互形态对照 `pi/packages/coding-agent/src/modes/interactive/`。

---

### v0.6.0 — RPC 模式 + 扩展生态

**目标：** headless 集成口与可扩展性——C++ 不能照搬 jiti 动态 TS，改为进程外扩展。

**功能清单：**
- [ ] `cli/rpc_mode.cpp`：`picpp --mode rpc`，stdin/stdout JSONL 协议（pi 兼容命令集：prompt/steer/abort/fork/compact/status/exit…），事件以通知形式回流
- [ ] 扩展机制 v1：`~/.picpp/extensions/*.json` 声明式注册子进程扩展——子进程同样说 JSONL RPC，可 register_tool / register_command / 挂 tool_call 拦截钩子（block+reason）；附 `examples/extensions/permission-gate`（拦截 `rm -rf`、`git push --force`）与 `examples/extensions/subagent`（spawn 隔离 picpp 进程）两个官方示例
- [ ] 新增只读工具：`grep`（正则内容搜索）、`find`（文件名 glob）、`ls`；`util/ignore.cpp` 实现 gitignore 子集过滤（跳过 .git/node_modules 等）
- [ ] ExtensionAPI 事件面文档化（对照 pi extensions/types.ts 的 ~30 个事件，标注 v1 支持子集）

**验收标准：** 测试 harness 经 RPC 全流程驱动（prompt→工具→fork→exit）；示例扩展真的拦住危险命令并把 reason 喂回模型；grep/find/ls 尊重 gitignore 且大目录下性能可接受（流式+截断）。

**参考蓝本：** `pi/packages/coding-agent/src/modes/rpc/rpc-mode.ts:774` + `rpc-types.ts:21-69`（命令集直接对齐）；`pi/examples/extensions/permission-gate.ts`、`subagent/`；`pi/packages/coding-agent/src/core/tools/{grep,find,ls}.ts`。

---

### v1.0.0 — 发布打磨

**目标：** 生产可用与分发。

**功能清单：**
- [ ] Skills：`.picpp/skills/*.md`（frontmatter name/description）发现 + `/skill` 展开、`<skill>` 块注入
- [ ] Prompt 模板：`.picpp/prompts/*.md` → 斜杠命令
- [ ] 韧性：JSONL 崩溃安全（半行 entry 容忍与修复）、重试策略可配置、低内存会话加载（流式读取大 JSONL）
- [ ] 性能：冷启动 <50ms、空闲内存 <30MB 基线并记录基准
- [ ] 分发：GitHub Releases 三平台单静态二进制 + 安装脚本；README/文档站（架构、扩展协议、FAQ）
- [ ] 端到端冒烟矩阵：三平台 × 两大协议 × print/REPL/TUI/RPC 四模式

**验收标准：** tag v1.0.0 的 Release 资产在三个平台开箱可用；冒烟矩阵全绿；文档覆盖全部用户可见功能。

**参考蓝本：** `pi/packages/coding-agent/src/core/skills.ts`、`prompt-templates.ts`、`slash-commands.ts`。

---

## 6. 测试策略

1. **单元测试**（doctest）：类型序列化、SSE 解析状态机、delta 合并、diff、截断、树查询——纯逻辑全覆盖
2. **黄金样本**：pi `v0.80.0` 真实会话 JSONL 是主 fixtures 和兼容门；Tau `v0.4.1` 样本只做辅助互操作回归
3. **FakeProvider 集成测试**：事件脚本驱动主循环/工具/取消/steering 的确定性回放（对照 tau 46,908 行测试的思路，C++ 侧按比例精简但保住关键路径）
4. **真实端点冒烟**：本地手动/可选 nightly（secrets 走环境变量），不在 PR CI 强制
5. **每版本回归门**：CI 三平台 matrix 全绿才可合入

## 7. 非目标（明确不做）

- MCP 支持（pi/tau 均无内置；如需可在 v1.0.0 后以扩展实现）
- OAuth 流程与 credential 加密（API key 环境变量/配置文件足够）
- 内置沙箱/容器隔离（哲学与 pi 一致：信任 + 外部隔离；审批钩子已提供挂点）
- 终端图片（Kitty/iTerm2）、HTML 导出、遥测、自更新、Bun 式分发
- 协程/modules（C++20 特性，本项目不使用；未来若升级标准需重新评估）
- openai-responses/bedrock/vertex/mistral 等其余线协议（目录机制就位后按需加）

## 8. 风险与对策

| 风险 | 等级 | 对策 |
|---|---|---|
| Windows 差异（终端/编码/进程模型） | 高 | v0.0.1 起 CI 即含 Windows；v0.0.4 Job Object、v0.2.0 VT+UTF8 专项；每个 UI 版本三平台人工走查 |
| FTXUI CJK/IME 不达标 | 中 | v0.5.0 前置 1 天 spike 作为 go/no-go 门；退路是增强 REPL + notcurses 重评 |
| OpenAI 兼容服务器行为碎片化 | 中 | compat 开关 + 每版本实测 2-3 家；错误信息结构化便于上报 |
| cpr 流式被超时掐断 | 低 | 设计期已规避（只设 LowSpeed），写入代码评审 checklist |
| 手写 CancellationToken 的竞态/漏唤醒（无 std::stop_token） | 中 | v0.0.1 作为一等公民交付并单测覆盖；接口语义对齐 stop_token，未来升 C++20 可平替 |
| variant 深嵌套导致代码可读性差 | 中 | v0.0.1 即建立 visit 辅助层，禁止裸 variant 访问进库代码 |
| 单人开发周期拉长 | 中 | 每版本独立可用可发布；v0.1.0 后任意时点停下都是能用的产品 |

## 9. 开发方法论（学习驱动 · 文章级设计文档 · 版本 tag）

本仓库是**学习型项目**：一边学习 pi 架构一边用 C++17 实现。每个版本固定节奏，产出三类资产：**设计文章、代码、tag**。

### 9.1 每版本标准工作流

1. **设计先行**：动代码前完成 `docs/design/vX.Y.Z.md`。文档必须写到「照着能写代码」的深度，同时按**可直接发表的文章**标准撰写（自包含、有叙事、讲清原理——模板见 `docs/design/TEMPLATE.md`）。
2. **任务拆解**：基于设计文档用 writing-plans 拆成 TDD 任务级计划（每任务：失败测试 → 最小实现 → 通过 → 提交）。
3. **实现**：小步提交，约定式 commit（feat/fix/test/docs）；关键路径写教学性注释（对照 pi/tau 蓝本行号）。
4. **验收**：跑完设计文档全部验收标准；三平台 CI 绿。
5. **回顾**：在设计文档末尾附录追加「实现回顾」（实际与设计的偏差、踩坑记录、遗留问题→下版本）。
6. **收尾打 tag**：更新 `CHANGELOG.md` 与 README 进度表，打 annotated tag：

   ```bash
   git tag -a vX.Y.Z -m "<主题一句话>。主要交付：<2-3 项>"
   ```

   **tag / 设计文档 / CHANGELOG 条目三者一一对应。**

### 9.2 设计文档 = 可发表文章

- **自包含**：读者无上下文也能读懂——每篇开头用 ~150 字回顾项目是什么、本篇在路线图中的位置。
- **讲原理**：先讲机制（SSE 分帧、JSONL 会话树、进程组杀灭……引用 pi 源码行号与 pi-architecture.md 章节），再讲设计——「为什么」优先于「做什么」。
- **展示真实代码**：C++ 类型/接口定义、wire 格式 JSON 示例直接进文。
- **决策透明**：每个权衡列出候选方案与取舍理由。
- **正文/附录分离**：验收 checklist、任务拆解、蓝本对照表放附录，发布时删除即为成品文章。
- 系列标题格式：《用 C++ 重写 pi coding agent（N）：vX.Y.Z 主题》。

### 9.3 学习导向原则

- 遇到不熟悉的机制，先在设计文档「原理讲解」章节写明白再动手实现。
- 对照实现纪律：改行为必须有理由，默认与 pi `v0.80.0` 的可观察语义一致；Tau `v0.4.1` 只用于参考实现方式（wire 兼容是硬约束）。
- 每版本沉淀「学习要点」：本版本用到的 C++17 技术点（原理+坑）与 pi 架构原理讲解。

### 9.4 版本纪律

- 主干开发 + annotated tag 发布；CI 红即最高优先级。
- 每个版本都是可发布状态：构建绿、测试绿、上版功能不回退。
