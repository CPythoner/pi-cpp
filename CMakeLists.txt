cmake_minimum_required(VERSION 3.25)

project(picpp VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 依赖先于警告选项引入（FetchContent 的子目录不继承本目录的 -Werror）
include(cmake/deps.cmake)

# 警告即错误（依赖目录已在上方引入，不受影响）
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# 生成的 version.h（含 project 版本号）
configure_file(cmake/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h" @ONLY)

# 类型/基础设施库（v0.0.1 阶段源列表暂空，故先用 INTERFACE；T4 起填充源码后转 STATIC）
add_library(pi_types INTERFACE)
target_include_directories(pi_types INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(pi_types INTERFACE nlohmann::nlohmann_json)

add_executable(picpp src/cli/main.cpp)
target_link_libraries(picpp PRIVATE pi_types fmt::fmt)

enable_testing()
add_subdirectory(tests)
