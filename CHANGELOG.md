# Changelog

本仓库所有显著变更记录。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，版本号遵循三位语义化（从 v0.0.1 起）。每个版本与 `docs/design/vX.Y.Z.md` 设计文章、git tag 一一对应。

## [Unreleased]

## [v0.0.1] - 2026-09-03
### Added
- CMake 骨架与三平台 CI（ubuntu-24.04 / macos-15 / windows-2022；C++17、-Wall -Wextra -Werror）
- 依赖管理：nlohmann/json v3.11.3、fmt 10.2.1、doctest v2.4.12（FetchContent + SYSTEM + 精确 pin）
- pi 兼容消息类型系统：4 种内容块 + 7 种消息角色，camelCase JSON 双向序列化，黄金样本 JSONL round-trip
- 事件类型系统：L1 provider 事件 12 种 + L2 agent 事件 10 种
- util 基础设施：CancellationToken / CombinedCancellation / ThreadGuard / string 工具 / overload
- `picpp --version` 入口
- 63 个 doctest 用例（6 个测试目标）

<!-- 模板：
## [vX.Y.Z] - YYYY-MM-DD
### Added / Changed / Fixed / Removed
- ...
-->
