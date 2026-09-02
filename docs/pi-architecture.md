# pi 架构参考文档（pi-cpp 移植指南）

> **用途**：本文档是 [pi](https://github.com/earendil-works/pi)（earendil-works/pi-mono，v0.80.0，MIT，作者 Mario Zechner）的架构参考，作为 pi-cpp 移植的规格来源。
> **数据来源**：对 `/Users/wangqiang/workspace/github_repos/pi` 的直接代码分析（行号以该工作区为准）。
> **配套文档**：[dev-plan.md](./dev-plan.md)（版本化开发计划）；[tau](https://github.com/huggingface/tau) 是 pi 架构的官方 Python 复刻（wire 格式兼容），可作为第二参考实现。
> **规模参考**：pi 核心源码（不含测试/示例）约 355 文件 / 105,700 行——ai 34,710 · agent 8,063 · coding-agent 50,786 · tui 12,112。

---

## 目录

1. [项目定位与设计哲学](#1-项目定位与设计哲学)
2. [分层架构总览](#2-分层架构总览)
3. [packages/ai — LLM 统一 API 层](#3-packagesai--llm-统一-api-层)
4. [packages/agent — 通用 Agent 运行时](#4-packagesagent--通用-agent-运行时)
5. [packages/coding-agent — 应用层](#5-packagescoding-agent--应用层)
6. [packages/tui — 终端 UI 库](#6-packagestui--终端-ui-库)
7. [核心数据流走查](#7-核心数据流走查一次-prompt-的完整生命周期)
8. [线协议规格（C++ 移植用）](#8-线协议规格c-移植用)
9. [错误、重试与取消模型](#9-错误重试与取消模型)
10. [与 pi-cpp 的映射关系](#10-与-pi-cpp-的映射关系)

---

## 1. 项目定位与设计哲学

pi 是一个 "agent harness"（智能体运行框架）+ 自我扩展的 coding agent CLI。README 明确的核心决策：

1. **核心刻意留白**：无内置权限/沙箱系统（隔离靠容器化或扩展，如 Gondolin micro-VM / Docker）；无 MCP 支持（全仓库无 mcp 引用）。审批 = `beforeToolCall` 钩子 + 扩展 `tool_call` 事件；集成 = 自研扩展系统 + RPC 模式。
2. **错误即消息**：StreamFn 契约（`packages/agent/src/types.ts:17-26`）要求 LLM 调用失败**不得抛异常**，必须编码为 `stopReason: "error"|"aborted"` 的 AssistantMessage 在流内流动。整个循环因此对故障免疫。
3. **消息双层模型**：会话日志存 AgentMessage（含 bash 执行记录、扩展自定义消息等应用消息），仅在调 LLM 边界经 `convertToLlm` 降维为纯 user/assistant/toolResult。日志完整、上下文干净。
4. **会话是 append-only JSONL 树**：每条 entry 带 `id/parentId`，fork/branch/压缩都是追加 entry 而非改写历史。
5. **异步模型 = 单线程事件驱动**：无 worker 线程（除图片缩放），LLM 流、工具执行、UI 渲染全在一个 Node 事件循环上并发。AbortSignal 是贯穿一切的一等公民。
6. **工具默认并行**，文件写工具经 mutation queue 串行化。
7. **扩展系统是灵魂**：约 80 个官方示例扩展；`ExtensionAPI` 的 ~28 个事件面是稳定契约。

---

## 2. 分层架构总览

```
┌──────────────────────────────────────────────────────┐
│ packages/tui (pi-tui, ~12,112 行)                     │
│ 差分渲染引擎 · 组件 · 键位 · Kitty 图片                 │
└──────────────────────┬───────────────────────────────┘
                       │ 被引用
┌──────────────────────▼───────────────────────────────┐
│ packages/coding-agent (pi 命令, ~50,786 行)            │
│ AgentSession(三模式共享编排) · 工具 · 扩展 · 会话管理    │
│ 模式: interactive(TUI) / print / rpc                   │
└──────────────────────┬───────────────────────────────┘
                       │ 组装（直接 new Agent，非 harness）
┌──────────────────────▼───────────────────────────────┐
│ packages/agent (pi-agent-core, ~8,063 行)              │
│ agent-loop · Agent 门面 · AgentTool 抽象 · harness     │
└──────────────────────┬───────────────────────────────┘
                       │ StreamFn 单接口调用
┌──────────────────────▼───────────────────────────────┐
│ packages/ai (pi-ai, ~34,710 行)                        │
│ 17 个线协议适配器 · 38 个提供商注册 · 模型目录 · OAuth   │
└──────────────────────────────────────────────────────┘
```

| 包 | npm 名 | 职责 |
|---|---|---|
| ai | `@earendil-works/pi-ai` | 统一多提供商 LLM 流式 API；模型目录生成 |
| agent | `@earendil-works/pi-agent-core` | 通用 agent 运行时：主循环、工具调用、状态、会话存储抽象 |
| coding-agent | `@earendil-works/pi-coding-agent` | 交互式 coding agent CLI（`pi` 命令） |
| tui | `@earendil-works/pi-tui` | 差分渲染终端 UI 库（独立可复用） |

**ai 包内部两个正交维度**：
- **协议层**（`src/api/`，17 个文件）：按 *wire protocol* 划分。内置 9 种 Api：`openai-completions`、`openai-responses`、`azure-openai-responses`、`openai-codex-responses`、`mistral-conversations`、`anthropic-messages`、`bedrock-converse-stream`、`google-generative-ai`、`google-vertex`（可注册自定义字符串）。
- **厂商层**（`src/providers/`，38 个注册）：只声明 baseUrl、鉴权、模型清单，挂到某个协议。**新增 OpenAI 兼容厂商只需注册配置，零协议代码**。

---

## 3. packages/ai — LLM 统一 API 层

### 3.1 类型系统（`src/types.ts`，697 行）

**内容块（wire 级）**：

```ts
TextContent     { type:"text"; text:string; textSignature?:string }
ThinkingContent { type:"thinking"; thinking:string; thinkingSignature?:string; redacted?:boolean }
ImageContent    { type:"image"; data:string /*base64*/; mimeType:string }
ToolCall        { type:"toolCall"; id:string; name:string;
                  arguments:Record<string,any>; thoughtSignature?:string /*Google 思路签名*/ }
```

**消息三角色**（`Message`，types.ts:402）：

```ts
UserMessage      { role:"user"; content:string | (Text|Image)[]; timestamp:number }
AssistantMessage { role:"assistant"; content:(Text|Thinking|ToolCall)[];
                   api:Api; provider:ProviderId; model:string;
                   responseModel?; responseId?;
                   diagnostics?:AssistantMessageDiagnostic[];
                   usage:Usage; stopReason:StopReason; errorMessage?; timestamp }
ToolResultMessage{ role:"toolResult"; toolCallId; toolName;
                   content:(Text|Image)[]; details?; isError:boolean; timestamp }
```

```ts
Usage { input; output; cacheRead; cacheWrite; cacheWrite1h? /*Anthropic 1h 保留*/; totalTokens;
        cost:{ input; output; cacheRead; cacheWrite; total } }   // $ 计价
StopReason = "stop" | "length" | "toolUse" | "error" | "aborted"
```

`AssistantMessageDiagnostic`（`utils/diagnostics.ts:8-13`）：`{ type; timestamp; error?{name,message,stack?,code?}; details? }`——transport 降级等非致命诊断挂在最终消息上。

**Model 元数据**（types.ts:660-690，真实样例 `providers/anthropic.models.ts:298-316`）：

```ts
{ id:"claude-opus-4-6", name:"Claude Opus 4.6",
  api:"anthropic-messages", provider:"anthropic", baseUrl:"https://api.anthropic.com",
  compat:{"forceAdaptiveThinking":true}, reasoning:true, thinkingLevelMap:{"xhigh":"max"},
  input:["text","image"],
  cost:{input:5, output:25, cacheRead:0.5, cacheWrite:6.25},   // $/百万 token
  contextWindow:1000000, maxTokens:128000 }
```

模型目录 `models.generated.ts` 由脚本从 models.dev/OpenRouter 等生成（`scripts/generate-models.ts`，2345 行），**严禁手改**。ThinkingLevel 枚举：`"off"|"minimal"|"low"|"medium"|"high"|"xhigh"`。

### 3.2 流事件协议 AssistantMessageEvent（types.ts:447-459）

```
start / text_start|delta|end / thinking_start|delta|end / toolcall_start|delta|end / done / error
```
每个 delta 事件携带 `contentIndex` 与**全量 partial AssistantMessage**；`done`/`error` 既是最后一个迭代事件、也 resolve `result()`。规范：start 先于一切 partial 更新；终止必须是 done 或 error 之一。

### 3.3 EventStream<T,R>（`src/utils/event-stream.ts`，88 行）

pi 全系统的流原语，手写 push 型异步队列：

```ts
class EventStream<T,R> implements AsyncIterable<T> {
  queue: T[]                        // 生产快于消费时缓冲
  waiting: ((r:IteratorResult<T>)=>void)[]  // 消费快于生产时的等待者
  done: boolean
  finalResultPromise: Promise<R>    // 构造时创建，只 resolve 一次
  push(event)   // isComplete 命中 → done=true + resolveFinalResult；有等待者直接交付，否则入队
  end(result?)  // 唤醒所有等待者 {done:true}
  result()      // 取终值，与迭代互不干扰
}
```
**C++ 对应物**：线程安全事件队列 + finalResult future 三件套；"完成事件也作为迭代值投递"、"done 后 push 静默丢弃"是必须保持的语义。

### 3.4 streamSimple 分派（compat.ts + models.ts）

三层结构：
1. **每个 api 模块**导出 `stream`（provider 专属选项）与 `streamSimple`（SimpleStreamOptions；内部把 thinkingLevel clamp 成该 API 字段，如 openai 的 `reasoning_effort`、anthropic 的 effort/budget 分流）。
2. **compat.ts 全局分派**（旧 API 面）：`apiProviderRegistry: Map<Api,...>` + 9 个 BUILTIN_APIS 懒加载包装（首调 dynamic import）。`streamSimple(model, context, options)`：内建模型走 Models 实例（含 provider/auth 解析），否则按 `model.api` 查注册表；显式 apiKey 优先、否则注入环境变量 key。
3. **新 API**（models.ts）：`ModelsImpl.streamSimple` = `lazyStream(model, async () => requireProvider → applyAuth → provider.streamSimple)`。`lazyStream` 同步返回外层流、后台跑 setup，失败 push error 事件——**保持"stream 函数不 throw"契约**。

**SimpleStreamOptions 关键字段**：`temperature/maxTokens/signal/apiKey/transport("sse"|"websocket"|"auto")/cacheRetention/sessionId/onPayload(可替换请求体)/onResponse/headers( null 值可抑制默认头)/timeoutMs/maxRetries(默认 0)/maxRetryDelayMs(默认 60000)/metadata/env` + `reasoning/thinkingBudgets(默认 minimal:1024/low:2048/medium:8192/high:16384)`。

**thinking 换算（移植必读）**：`clampThinkingLevel`（不支持则先向上再向下找最近级）；`adjustMaxTokensForThinking`：`maxTokens = min(base+budget, modelMax)`，若 `maxTokens<=budget` 则 `budget=max(0, maxTokens-1024)`（保底 1024 输出）；xhigh clamp 为 high。

### 3.5 usage 与成本（models.ts:376-386）

`calculateCost(model, usage)` **原地写入** usage.cost：

```ts
cost.input      = model.cost.input/1e6  * usage.input
cost.output     = model.cost.output/1e6 * usage.output
cost.cacheRead  = model.cost.cacheRead/1e6 * usage.cacheRead
cost.cacheWrite = (model.cost.cacheWrite*(cacheWrite-cacheWrite1h) + model.cost.input*2*cacheWrite1h)/1e6
                 // Anthropic 1h 写收 2x 输入价
cost.total      = 四项和
```

---

## 4. packages/agent — 通用 Agent 运行时

### 4.1 核心契约（`src/types.ts`，428 行）

**AgentLoopConfig 完整钩子表**（types.ts:140-282，继承 SimpleStreamOptions）：

| 钩子/字段 | 签名 | 语义 |
|---|---|---|
| `model` | `Model<any>` | 必填 |
| `convertToLlm` | `(AgentMessage[]) => Message[]` | LLM 调用边界降维；不可转换的应过滤；不得 throw |
| `transformContext` | `(messages, signal) => AgentMessage[]` | convertToLlm **之前**的裁剪/注入（compaction 挂点） |
| `getApiKey` | `(provider) => string` | 每次调用动态解析（短命 OAuth） |
| `shouldStopAfterTurn` | `(ctx) => boolean` | turn_end 后、轮询 steering 前调用 |
| `prepareNextTurn` | `(ctx) => {context?, model?, thinkingLevel?} \| undefined` | turn_end 后、下一请求前；返回即替换 |
| `getSteeringMessages` | `() => AgentMessage[]` | 工具执行完后轮询；消息在下次 LLM 调用前注入 |
| `getFollowUpMessages` | `() => AgentMessage[]` | agent 本要停止时轮询；有则复活循环 |
| `toolExecution` | `"sequential"\|"parallel"`（默认 parallel） | 或任一目标工具 executionMode===sequential 则整批串行 |
| `beforeToolCall` | `(ctx, signal) => {block?, reason?}\|undefined` | 参数校验后调用；block:true 则 reason 成为错误 tool result |
| `afterToolCall` | `(ctx, signal) => {content?, details?, isError?, terminate?}\|undefined` | 逐字段整体替换（无深合并）；批内**全部** terminate 才终止 |

上下文类型：`BeforeToolCallContext{assistantMessage, toolCall, args(已校验), context}`；`AfterToolCallContext` 加 `result, isError`；`ShouldStopAfterTurnContext{message, toolResults, context, newMessages}`。

**AgentTool 接口**（types.ts:371-394）：

```ts
interface AgentTool extends Tool { name, description, parameters /*JSON Schema*/ } {
  label: string                       // UI 显示名
  prepareArguments?: (args) => args   // schema 校验前的兼容垫片
  execute: (toolCallId, params, signal?, onUpdate?) =>
    Promise<AgentToolResult>          // 失败用 throw（循环会转成错误 tool result）
  executionMode?: "sequential"|"parallel"
}
AgentToolResult { content:(Text|Image)[]; details:T; terminate?:boolean }
```
注意：**没有 renderCall 字段**——渲染职责在 coding-agent/tui 层，由 details 驱动。

**AgentEvent 联合**（types.ts:413-428）：

| 事件 | payload |
|---|---|
| `agent_start` / `agent_end` | `{}` / `{messages}`（一次 run 的最后事件） |
| `turn_start` / `turn_end` | `{}` / `{message, toolResults}` |
| `message_start` / `message_end` | `{message}` |
| `message_update` | `{message, assistantMessageEvent}`（仅 assistant 流式期间） |
| `tool_execution_start` / `update` / `end` | `{toolCallId, toolName, args[, partialResult][, result, isError]}` |

**PendingMessageQueue**（agent.ts:118-152）：steering/followUp 各一条队列，QueueMode `"all"`（排空点一次性全注入）/ `"one-at-a-time"`（只弹最老一条）。Agent 默认两者都是 one-at-a-time。

### 4.2 runLoop 主循环（`src/agent-loop.ts:155-269`）

```
167:  pendingMessages = await getSteeringMessages()          // ★启动即轮询一次（等待期用户可能已输入）
170:  while (true) {                                          // 外层：followUp 复活
171:    hasMoreToolCalls = true
174:    while (hasMoreToolCalls || pendingMessages.length) {  // 内层：工具链 + steering
182-190:   ★steering 注入：每条 emit message_start/end，push 进 context 与 newMessages
193:       message = await streamAssistantResponse(...)       // ★流式响应（见下）
196-200:   stopReason error/aborted → turn_end(空) → agent_end → return   // 错误即终止
203:       toolCalls = message.content.filter(toolCall)
207-216:   有 → executeToolCalls()；hasMoreToolCalls = !batch.terminate
218:       emit turn_end {message, toolResults}
226:       prepareNextTurn 钩子 → 应用 context/model/thinkingLevel 快照
241:       shouldStopAfterTurn → agent_end → return
253:       pendingMessages = await getSteeringMessages()      // ★每 turn 后轮询
        }  // 内层退出：无工具且无 pending
257:      followUpMessages = await getFollowUpMessages()      // ★本要停止时轮询
258-261:  有 → pendingMessages = followUp；continue 外层
265:      无 → break
268:  emit agent_end {messages: newMessages}
```

**streamAssistantResponse**（:275-368）——AgentMessage→Message 边界：

```
transformContext(messages, signal) → convertToLlm(messages) → llmContext
resolvedApiKey = getApiKey(provider) || config.apiKey        // 动态 key 优先
response = streamFn(model, llmContext, {...config, apiKey, signal})
for await event:
  "start"        → partial 写回：context.messages.push(partial)；emit message_start
  *_start/delta/end → context.messages[len-1] = event.partial（★原地替换最后槽位）
                   ；emit message_update
  "done"/"error" → finalMessage = await response.result()；写回；emit message_end
```

**关键不变量**：partial assistant message 始终占据 context.messages 最后一个槽位、随 delta 原地替换。loop 的 context 与 Agent.state 是两份列表，Agent 靠事件重建（message_end 时才 push）。

**工具批执行**（:373-516）：
- 串行路径：逐个 `tool_execution_start` → prepareToolCall（查工具→prepareArguments 垫片→validateToolArguments→beforeToolCall 钩子；每步检查 aborted）→ execute → finalizeExecutedToolCall（afterToolCall 覆盖）→ `tool_execution_end` → 生成 toolResultMessage → message_start/end。
- 并行路径：**顺序** emit start + prepare（immediate 当场 finalize）；prepared 的 thunk `Promise.all` 并发执行；`tool_execution_end` 按完成序；toolResult 消息**按 assistant 源序**补发。
- 工具缺失 → 错误结果 `"Tool ${name} not found"`；校验失败/钩子 block/aborted → immediate 错误结果（原因文本喂回模型）。

### 4.3 Agent 门面（`src/agent.ts`，557 行）

| API | 行号 | 说明 |
|---|---|---|
| `prompt(msg\|string, images?)` | 325 | 有 activeRun → throw；string 归一化为 user 消息 |
| `continue()` | 338 | 最后是 assistant：先 drain steering（有→作为新 prompt 重跑）；否则 drain followUp；都无→throw。否则 runAgentLoopContinue |
| `steer()/followUp()` | 264 | 入队 |
| `abort()` | 300 | activeRun.abortController.abort() |
| `subscribe(listener)` | 231 | 监听器按订阅顺序 await，计入 run 的 settlement |
| `waitForIdle()` | 309 | `activeRun?.promise ?? resolved` |
| `reset()` | 314 | 清 transcript/运行态/队列 |

状态机：`runWithLifecycle`（:451-474）——executor throw 时 `handleRunFailure` 合成一条失败 assistant 消息（stopReason aborted/error、usage 全零）走完整事件序列；**idle 判定 = finishRun**：agent_end 先发，全部监听器 settle 后才 isStreaming=false。

### 4.4 harness/（通用高层运行时，1029 行）

`AgentHarness` 是面向应用的"会话持久化 + 技能/模板 + 压缩 + 树导航"运行时，直接驱动 runAgentLoop，把钩子位实现为可注册 hook 事件（`tool_call`→block、`context`→替换消息、`before_provider_request/payload`、`session_before_compact/tree` 等，多 handler 取最后非 undefined 结果）。phase 状态机：`idle|turn|compaction|branch_summary|retry`；运行中的 setModel/setTools 进 pendingSessionWrites、turn_end 后 flush 落盘。

**注意**：coding-agent 的 AgentSession **当前不用 AgentHarness**，而是直接 `new Agent(...)`（core/sdk.ts:293-331）；两者是平行实现，AgentSession 自己实现了 auto-retry/auto-compaction。移植时二选一即可（pi-cpp 计划采用 AgentSession 路线）。

**会话存储抽象**（harness/types.ts:440-478）：

```ts
interface SessionStorage {
  getMetadata(); getLeafId(); setLeafId(id);
  createEntryId(); appendEntry(entry);
  getEntry(id); findEntries(type); getLabel(id);
  getPathToRoot(leafId): SessionTreeEntry[]   // root→leaf 有序
  getEntries()
}
interface SessionRepo { create(); open(); list(); delete(); fork(source, {entryId?, position}) }
```

`JsonlSessionRepo`（jsonl-repo.ts）：目录 `{sessionsRoot}/{--编码cwd--}/{ISO时间戳}_{uuidv7}.jsonl`；首行 header `{type:"session", version:3, id, timestamp, cwd, parentSession?}`；每行一个 entry；条目 id = uuidv7 前 8 字符（冲突重试 100 次）。

### 4.5 工具参数校验与流式 JSON（ai 包 utils）

- `parseStreamingJson(partialJson)`（json-parse.ts:104-124）三级降级：① JSON.parse(repairJson(...)) ② partial-json 补全解析 ③ repair 后再 partial-parse；全失败 → `{}`。每个 toolcall_delta 后调用；块 finalize 时再 parse 并删 scratch 字段（partialArgs/partialJson）。
- `validateToolArguments`（validation.ts:292-324）：structuredClone → 标量转换（"5"→5 等，纯 JSON Schema 时走自研递归矫正 coerceWithJsonSchema）→ 编译校验（WeakMap 缓存）→ 失败错误消息含逐路径错误列表 + 完整 args JSON。
- `sanitizeSurrogates`：所有出站文本清除孤立 UTF-16 代理对，防服务端 JSON.parse 失败。

### 4.6 AbortSignal 传递路径

```
Agent.abort() → activeRun.abortController
  → runAgentLoop(signal) → 三处消费：
  (a) streamFn options.signal → 协议层 fetch + 流循环内检查
  (b) 工具链：prepare 各步检查 aborted → immediate "Operation aborted"；
      execute(id, params, signal, onUpdate) 原样传递；before/afterToolCall 钩子自响应
  (c) transformContext(messages, signal)
AgentHarness.abort() 额外清空两队列 + waitForIdle
```
**C++ 对应物**：CancellationToken（见 dev-plan v0.0.1），语义对齐 stop_token。

---

## 5. packages/coding-agent — 应用层

### 5.1 AgentSession（`core/agent-session.ts`，3159 行）

三种模式共享的核心编排层（模式只叠加 I/O）。构造时订阅 agent 事件 → `_handleAgentEvent` **自动落盘**：`message_end` 时按 role 写 sessionManager（custom→appendCustomMessageEntry；user/assistant/toolResult→appendMessage）；assistant 消息另存 `_lastAssistantMessage` 供自动压缩判定。每个事件先转发扩展再给用户 listener。

**对外事件**（AgentEvent 之上扩展）：`queue_update`、`compaction_start/end{reason: manual|threshold|overflow}`、`session_info_changed`、`thinking_level_changed`、`auto_retry_start/end`、`agent_end`（重定义带 willRetry）。

**API 分类**：
- 状态：model/thinkingLevel/isStreaming/systemPrompt/getActiveToolNames/getAllTools/messages/sessionFile...
- 提示：`prompt(text, options)`——流程：`/` 命令 → `input` 扩展事件拦截/变换 → `/skill:name` 与模板展开 → streaming 中按 streamingBehavior 入队 → 压缩预检 → 组装（user 消息 + pendingNextTurn asides + before_agent_start 扩展消息 + 本轮 systemPrompt 可被扩展替换）
- 模型/思考级别：setModel（鉴权→appendModelChange→写 settings）、setThinkingLevel（clamp→appendThinkingLevelChange）
- Bash：executeBash/recordBashResult（BashExecutionMessage；**streaming 时挂 pending 延迟到 agent_end 后写回**，保 tool_use/tool_result 顺序）
- 会话树：navigateTree（收集放弃分支→session_before_tree 可取消→分支摘要→移动 leaf→重建 agent 消息）、getUserMessagesForForking、getSessionStats、getContextUsage
- 导出：exportToHtml、exportToJsonl（新 header + 当前分支重线性化 parentId）
- 扩展：bindExtensions、_refreshToolRegistry（内置+扩展+SDK 工具合并、promptSnippet/Guidelines 提取、allow/deny 过滤）

### 5.2 会话格式完整规格（`core/session-manager.ts`，1578 行）

**版本**：`CURRENT_SESSION_VERSION = 3`。

**Header**（首行）：
```json
{"type":"session","version":3,"id":"<uuidv7>","timestamp":"<ISO>","cwd":"/path/to/proj","parentSession":"<fork来源文件路径,可选>"}
```

**Entry 公共基座**：`{ type, id(8字符随机,冲突重试100次), parentId: string|null, timestamp: ISO }`。全部 entry 类型：

| type | 专有字段 | 说明 |
|---|---|---|
| `message` | `message: AgentMessage` | user/assistant/toolResult/bashExecution 等 |
| `thinking_level_change` | `thinkingLevel` | |
| `model_change` | `provider, modelId` | |
| `compaction` | `summary, firstKeptEntryId, tokensBefore, details?{readFiles,modifiedFiles}, fromHook?` | |
| `branch_summary` | `fromId, summary, details?, fromHook?` | |
| `custom` | `customType, data?` | 扩展私有持久化，**不进 LLM 上下文** |
| `custom_message` | `customType, content, details?, display:boolean` | **进 LLM 上下文**，display 控制 TUI 显隐 |
| `label` | `targetId, label?` | 书签；label=undefined 清除 |
| `session_info` | `name?` | 会话显示名（最新一条生效） |

**树语义**：append-only；append 以当前 leaf 为 parentId、leaf 前移。`branch(id)` 只移 leaf 指针（下次 append 即新分支）；`resetLeaf()` 置 null（下次 append 成新根，用于重编辑首条 user 消息）；`branchWithSummary` 移动 + 追加 branch_summary。

**发给 LLM 的解析规则** `buildSessionContext`（:325-433）：leaf→root 收集路径 → 沿路径折叠最终 thinkingLevel/model → 路径上存在 compaction 则：先输出 compactionSummary 消息，再输出 firstKeptEntryId 到 compaction 的保留消息 + compaction 之后全部消息；无 compaction 则全路径消息。

**文件布局**：`~/.pi/agent/sessions/--<编码cwd>--/<ISO时间戳:冒号换->_<uuidv7>.jsonl`；cwd 编码 `"--" + cwd.replace(/[/\\:]/g,"-") + "--"`。

**延迟落盘**：文件中尚无 assistant 消息时不写盘；首条 assistant 出现后 `open(file,"wx")` 独占创建并全量写出，之后逐条 append。读取 1MB 缓冲按行解析、坏行跳过。迁移 v1→v2→v3（加载后如迁移过整文件重写）。

**Fork**：`createBranchedSession(leafId)`——取根到 leaf 路径，剔除 label 并重链 parentId，写新文件（新 id，parentSession 指向原文件）；`forkFrom` 跨项目 fork（新 cwd header + 逐行复制非 header 条目）。

### 5.3 内置工具（`core/tools/`，7 个）

注册表 `allToolNames = {read, bash, edit, write, grep, find, ls}`；**默认激活 read/bash/edit/write**，grep/find/ls 只读默认关闭。每个工具带 `ToolDefinition`（name/label/description/promptSnippet/promptGuidelines/parameters/execute/renderCall/renderResult）+ 可插拔 `XxxOperations` 接口（SSH 等远程委托点）。

**参数 Schema**：

```ts
bash : { command: String, timeout?: Number /*秒,无默认*/ }
read : { path: String, offset?: Number /*1-indexed 行*/, limit?: Number }
edit : { path: String, edits: Array<{oldText, newText}> }
       // "Each edit is matched against the original file, not incrementally"
write: { path: String, content: String }
grep : { pattern, path?, glob?, ignoreCase?, literal?, context?, limit?/*默认100*/ }
find : { pattern/*glob*/, path?, limit?/*默认1000*/ }
ls   : { path?, limit?/*默认500*/ }
```

**截断策略**（truncate.ts）：`DEFAULT_MAX_LINES=2000`、`DEFAULT_MAX_BYTES=50KB` 双上限独立先到者赢。`truncateHead`（read/grep/find/ls，保头不返回半行）；`truncateTail`（bash，保尾，唯一例外是末行超限时按 UTF-8 字符边界截半行）；单行 >500 字符截断加 `... [truncated]`。read 续读提示 `[Showing lines A-B of N. Use offset=B+1 to continue.]`。

**edit 校验链**（edit.ts execute）：mutation queue 包裹 → R_OK|W_OK 检查 → stripBom（匹配前剥离写回前恢复）→ 行尾检测（首个 \r\n vs \n）→ 归一化 LF 匹配 → 恢复行尾写回 → 生成 diff/patch。校验错误消息（喂回模型自纠错）：空 oldText / 找不到（`must match exactly including all whitespace`）/ 不唯一（`Found N occurrences`）/ 重叠 / 无变化。**匹配是精确优先 + 模糊回退**（NFKC、行尾空白、智能引号→ASCII、Unicode 短横归一）。兼容垫片：edits 为 JSON 字符串则解析；顶层 oldText/newText 合并进 edits。

**bash 执行细节**（bash.ts + utils/shell.ts）：
- Shell 选择：settings shellPath → Windows: Git Bash → PATH bash.exe → 报错；Unix: /bin/bash → which bash → 兜底 sh。旧版 WSL：命令经 stdin 传输。
- spawn：`{cwd, detached: 非win32, stdio:[stdin?"pipe":"ignore","pipe","pipe"], windowsHide}`；env 把 `~/.pi/agent/bin` 前置 PATH（rg/fd）。
- 超时（秒）→ killProcessTree；abort listener 一次性挂载触发即杀树。
- **进程树 kill**：Windows `taskkill /F /T /PID`；Unix `kill(-pid, SIGKILL)`（进程组，detached 使子进程成为组长）回退单 pid。
- 输出：流式 UTF-8 解码；内存滚动 tail = 2×maxBytes；超限落盘临时文件 `${tmpdir}/pi-bash-<hex>.log`；UI 节流 100ms。非零退出码/超时 → 错误文本喂回。
- detached pid 登记，SIGHUP/SIGTERM 时统一杀（print/rpc 模式信号处理器调用）。

**file-mutation-queue**（61 行）：同文件写串行化、不同文件并行。key = realpath；全局注册队列保证原子性 → 每文件 Promise 链。**不在 abort listener 里 reject，而在每个 await 后检查 aborted**，保证锁不提前释放。

### 5.4 压缩 compaction（`core/compaction/`）

**默认设置**：`{enabled:true, reserveTokens:16384, keepRecentTokens:20000}`。

**触发公式**：`shouldCompact = contextTokens > contextWindow - reserveTokens`。contextTokens 优先取最后 assistant 的真实 usage totalTokens；error/全零时回退估算（chars/4，图片按 4800 字符），且校验 usage 不早于最近 compaction。

**触发时机**（agent_end 后 + prompt 提交前）两种：
1. **Overflow**（上下文溢出错误）：同模型 → stopReason=stop 仅压缩；否则删错误消息→压缩→**自动 continue 重试一次**（`_overflowRecoveryAttempted` 防循环）。
2. **Threshold**：压缩，不重试。

**切点算法**：合法切点 = user/assistant/custom/bashExecution/branchSummary/compactionSummary 消息与 branch_summary/custom_message 条目，**绝不切在 toolResult**。从最新向最旧累计 estimateTokens ≥ keepRecentTokens 处取最近合法切点；切点为 assistant 即"拆分回合"（isSplitTurn），另产 turnStartIndex。

**摘要生成**：结构化 prompt 强制输出 `## Goal / ## Constraints & Preferences / ## Progress (Done/In Progress/Blocked) / ## Key Decisions / ## Next Steps / ## Critical Context`；"Preserve exact file paths, function names, and error messages"。有上次 summary 时走增量合并（UPDATE_SUMMARIZATION_PROMPT）。maxTokens = min(0.8×reserve, model.maxTokens)。对话序列化：`[User]:/[Assistant]:/[Assistant thinking]:/[Assistant tool calls]: name(k=v)/[Tool result]:`（tool result 截 2000 字符）。

**文件操作累积追踪**：从 assistant toolCall 参数提 path：read→read 集、write→written、edit→edited，并从上次 compaction.details 累积；modified=edited∪written、readOnly=read−modified，以 `<read-files>/<modified-files>` XML 追加到摘要。

**branch-summarization vs compaction**：前者用于树导航放弃分支（branch_summary entry 挂导航目标位置，预算 contextWindow−reserve，输出 maxTokens 2048，收集到最近公共祖先且不在 compaction 边界停）；后者是同路径上下文超限。两者都可被 `session_before_compact`/`session_before_tree` 扩展钩子取消或整体替换。

### 5.5 扩展系统（`core/extensions/`）

**加载**：jiti 加载 TS/JS；Bun 二进制用虚拟模块表（`@earendil-works/*` 全部指向宿主已打包实例）。发现顺序：项目 `cwd/.pi/extensions/` → 全局 `~/.pi/agent/extensions/` → settings/CLI 显式路径。目录规则：直下 *.ts/*.js（含 symlink）、子目录 index.ts、package.json 的 `"pi":{extensions:[...]}` 清单，**只递归一层**。

**ExtensionAPI 事件面（28 种，types.ts:1133-1171）**——pi-cpp 扩展协议的规格基准：

| 类别 | 事件 | payload → 返回 |
|---|---|---|
| 生命周期 | `session_start{reason: startup/reload/new/resume/fork}` | 无 |
| | `session_before_switch / _fork` | `{cancel?}` 可取消 |
| | `session_shutdown{reason: quit/reload/new/resume/fork}` | 无 |
| 压缩/树 | `session_before_compact{preparation,...}` | `{cancel?, compaction?}` 可整体替换 |
| | `session_compact / session_before_tree / session_tree` | 前者通知；before_tree 可 cancel/换 summary |
| 上下文 | `context{messages}`（每次 LLM 调用前） | `{messages?}` 替换（链式） |
| 请求 | `before_provider_request{payload}` | 返回值替换 payload（链式） |
| | `after_provider_response{status,headers}` | 无 |
| Agent | `before_agent_start{prompt,...}` | `{message?注入, systemPrompt?替换本轮}` |
| | `agent_start/end`、`turn_start/end`、`message_start/update/end` | message_end 可 `{message}` 替换（role 必须相同） |
| | `tool_execution_start/update/end` | 无 |
| 工具 | `tool_call{toolCallId, toolName, input}` | `{block?, reason?}` **可阻止**；input 原地可变改参 |
| | `tool_result{...}` | `{content?, details?, isError?}` 可改结果 |
| 输入 | `input{text, source, streamingBehavior?}` | `continue / transform(text) / handled(吞掉)` |
| 其他 | `project_trust{cwd}`、`resources_discover`、`model_select`、`thinking_level_select`、`user_bash{command}`（可接管执行） | |

**注册类 API**：registerTool / registerCommand / registerShortcut / registerFlag / registerMessageRenderer / registerProvider。**动作类**：sendMessage / sendUserMessage / appendEntry / setSessionName / setLabel / exec / setActiveTools / setModel / compact / events(EventBus 扩展间通信)。**UI 面**（ExtensionUIContext）：select/confirm/input/notify/setStatus/setWidget(aboveEditor|belowEditor)/setFooter/setHeader/setTitle/editor/pasteToEditor/addAutocompleteProvider/setEditorComponent/theme...

**官方示例**（~80 个，`examples/extensions/`）关键几个：permission-gate（危险命令确认）、git-checkpoint（检查点/回滚）、subagent（spawn 独立 pi 进程）、custom-provider-*（自定义 provider + OAuth）、plan-mode、sandbox、todo、structured-output。

### 5.6 系统提示组装（`core/system-prompt.ts:28-173`）

固定顺序：
1. 开头 "You are an expert coding assistant operating inside pi, a coding agent harness..."
2. `Available tools:` —— 激活工具的 promptSnippet 列表
3. `Guidelines:` —— 各工具 promptGuidelines 去重 + 条件项 + 固定两条（"Be concise" / "Show file paths clearly"）
4. **pi 文档段**（README/docs 路径 + 话题映射）
5. `--append-system-prompt` 追加段
6. `<project_context>` 块：每文件 `<project_instructions path="…">…</project_instructions>`
7. skills 段（仅当 read 工具激活）`<available_skills><skill><name/><description/><location/></skill>...`
8. 末两行 `Current date: YYYY-MM-DD` / `Current working directory: /posix/路径`

**项目上下文文件查找**（resource-loader.ts:84-122）：候选名 `AGENTS.md, AGENTS.MD, CLAUDE.md, CLAUDE.MD`；全局 `~/.pi/agent/` 一份 → 从 cwd 向上逐级到根各一份 → 去重。

**skills**（core/skills.ts）：frontmatter `name?/description(必填,≤1024)/disable-model-invocation?`；名称 `^[a-z0-9-]+$` ≤64 字符；发现 `~/.pi/agent/skills` → `cwd/.pi/skills` → 显式路径；尊重 .gitignore；`/skill:name` 展开为 `<skill name="…" location="…">正文</skill>` 块。

### 5.7 配置与目录布局（config.ts + settings-manager.ts）

```
~/.pi/agent/                       # $PI_CODING_AGENT_DIR 可覆盖
├── settings.json                  # 全局设置
├── auth.json                      # 凭据（含 OAuth）
├── models.json                    # 自定义 provider/model（支持 // 注释）
├── keybindings.json               # 键位覆盖
├── AGENTS.md / CLAUDE.md          # 全局项目上下文
├── extensions/  skills/  prompts/  themes/  tools/
├── bin/                           # 自动下载的 rg/fd 二进制
├── sessions/--<编码cwd>--/<时间戳>_<uuidv7>.jsonl
└── pi-debug.log
项目级: cwd/.pi/{settings.json, extensions/, skills/, prompts/}
```

**settings.json 关键字段**：`defaultProvider/defaultModel/defaultThinkingLevel/transport/steeringMode/followUpMode(默认 one-at-a-time)/theme/compaction{enabled,reserveTokens=16384,keepRecentTokens=20000}/branchSummary{reserveTokens}/retry{enabled=true,maxRetries=3,baseDelayMs=2000,provider{timeoutMs,maxRetries,maxRetryDelayMs=60000}}/hideThinkingBlock/shellPath/shellCommandPrefix/defaultProjectTrust(ask|always|never)/terminal{...}/images{autoResize,blockImages}/enabledModels/thinkingBudgets{...}/sessionDir/httpProxy/...`

**合并**：`deepMergeSettings(global, project)`（嵌套递归、基元/数组 project 覆盖）；**项目未信任时完全不加载项目级文件**（trust 机制）。写盘：proper-lockfile 锁 + 字段级合并（只写本会话改过的字段，避免覆写并发会话）。

**models.json 格式**（真实示例）：

```json
{
  "providers": {
    "ollama": {
      "baseUrl": "http://localhost:11434/v1",
      "api": "openai-completions",
      "apiKey": "ollama",
      "compat": { "supportsDeveloperRole": false, "supportsReasoningEffort": false },
      "models": [
        { "id": "llama3.1:8b", "name": "Llama 3.1 8B (Local)", "reasoning": false,
          "input": ["text"], "contextWindow": 128000, "maxTokens": 32000,
          "cost": { "input": 0, "output": 0, "cacheRead": 0, "cacheWrite": 0 } }
      ]
    }
  }
}
```
apiKey 支持 `$VAR`/`${VAR}`/`!command` 插值。**registry 合并优先级**：内置 models.generated → models.json 的 provider 覆盖（baseUrl/compat）与 modelOverrides → 自定义 models **替换内置同名** → OAuth provider 最后加工。API key 解析：authStorage(OAuth) > provider.apiKey；headers = model < provider；authHeader:true 自动 Bearer。

### 5.8 三种运行模式

**print**（`modes/print-mode.ts`，159 行）：text 模式逐 text 块写 stdout，error/aborted → stderr + exit 1；json 模式先输出 session header 一行，随后**每个 AgentSessionEvent 一行 JSON**。信号 SIGTERM/SIGHUP → 杀 detached 进程 → exit 143/129。

**interactive**（5741 行）：**不含业务逻辑**——只做 TUI 组件树、键位、斜杠命令（/model /tree /fork /resume /theme /reload /export...）、选择器、扩展 UI 上下文实现、`!`/`!!` bash 前缀。会话切换委托 AgentSessionRuntime。

**rpc**（`modes/rpc/rpc-mode.ts:774 + rpc-types.ts`）：stdin/stdout **严格 LF 分隔 JSONL**（stdout 被接管防污染）；扩展 UI 经 `extension_ui_request/response` 往返。命令全表：

| 命令 | payload → 响应 |
|---|---|
| `prompt` | `{message, images?, streamingBehavior?}` → preflight 成功回 success，后续走事件流 |
| `steer` / `follow_up` | `{message}` → 无 data |
| `abort` / `abort_bash` / `abort_retry` | – |
| `new_session{parentSession?}` / `clone` / `switch_session{sessionPath}` | `{cancelled}` |
| `fork{entryId}` | `{text, cancelled}`（目标为 user 消息时返回编辑器文本） |
| `get_state` | `{model,thinkingLevel,isStreaming,isCompacting,steeringMode,followUpMode,sessionFile,...}` |
| `set_model{provider,modelId}` / `cycle_model` / `get_available_models` | `Model` / `{model,...}\|null` / `{models}` |
| `set_thinking_level{level}` / `cycle_thinking_level` | 无 / `{level}\|null` |
| `set_steering_mode` / `set_follow_up_mode{mode}` | 无 |
| `compact{customInstructions?}` | `CompactionResult` |
| `set_auto_compaction{enabled}` / `set_auto_retry{enabled}` | 无 |
| `bash{command, excludeFromContext?}` | `BashResult` |
| `get_session_stats` / `get_messages` / `get_commands` / `get_fork_messages` / `get_last_assistant_text` | 各自数据 |
| `export_html{outputPath?}` | `{path}` |
| `set_session_name{name}` | 无 |

错误响应 `{id?, type:"response", command, success:false, error}`；AgentSessionEvent 原样逐行输出为通知。

---

## 6. packages/tui — 终端 UI 库

### 6.1 差分渲染（tui.ts `doRender`:1254-1620）

组件模型：`Component{render(width): string[]; handleInput?; invalidate()}`；`Container` 顺序拼接。渲染节流合帧 16ms。**三类渲染路径**：
1. **全量重绘**（首帧/宽度变化/高度变化/clearOnShrink/首变化行在旧视口上方/行删除推挤视口）：`\x1b[2J\x1b[H\x1b[3J` + 清 kitty 图片。
2. **纯删除行路径**：只有行消失时，移到内容末尾逐行 `\r\x1b[2K` 清除，不上推。
3. **差分路径**：找 firstChanged/lastChanged；纯追加先滚动视口；光标相对移动；**只重绘变化区间**，每行清行后写入（spinner 场景显著减闪烁）。行可视宽 > 终端宽 → crash log + throw。

**CSI 2026 同步输出**：所有路径包裹 `\x1b[?2026h … \x1b[?2026l`，一次性 write。其它：overlay 栈合成（anchor/百分比尺寸/visible 回调）；`CURSOR_MARKER`（APC 零宽序列）标记 IME 光标位；OSC 11 背景色 + CSI ?996n 配色方案探测。

### 6.2 ProcessTerminal（terminal.ts）

`Terminal` 接口 + 实现：raw mode + 恢复；**bracketed paste** `\x1b[?2004h`；resize → SIGWINCH；Windows ENABLE_VIRTUAL_TERMINAL_INPUT；**Kitty 键盘协议协商**（`>7u`，按 DA 响应回退 modifyOtherKeys）；StdinBuffer 10ms 拆分批量输入；OSC 9;4 进度指示。

### 6.3 组件与键位/主题

组件：editor（多行编辑：undo-stack/kill-ring/补全/IME 标记，2307 行）、markdown、select-list、settings-list、image（kitty/iTerm2）、box/text/truncated-text/spacer/loader/cancellable-loader/input；支撑 keys/fuzzy/utils(visibleWidth、ANSI 感知折行)/terminal-image 等。

键位：tui 层默认表（tui.editor.*/input.*/select.*）+ coding-agent 经 declaration merging 追加 `app.*`（escape=中断、ctrl+c=清空、ctrl+d=退出、shift+tab=思考级别、ctrl+p=模型轮换、ctrl+l=模型选择、ctrl+o=展开工具、ctrl+t=思考块、alt+enter=followUp...）；用户 `~/.pi/agent/keybindings.json` 覆盖。

主题：Theme 接口（fg/bg/语法映射/loader 帧）+ 内置 dark/light JSON + schema 校验；`theme` 全局 Proxy 可热切；跟随系统明暗（OSC 11）。

---

## 7. 核心数据流走查（一次 prompt 的完整生命周期）

以 interactive 模式用户输入 "帮我修这个 bug" 为例：

```
1. TUI editor 收到回车 → InteractiveMode 处理（! 前缀走 bash-executor；/ 前缀走命令）
2. session.prompt(text)
   ├─ input 扩展事件（可变换/吞掉）
   ├─ /skill 与模板展开
   ├─ streaming 中 → steer/followUp 入队 + queue_update 事件，结束
   ├─ 压缩预检（overflow 恢复）
   └─ before_agent_start 扩展（可注入消息/换 systemPrompt）
3. agent.prompt(userMessage)
   └─ runWithLifecycle: isStreaming=true → runAgentLoop
4. runLoop 内层循环：
   a. steering 注入（message_start/end + push）
   b. streamAssistantResponse:
      transformContext → convertToLlm（bashExecution/自定义消息被过滤/映射）
      → streamSimple 按 model.api 分派 → openai-completions 协议模块
      → buildParams（消息转换 + tools schema + thinking 变体）→ SSE POST
      → 逐 chunk：text/reasoning_content/tool_calls delta → AssistantMessageEvent
      → EventStream push → loop 消费：partial 原地写回 context.messages 尾槽
      → message_update 事件 → AgentSession 转发扩展 → TUI 流式渲染
   c. done → message_end → AgentSession 落盘 appendMessage
   d. 有 toolCalls → executeToolCalls:
      tool_execution_start → beforeToolCall 钩子（扩展 tool_call 事件可 block）
      → tool.execute(id, params, signal, onUpdate)（read/edit/write 经 mutation queue）
      → afterToolCall → tool_execution_end → toolResultMessage → message_start/end → 落盘
   e. turn_end → prepareNextTurn（flush pending：模型切换等）→ steering 轮询
5. 模型无 toolCalls 且无 steering → 外层 followUp 轮询 → 无 → agent_end
6. AgentSession._handlePostAgentRun:
   ├─ 成功 → retry 计数清零
   ├─ 可重试错误 → _prepareRetry（退避、删错误消息）→ agent.continue()
   └─ _checkCompaction（threshold/overflow → compact → 可能 continue）
7. finishRun: isStreaming=false → waitForIdle resolve → TUI 恢复输入
```

---

## 8. 线协议规格（C++ 移植用）

### 8.1 openai-completions（Chat Completions）

**请求**（buildParams:534-687）：

```jsonc
POST {baseUrl}/chat/completions
{
  "model": "…",
  "messages": [ {"role":"system"|"developer","content":"…"}, … ],
  "stream": true,
  "stream_options": { "include_usage": true },
  "store": false,
  "max_completion_tokens": 16384,          // 或 max_tokens，按 compat.maxTokensField
  "temperature": 0.7,
  "tools": [ { "type":"function",
      "function": {"name":"…","description":"…","parameters":{…schema…},"strict":false} } ],
  "tool_choice": "auto",
  "reasoning_effort": "medium"             // 9 种 thinkingFormat 变体之一
}
```
- 历史含 toolCall/toolResult 而本次 tools 为空时强制 `"tools": []`。
- thinkingFormat 变体（各家不兼容服务器）：zai `thinking:{type}`、qwen `enable_thinking`、deepseek `thinking:{type}`、openrouter `reasoning:{effort}`、together `reasoning:{enabled}`、string-thinking 顶层 `thinking:string` 等。
- 消息转换细节：assistant 的 text 合并为**纯字符串**；thinking 块用 thinkingSignature 作为动态字段名（如 `reasoning_content`）；tool_calls 的 arguments JSON.stringify；**无 content 且无 tool_calls 的 assistant 消息整条跳过**（aborted 空响应）；toolResult 逐条转 `{role:"tool", content, tool_call_id}`，图像收集后追加合成 user 消息。
- toolCallId 归一化：非 `[a-zA-Z0-9_-]` 替换 `_`、截 40 字符。

**SSE chunk 真实样例**（工具调用增量）：

```jsonc
{"id":"chatcmpl-x","choices":[{"delta":{"tool_calls":[{"index":0,"id":"functions.read:0","type":"function","function":{"name":"read","arguments":""}}]},"finish_reason":null}]}
{"id":"chatcmpl-x","choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"{\"path\":\"README"}}]},"finish_reason":null}]}
{"id":"chatcmpl-x","choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":".md\"}"}}]},"finish_reason":"tool_calls"}],
 "usage":{"prompt_tokens":10,"completion_tokens":5,"prompt_tokens_details":{"cached_tokens":0}}}
```
delta 处理：`content`→text；`reasoning_content|reasoning|reasoning_text`→thinking；`tool_calls[]` 按 index/id 双索引定位，arguments 增量拼 partialArgs 并每次 parseStreamingJson。

**finish_reason 映射**：null/stop/end→stop；length→length；function_call/tool_calls→toolUse；content_filter/network_error/其他→error+errorMessage。

**usage 解析**：`cacheRead = prompt_tokens_details.cached_tokens ?? prompt_cache_hit_tokens`；`input = max(0, prompt − cacheRead − cacheWrite)`（不互减）；output 含 reasoning。

**compat 默认值**：`supportsStore/supportsDeveloperRole/supportsReasoningEffort/supportsUsageInStreaming:true`、`maxTokensField:"max_completion_tokens"`、`thinkingFormat:"openai"`。

### 8.2 anthropic-messages

**请求**：

```jsonc
POST {baseUrl}/v1/messages
{
  "model": "claude-opus-4-6",
  "messages": [...],                       // 见下
  "max_tokens": N,                          // 必填
  "stream": true,
  "system": [ {"type":"text","text":"…","cache_control":{"type":"ephemeral"[,"ttl":"1h"]}} ],
  "temperature": 0.7,                       // 仅未开 thinking
  "tools": [ {"name":"edit","description":"…","input_schema":{…},
              "eager_input_streaming":true, "cache_control":{…} } ],
  "thinking": { "type":"adaptive","display":"summarized" }   // 或 {"type":"enabled","budget_tokens":N}
                                                    // 或 {"type":"disabled"}
}
```
消息转换：user 字符串非空白才发；assistant thinking 带 signature 原样回放（redacted→`redacted_thinking`）；toolCall→`tool_use{id,name,input}`；**连续 toolResult 合并为一条 user 消息**的 `tool_result` 块数组；最后一条 user 消息末块打 cache_control。

**OAuth 伪装细节**（移植可忽略但需知）：`sk-ant-oat` key 时带 claude-code beta 头、固定 system 首块、工具名映射到 Claude Code 规范名。

**SSE 事件真实序列**：

```
event: message_start
data: {"type":"message_start","message":{"id":"msg_…","usage":{"input_tokens":12,"output_tokens":0,...}}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{...}}

event: message_stop
data: {"type":"message_stop"}
```
处理：content_block_start 的 thinking 初始化空 signature、redacted_thinking→`{thinking:"[Reasoning redacted]",redacted:true}`；`input_json_delta` 拼 partialJson 并 parseStreamingJson；`signature_delta` 追加 thinkingSignature；usage 仅在非 null 时覆盖（代理会省略）。

**stop_reason 映射**：end_turn/pause_turn/stop_sequence→stop（pause_turn 可直接重提交）；max_tokens→length；tool_use→toolUse；refusal/sensitive→error；未知→throw（进 catch→error 事件）。

### 8.3 transform-messages（跨协议公共前置，64-219 行）

AgentMessage → 各协议请求体之前统一做三件事：
1. **图像降级**：model.input 不含 image 时替换为占位文本，连续占位去重。
2. **同模型判定与 thinking 处理**：redacted 跨模型丢弃；同模型有 signature 原样保留（回放必需）；**跨模型 thinking 降级为 text 块**；toolCall 跨模型剥 thoughtSignature、改写 id 并维护映射。
3. **孤儿 tool call 补齐**：**stopReason 为 error/aborted 的 assistant 消息整条删除**（不完整 turn 回放会触发 API 错误）；assistant 的 toolCall 若无对应 toolResult，合成 `{role:"toolResult",content:[{text:"No result provided"}],isError:true}`。

> pi-cpp 的 `repair_tool_history` 对应此第 3 条（tau 的 tool_history.py 是同型实现，169 行，更适合逐行翻译）。

---

## 9. 错误、重试与取消模型

### 9.1 两层重试

**层 1（HTTP 级，默认关闭）**：SDK 请求 `maxRetries: options?.maxRetries ?? 0`；`maxRetryDelayMs`(默认 60000)——服务器要求更长延迟时不等待、直接失败并把延迟写进错误，交给上层有可见性地处理。参考实现 openai-codex-responses.ts:56-393（429/5xx/网络错、retry-after 解析、指数退避）。

**层 2（agent run 级，AgentSession 自动重试）**：
- 判定 `_isRetryableError`：stopReason=error 且 errorMessage 匹配白名单正则（overloaded/rate limit/429/5xx/network/connection/timeout/...），**上下文溢出不重试**（交给 compaction），终端配额错误（insufficient_quota/billing 等）不重试。
- 退避：默认 `maxRetries:3, baseDelayMs:2000`，`delay = base × 2^(attempt-1)`；发 `auto_retry_start` 事件；**从 state.messages 删除末尾错误 assistant 消息**（保留在 session 文件）再 `agent.continue()`；成功即清零计数。

### 9.2 错误编码

协议层 catch：`stopReason = aborted?"aborted":"error"`、`errorMessage = error.message`，push error 事件 + `result()` 返回错误消息。Agent 门面兜底 `handleRunFailure` 合成失败 assistant 消息走完整事件序列。回放语义：错误/中止消息在 transformMessages 中剔除，不污染下一请求。

### 9.3 取消

见 §4.6 AbortSignal 路径。C++ 对应 CancellationToken（语义对齐 stop_token，dev-plan v0.0.1 交付）。

---

## 10. 与 pi-cpp 的映射关系

### 10.1 分层对应

| pi | tau（第二参考） | pi-cpp（本仓库） | dev-plan 版本 |
|---|---|---|---|
| packages/ai 类型 + EventStream | tau_ai | `src/ai/` + `src/agent/` 类型 | v0.0.1–v0.0.2 |
| packages/agent agent-loop/Agent | tau_agent | `src/agent/` | v0.0.3 |
| coding-agent tools | tau_coding tools | `src/coding/tools/` | v0.0.4 |
| print-mode | cli print | `src/cli/print_mode.cpp` | v0.1.0 |
| session-manager | tau_agent/session | `src/coding/session/` | v0.2.0 |
| compaction + 树导航 | — | `src/coding/compaction.cpp` | v0.3.0 |
| ai 双协议 + catalog | tau_ai 双适配器 | `src/ai/` | v0.4.0 |
| packages/tui | — | `src/tui/`（FTXUI） | v0.5.0 |
| rpc-mode + extensions | rpc.py | `src/cli/rpc_mode.cpp` | v0.6.0 |
| skills/templates | skills.py | `src/coding/` | v1.0.0 |

### 10.2 移植时优先抄 tau 的部分

tau（HuggingFace 官方 Python 复刻）与 pi wire 兼容、零 SDK 依赖（全部手写 SSE），核心仅 ~7,400 行。**手写协议/SSE 解析/事件归一化**优先对照 tau（`tau_ai/anthropic.py:224` 的 SSE 行解析状态机、`tau_ai/stream.py:88` 的 canonicalizer、`tau_agent/tool_history.py` 的孤儿修复）；**契约语义与功能面**以本文档（源自 pi）为准。tau 的 `dev-notes/`（phase-1…28 施工日志）是增量顺序的验证参考。

### 10.3 必须保持的关键不变量（移植检查清单）

1. StreamFn 不抛异常——错误永远是 `stopReason:error` 的 AssistantMessage。
2. partial assistant message 原地占据 context.messages 最后槽位，随 delta 替换。
3. 会话 append-only：一切（fork/compaction/模型切换）都是追加 entry。
4. 工具批：默认并行、mutation queue 串行文件写、toolResult 按源序补发。
5. steering/followUp 排空点固定：启动时轮询一次 steering；每 turn 后再轮询；停止前轮询 followUp。
6. 终止聚合：批内**全部** terminate=true 才提前终止。
7. 出站 JSON 文本清除孤立 UTF-16 代理对。
8. usage 的 input 不与 cacheRead/cacheWrite 互减（OpenAI 语义）。
9. 错误/aborted 的 assistant 消息在重放请求前删除（transform-messages 规则 3）。
10. 延迟落盘：首条 assistant 出现才创建会话文件。

### 10.4 明确不移植的部分（pi-cpp 非目标）

OAuth 各流、Kitty/iTerm2 图片、HTML 导出、遥测、自更新、models.generated 生成管线、Bun 双运行时分发、动态 TS 扩展加载（改为进程外 RPC 扩展，见 dev-plan v0.6.0）、沙箱/容器（哲学一致：信任 + 外部隔离）。
