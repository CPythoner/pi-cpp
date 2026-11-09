# CMake SDK Boundaries：Public/Private 依赖与宿主工程隔离

> 建立版本：v0.0.2。  
> 主要代码：`CMakeLists.txt`、`cmake/deps.cmake`、`tests/consumer/*`。

## 1. 为什么 SDK 构建边界值得单独记录

pi-cpp 不是只有一个最终可执行程序，它同时提供：

```text
pi::ai
pi::agent
pi::coding-agent
```

这些 target 会被未来的外部工程消费。因此 CMake 的问题不只是“本仓库能不能编译”，还包括：

- public consumer 会继承哪些 include；
- public consumer 会继承哪些 link dependency；
- private runtime DLL 是否被意外传递；
- FetchContent 配置是否污染宿主工程；
- warning policy 是否错误作用到第三方源码；
- SDK header 是否偷偷依赖 private include path。

v0.0.2 在接入 CPR/libcurl 时已经真实暴露过这些问题。

## 2. Target 层次

当前依赖方向：

```text
external app / picpp
        ↓
pi::coding-agent
        ↓
pi::agent
        ↓
pi::ai
```

长期要求：

> dependency 只能沿一个方向向下，底层 SDK 不能反向依赖上层。

具体 target：

```text
pi_ai            → alias pi::ai
pi_agent         → alias pi::agent
pi_coding_agent  → alias pi::coding-agent
```

## 3. PUBLIC 与 PRIVATE 不只是 CMake 语法

`target_link_libraries()` 的传播属性代表 SDK contract。

例如：

```cmake
target_link_libraries(pi_ai
    PUBLIC nlohmann_json
    PRIVATE cpr::cpr
)
```

含义：

### nlohmann_json 是 PUBLIC

因为 public header 中存在：

```text
nlohmann::json
```

consumer 编译 public API 时确实需要它。

### CPR 是 PRIVATE

因为 CPR 只用于 `src/ai/http.cpp` 等实现文件，public header 不应该知道它。

如果把 CPR 改成 PUBLIC，相当于宣布：

```text
CPR 是 pi::ai SDK contract 的一部分
```

这会增加 consumer 的编译、链接和部署耦合。

## 4. Public Header Audit

public boundary：

```text
include/pi/...
```

private boundary：

```text
src/...
```

public header 中不得出现：

```text
#include <cpr/...>
#include <curl/...>
cpr::Response
CURL*
private src/ai header
```

v0.0.2 的 `OpenAICompatibleProvider` 使用 PImpl，就是为了让：

```text
include/pi/ai/openai_compatible.hpp
```

只依赖 public Provider 类型和标准库。

## 5. External Consumer Test 为什么重要

仓库内部 target 往往拥有很多“额外便利”：

```text
src include path
transitive dependency
generated build include
repository working directory
```

所以内部单测通过不能证明 public SDK 真能独立消费。

`tests/consumer/test_ai_sdk.cpp` 的设计目的是：

```text
只 include public header
只 link pi::ai
不手工添加 src/
不手工 link private dependency
```

它能发现：

- public header 漏依赖；
- private header 泄漏；
- private link dependency 误传；
- Windows runtime DLL 问题。

## 6. v0.0.2 的 Windows `cpr.dll` 教训

当 public consumer 第一次真正实例化：

```text
OpenAICompatibleProvider
```

Windows CTest 出现：

```text
0xc0000135
```

构建本身成功，但测试进程启动失败。

原因是：

```text
pi_ai
  → private cpr shared library
  → consumer runtime needs cpr.dll
```

fake transport tests 没有真正构造 CPR transport，因此之前没有暴露。

这个案例说明：

> “PRIVATE link”只描述 CMake usage requirement，不自动解决动态库运行时部署。

SDK consumer test 必须实际运行，不能只编译。

## 7. 为什么当前把 FetchContent dependency 构建为 static

v0.0.2 选择：

```cmake
set(BUILD_SHARED_LIBS OFF)
```

让 CPR/libcurl 等 FetchContent dependency 在当前依赖子树中保持 static，从而：

```text
external pi::ai consumer
    不需要额外部署 private cpr.dll
```

这是当前阶段的实现策略，不等于项目永久禁止 shared library。

未来 pi-cpp 自己支持 shared SDK 时，需要重新设计：

- export/import symbols；
- install layout；
- runtime dependency packaging；
- Windows DLL deployment。

## 8. 不能强写宿主工程 `BUILD_SHARED_LIBS` cache

曾经为了修 Windows runtime 问题使用：

```cmake
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
```

这对 standalone build 有效，但如果别人：

```cmake
add_subdirectory(pi-cpp)
```

会导致 pi-cpp 修改整个宿主工程的 cache policy。

例如宿主本来要求：

```text
BUILD_SHARED_LIBS=ON
```

pi-cpp 不应该替它改成 OFF。

所以 closeout 改为目录作用域 normal variable：

```cmake
set(BUILD_SHARED_LIBS OFF)
```

意图是只影响当前依赖配置上下文，而不 `FORCE` 覆盖父项目 cache。

