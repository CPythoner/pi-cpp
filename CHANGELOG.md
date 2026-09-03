# Changelog

本仓库所有显著变更记录。每个版本与 `docs/design/vX.Y.Z.md`、CHANGELOG 和 git tag 一一对应。

## [Unreleased]

## [v0.0.2] - 2026-09-03
### Added
- `pi::ai::Provider`、`Model`、`Context`、`StreamOptions` public API，以及 thread-safe `EventStream` / `AssistantMessageEventStream`
- public `OpenAICompatibleProvider`，通过 PImpl 隔离 private HTTP 实现
- private CPR 1.14.2 / libcurl HTTP streaming adapter，支持 timeout、low-speed 与 cancellation
- 独立 SSE parser：任意 transport chunk 边界、CRLF/LF、多 `data:` 行、id/event/retry、`[DONE]`
- OpenAI Chat Completions compatible request builder、streaming JSON、text/thinking/tool-call delta 聚合、finish_reason/usage 映射
- OpenAI-compatible retry/error mapping：408、409、429、5xx、transport、`x-should-retry`、`retry-after-ms`、`Retry-After`、exponential backoff/jitter
- 真实 pi `v0.80.0` reference harness：exact commit checkout + deterministic local HTTP/SSE server + normalized JSONL differential
- 三组 canonical trace：`text-stop`、`tool-call`、`length-stop`
- Compatibility workflow：checked-in canonical fixture == fresh exact pi `v0.80.0` == current pi-cpp 三方严格比较
- Provider/EventStream/SSE/request/delta/retry/HTTP integration tests，以及更新后的 independent `pi::ai` public consumer test
- `docs/closeout/v0.0.2.md` 最终验收记录

### Changed
- `CancellationToken::request()` 改为锁内 transition + callback snapshot/detach，用户 callback 锁外执行
- register-after-cancel callback 改为锁外同步执行；request/unregister 竞态语义固定并测试
- `CombinedCancellation` 改用 shared/weak state，避免 source request 与 owner 析构竞态访问裸 `this`
- `pi_ai` 从仅消息/事件类型层扩展为第一条可运行 Provider/SSE SDK；CPR 仍保持 PRIVATE linkage
- `BUILD_SHARED_LIBS` 仅在 pi-cpp 依赖目录作用域关闭，不再强写宿主工程 cache
- retry 语义按 pi `v0.80.0` 使用的 openai-node 6.26.0 对齐：retry sleep 本身不被 AbortSignal 提前唤醒，取消在 delay 后、下一次请求前观察
- OpenAI 路径为兼容固定基线故意不使用 `StreamOptions::maxRetryDelay` 截断 server 指定的 Retry-After

### Fixed
- 真实 differential 发现并修复 `text_start.partial`：首个 start event 的 partial 已包含首个 text delta，匹配 pi `v0.80.0` 可观察行为
- 真实 differential 发现并修复 `toolcall_start.partial.arguments`：start event 已包含首轮 partial JSON 解析结果
- 修复 reference TypeScript runner 在 CJS 转换环境中的 top-level await 问题
- 修正文档中“retry backoff wait 可提前取消”的过强描述，以及 `maxRetryDelay` public 注释与实际 OpenAI 兼容行为不一致的问题

### Validation
- 代码验收基线 `36f6e3be0e772e2c78cb9164dd595a7f4536ded8`：GitHub Actions CI run #239 在 ubuntu-24.04、macos-15、windows-2022 均通过 configure / build / ctest
- 同一代码验收基线：Compatibility run #10 成功执行 exact pi `v0.80.0` checkout、reference dependencies、pi-cpp runner 与 strict differential
- canonical fixture commit `64fa3a8f55ee2b024a66c57704c55880d5580a3c`：Compatibility run #8 首次证明 checked-in fixture、fresh pi `v0.80.0`、pi-cpp 三方一致
- public-header audit 未发现 cpr/curl 类型进入 `include/pi/ai/...`

### Known limitations
- canonical real differential 当前聚焦 OpenAI-compatible L1 streaming 行为的三组确定性场景，不代表 pi `v0.80.0` 全 Provider/全输入 wire 已完成兼容证明
- Session wire/tree/compaction 仍按路线图从 v0.1.0–v0.1.2 实现，不属于 v0.0.2
- Agent runLoop 与 coding tools 分别留给 v0.0.3 / v0.0.4
- `cmake --install` / export targets / `find_package(picpp CONFIG REQUIRED)` 仍计划在 v0.1.0 完成

## [v0.0.1] - 2026-09-03
### Added
- CMake 骨架与三平台 CI（ubuntu-24.04 / macos-15 / windows-2022；C++17、warning-as-error）
- 三层 SDK 边界：`ai` → `agent` → `coding-agent`
- public headers：`include/pi/ai`、`include/pi/agent`、`include/pi/coding-agent`
- CMake targets：`pi::ai`、`pi::agent`、`pi::coding-agent`
- canonical C++ namespaces：`pi::ai`、`pi::agent`、`pi::coding_agent`
- `apps/picpp/main.cpp` 官方 CLI 入口；核心 SDK 不依赖 CLI/TUI
- ai 消息/内容类型：4 种 ContentBlock、User/Assistant/ToolResult Message、Usage/Cost/StopReason
- agent 消息类型：7 种 AgentMessage 角色
- ai L1 Provider 事件 12 种 + agent L2 事件 10 种
- C++17 基础设施：CancellationToken / CombinedCancellation / ThreadGuard / string helpers
- 三个手工 JSONL fixture round-trip 测试
- `ai` / `agent` / `coding-agent` 三个独立 public SDK consumer tests
- `picpp --version`
- `docs/closeout/v0.0.1.md` 最终验收记录

### Changed
- 移除长期使用单一 `pi_types` target 的方案；类型按 `ai` / `agent` 职责拆分
- `src/` 不再作为 SDK PUBLIC include path
- CLI 从 `src/cli` 移到 `apps/picpp`
- 三个 SDK target 都显式声明自己的 canonical build-time public include root
- public headers 不再导出顶层 `pi::Type` compatibility aliases，统一使用所属 SDK namespace
- 行为测试统一通过 canonical `<pi/...>` headers 消费，并按 `pi::ai` / `pi::agent` 分层链接
- unknown role 测试只冻结异常类别与实际 role 诊断，不冻结错误前缀逐字文本
- 行为/wire 规范明确以 pi `v0.80.0` 为准，Tau `v0.4.1` 仅作实现参考

### Validation
- 最终 SDK/namespace 修复后的 `74842204dc91a14444484fd78c45fbde7097dcc7` 在 GitHub Actions CI run #57 上通过 ubuntu-24.04、macos-15、windows-2022 的 configure / build / ctest
- closeout 后 `v0.0.1` 功能面冻结；重新发布时 tag 必须指向包含 closeout 记录且三平台 CI 为 green 的最终 commit

### Known limitations
- 当前 JSONL fixtures 为手工样本，不构成真实 pi `v0.80.0` compatibility evidence；v0.0.2 建立 reference harness + differential tests
- CancellationToken 在进入真实 HTTP 前仍需完成 callback 锁外执行与 unregister/request/destructor race 加固
- coding-agent 在 v0.0.1 仅建立 target/public namespace 骨架；read/write/edit/bash 从 v0.0.4 实现
- `cmake --install` / export targets / `find_package(picpp CONFIG REQUIRED)` 从 v0.1.0 实现

<!-- 模板：
## [vX.Y.Z] - YYYY-MM-DD
### Added / Changed / Fixed / Removed
- ...
-->
