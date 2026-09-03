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
| v0.0.1 | 骨架 + 核心类型 + 三层 SDK 边界 | ✅ Final closeout 完成，tag 待重指向最终 commit |
| v0.0.2 | Provider / SSE + 真实差分测试基座 | 待开始 |
| v0.0.3 | Agent 主循环 | 待开始 |
| v0.0.4 | coding-agent 工具四件套 | 待开始 |
| v0.1.0 | print MVP + Stable Session Wire | 待开始 |

完整路线见 [docs/dev-plan.md](docs/dev-plan.md)；v0.0.1 设计见 [docs/design/v0.0.1.md](docs/design/v0.0.1.md)，最终验收记录见 [docs/closeout/v0.0.1.md](docs/closeout/v0.0.1.md)。

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

输出：

```text
picpp v0.0.1
```

## 本地要求

- CMake ≥ 3.25
- 支持 C++17 的编译器（GCC / Clang / MSVC）

当前依赖 nlohmann/json、fmt、doctest 由 CMake FetchContent 自动拉取。