长期原则：

> SDK 子项目不能为了解决自己的依赖问题，修改宿主工程的全局构建策略。

## 9. FetchContent dependency 配置

当前依赖均 pin 精确 tag：

```text
nlohmann/json v3.11.3
fmt           10.2.1
doctest       v2.4.12
CPR           1.14.2
```

这样可以：

- 保证 CI 可重复；
- 避免 upstream latest 引入突变；
- 让 compatibility/debug 能精确复现。

第三方测试/示例被关闭，避免把依赖自己的测试面引进 pi-cpp gate。

## 10. 为什么第三方依赖要先于项目 warning-as-error

项目启用：

```text
MSVC: /W4 /WX
others: -Wall -Wextra -Werror
```

如果第三方 FetchContent target 被错误继承项目 warning policy，第三方代码的新 warning 可能让 pi-cpp CI 无法构建。

当前依赖在项目 warning option 前引入，并使用 `SYSTEM`，目的是：

```text
我们的代码 warning-as-error
第三方源码不因我们的 warning policy 被强制修复
```

长期规则：

> warning-as-error 应约束项目代码，不应把维护第三方源码的责任隐式接过来。

## 11. Include Boundary

`pi_ai`：

```cmake
target_include_directories(pi_ai
    PUBLIC  include/
    PRIVATE src/
)
```

这意味着：

```text
external consumer
    可以 #include <pi/ai/...>
    不可以 #include "ai/http.hpp"
```

private tests 如果确实需要验证内部 component，可以显式通过专门的 test helper 增加 `src/` include path。

不要因为某个 test include private header，就把 `src/` 改成 PUBLIC。

## 12. INTERFACE coding-agent target

v0.0.1/v0.0.2 的 coding-agent 还没有实现源文件，因此：

```text
pi_coding_agent = INTERFACE
```

但它已经承担 public include boundary 和 dependency direction 的冻结作用。

这让外部 consumer 可以提前按最终依赖图：

```text
link pi::coding-agent
```

而不需要等所有功能实现后才重构 target graph。

## 13. CMake target 名称就是 API 的一部分

长期 public alias：

```text
pi::ai
pi::agent
pi::coding-agent
```

不应该因为内部 target rename 就随意改变。

内部 concrete target：

```text
pi_ai
pi_agent
pi_coding_agent
```

可以承担 implementation detail，但 alias 是外部 CMake consumer contract。

## 14. `cmake --install` 尚未完成意味着什么

v0.0.2 已经有 build-tree consumer test，但尚未完成：

```text
cmake --install
export targets
picppConfig.cmake
find_package(picpp CONFIG REQUIRED)
```

因此当前能证明：

```text
build tree public consumption
```

但不能宣称：

```text
installed SDK consumption 已稳定
```

这部分按路线图留到 v0.1.0。

## 15. v0.1.0 install/export 应延续的原则

以后做 install package 时至少验证：

```text
install headers
install libraries
export pi::ai / pi::agent / pi::coding-agent
find_dependency for true PUBLIC deps
不要 export CPR if still private
clean external project find_package
Windows/Linux/macOS consumer build
```

最好新增完全独立目录的 consumer fixture：

```text
cmake -S consumer -B consumer-build \
      -DCMAKE_PREFIX_PATH=<installed-picpp>
cmake --build consumer-build
ctest ...
```

这样可以发现 build-tree test 看不到的 export/install 问题。

## 16. Add-subdirectory 与 installed package 是两种不同场景

当前尤其要避免：

```text
为了 standalone 方便
→ 强改 cache
→ add_subdirectory 宿主被污染
```

未来还要避免：

```text
build-tree include path 可用
→ 误以为 install 后也可用
```

所以 CMake 验证应分层：

```text
repo internal targets
build-tree public consumer
add_subdirectory host consumer
installed find_package consumer
```

## 17. 修改 SDK CMake 时的 checklist

### Public header / link

```text
新增 public header 类型是否来自新三方库？
如果是，dependency 是否应 PUBLIC？
如果不是，为什么要 PUBLIC link？
```

### Private runtime dependency

```text
consumer 是否需要它的 DLL/SO？
是否应该 static/private？
install package 是否会误 export？
```

### Host isolation

```text
是否 FORCE 修改 cache？
是否改 CMAKE_* 全局变量？
是否修改父目录 warning/compiler policy？
```

### Consumer validation

```text
只 link public target 是否编译？
是否运行成功？
Windows runtime dependency 是否完整？
```

## 18. 设计结论

CMake SDK 边界的长期原则：

1. **PUBLIC/PRIVATE 是 SDK contract，不只是链接语法。**
2. **public header 用到的依赖才有资格成为 public usage requirement。**
3. **private runtime dependency 不能把 DLL 部署负担意外推给 consumer。**
4. **SDK 子项目不能 FORCE 修改宿主工程的全局 cache policy。**
5. **必须用真正的 external-style consumer test 验证 public boundary。**
6. **build-tree 可消费不等于 install/find_package 已完成。**
