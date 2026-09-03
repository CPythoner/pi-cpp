# Changelog

本仓库所有显著变更记录。每个版本与 `docs/design/vX.Y.Z.md`、CHANGELOG 和 git tag 一一对应。

## [Unreleased]

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
