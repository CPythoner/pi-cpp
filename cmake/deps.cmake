include(FetchContent)

# CMake 4 兼容：允许声明 cmake_minimum_required < 3.5 的旧依赖（如 doctest）
set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "")

# 关闭依赖自带的测试与示例，避免拖慢构建
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "")
set(DOCTEST_WITH_EXAMPLES OFF CACHE BOOL "")

# T1：暂只引入 doctest（冒烟测试所需）；T2 扩展为 §5.5 完整三依赖
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.12
    SYSTEM)

FetchContent_MakeAvailable(doctest)
