cmake_minimum_required(VERSION 3.25)

project(picpp VERSION 0.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 依赖先于警告选项引入（FetchContent 的子目录不继承本目录的 -Werror）
include(cmake/deps.cmake)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

configure_file(cmake/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h" @ONLY)

# ai SDK：LLM message/content、L1 provider events，以及 v0.0.2 起的 Provider/EventStream。
add_library(pi_ai STATIC
    src/ai/message.cpp
    src/ai/events.cpp
    src/ai/sse_parser.cpp
    src/ai/openai_request.cpp
    src/ai/openai_compatible.cpp
    src/ai/openai_provider.cpp
    src/ai/http.cpp
    src/util/string.cpp
)
add_library(pi::ai ALIAS pi_ai)
target_include_directories(pi_ai
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(pi_ai
    PUBLIC nlohmann_json
    PRIVATE cpr::cpr
)

# agent SDK：AgentMessage、L2 AgentEvent；v0.0.3 起加入 Agent/runLoop/tool abstraction。
add_library(pi_agent STATIC
    src/agent/message.cpp
    src/agent/events.cpp
)
add_library(pi::agent ALIAS pi_agent)
target_include_directories(pi_agent
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(pi_agent PUBLIC pi::ai)

# coding-agent SDK：v0.0.4 起加入 coding tools；v0.0.1 已冻结依赖与 public include 边界。
add_library(pi_coding_agent INTERFACE)
add_library(pi::coding-agent ALIAS pi_coding_agent)
target_include_directories(pi_coding_agent
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(pi_coding_agent INTERFACE pi::agent)

add_executable(picpp apps/picpp/main.cpp)
target_include_directories(picpp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(picpp PRIVATE pi::coding-agent fmt::fmt)

enable_testing()
add_subdirectory(tests)
