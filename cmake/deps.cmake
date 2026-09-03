include(FetchContent)

# CMake 4 兼容：允许声明 cmake_minimum_required < 3.5 的旧依赖（如 doctest）
set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "")

# FetchContent dependencies are implementation details of the SDK. Keep them
# static so an external pi::ai consumer does not inherit private CPR/libcurl
# runtime DLL deployment requirements on Windows. Use a directory-scoped normal
# variable instead of forcing the parent project's BUILD_SHARED_LIBS cache.
set(BUILD_SHARED_LIBS OFF)

# 关闭依赖自带的测试与示例，避免拖慢构建
set(FMT_TEST OFF CACHE BOOL "")
set(FMT_DOC OFF CACHE BOOL "")
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "")
set(DOCTEST_WITH_EXAMPLES OFF CACHE BOOL "")
set(CPR_BUILD_TESTS OFF CACHE BOOL "")
set(CPR_BUILD_TESTS_SSL OFF CACHE BOOL "")
set(CPR_BUILD_TESTS_PROXY OFF CACHE BOOL "")
set(CPR_CURL_NOSIGNAL ON CACHE BOOL "")
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "")
set(CPR_ENABLE_CURL_HTTP_ONLY ON CACHE BOOL "")
set(CPR_CURL_USE_LIBPSL OFF CACHE BOOL "")
set(CURL_ZLIB OFF CACHE BOOL "")

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
FetchContent_Declare(cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG        1.14.2
    SYSTEM)

FetchContent_MakeAvailable(json fmt doctest cpr)
