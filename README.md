# pi-cpp

用 C++17 重写 coding agent [pi](https://github.com/earendil-works/pi) 核心能力的学习型项目。

行为语义基线固定为 **pi `v0.80.0`**；Tau `v0.4.1` 仅用于辅助理解模块划分、算法和工程实现。

## SDK 架构

核心从第一版开始按三层 SDK 组织：

```text
external C++ app / picpp
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

公共头文件：

```text
include/pi/ai/
include/pi/agent/
include/pi/coding-agent/
```

CMake target：

```cmake
pi::ai
pi::agent
pi::coding-agent
```

C++ namespace：

```cpp
pi::ai
pi::agent
pi::coding_agent
```

`src/` 只用于实现和 private support，不作为 SDK PUBLIC include path；`apps/picpp/` 是官方 CLI/TUI 前端。

## 路线图

| 版本 | 主题 | 状态 |
|---|---|---|
| v0.0.1 | 骨架 + 核心类型 + 三层 SDK 边界 | ✅ Final closeout 完成 |
| v0.0.2 | Provider / SSE + 真实差分测试基座 | ✅ Final closeout 完成 |
| v0.0.3 | Agent 主循环 | 待开始 |
| v0.0.4 | coding-agent 工具四件套 | 待开始 |
| v0.1.0 | print MVP + Stable Session Wire | 待开始 |

完整路线见 [docs/dev-plan.md](docs/dev-plan.md)；v0.0.2 设计与最终验收见 [docs/design/v0.0.2.md](docs/design/v0.0.2.md) 和 [docs/closeout/v0.0.2.md](docs/closeout/v0.0.2.md)；v0.0.1 记录见 [docs/design/v0.0.1.md](docs/design/v0.0.1.md) 和 [docs/closeout/v0.0.1.md](docs/closeout/v0.0.1.md)。

## 构建

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

构建成功后：

```bash
./build/release/picpp --version
```

当前源码构建输出：

```text
picpp v0.0.2
```

## 本地要求

- CMake ≥ 3.25
- 支持 C++17 的编译器（GCC / Clang / MSVC）

当前通过 CMake FetchContent 拉取 nlohmann/json、fmt、doctest 与 CPR；CPR/libcurl 只作为 `pi::ai` 的 private HTTP 实现依赖，不进入 public SDK API。
