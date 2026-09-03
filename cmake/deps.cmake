include(FetchContent)

# CMake 4 兼容：允许声明 cmake_minimum_required < 3.5 的旧依赖（如 doctest）
set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "")

# 关闭依赖自带的测试与示例，避免拖慢构建
set(FMT_TEST OFF CACHE BOOL "")
set(FMT_DOC OFF CACHE BOOL "")
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "")
set(DOCTEST_WITH_EXAMPLES OFF CACHE BOOL "")

# Declare 全部前置 → 统一 MakeAvailable；pin 精确 tag；SYSTEM 隔离依赖警告
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    SYSTEM)
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        10.2.1
    SYSTEM)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.12
    SYSTEM)

FetchContent_MakeAvailable(json fmt doctest)
