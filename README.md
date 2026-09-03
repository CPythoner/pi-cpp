# pi-cpp

用 C++17 重写极简主义 coding agent [pi](https://github.com/earendil-works/pi) 的学习型项目：以 [tau](https://github.com/huggingface/tau) 为翻译蓝本、pi 为语义权威，逐版本重建其核心能力。

## 路线图

| 版本 | 主题 | 状态 |
|---|---|---|
| v0.0.1 | 骨架与核心类型（消息/事件类型系统、取消原语） | ✅ 完成（tag `v0.0.1`） |
| v0.0.2 | Provider 层与 SSE 流式 | 待开始 |
| v0.0.3 | Agent 主循环 | 待开始 |
| v0.0.4 | 工具四件套 | 待开始 |
| v0.1.0 | print 模式 MVP（第一个可用版本） | 待开始 |

完整路线见 [docs/dev-plan.md](docs/dev-plan.md)；pi 架构参考见 [docs/pi-architecture.md](docs/pi-architecture.md)。

## 构建

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

构建成功后运行 `./build/release/picpp --version` 输出 `picpp v0.0.1`。

## 本地要求

- CMake ≥ 3.25
- 支持 C++17 的编译器（GCC / Clang / MSVC）

其余依赖（nlohmann/json、fmt、doctest）由 CMake FetchContent 自动拉取，无需预装。
