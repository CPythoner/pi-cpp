# 测试注册：每个测试一个可执行文件。
# 业务类型通过三层 SDK target 获取；src/ 仅作为内部 util 测试的 PRIVATE include 路径。

function(picpp_add_test name src)
    add_executable(${name} "${src}" "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp")
    target_compile_definitions(${name} PRIVATE PI_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(${name} PRIVATE
        "${PROJECT_SOURCE_DIR}/include/pi"
        "${PROJECT_SOURCE_DIR}/src"
    )
    target_link_libraries(${name} PRIVATE doctest pi::coding-agent)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

picpp_add_test(test_smoke "${CMAKE_CURRENT_SOURCE_DIR}/test_smoke.cpp")
picpp_add_test(test_string "${CMAKE_CURRENT_SOURCE_DIR}/util/test_string.cpp")
picpp_add_test(test_cancel_token "${CMAKE_CURRENT_SOURCE_DIR}/util/test_cancel_token.cpp")
picpp_add_test(test_thread_guard "${CMAKE_CURRENT_SOURCE_DIR}/util/test_thread_guard.cpp")
picpp_add_test(test_message_json "${CMAKE_CURRENT_SOURCE_DIR}/agent/test_message_json.cpp")
picpp_add_test(test_events_json "${CMAKE_CURRENT_SOURCE_DIR}/agent/test_events_json.cpp")
picpp_add_test(test_sdk_headers "${CMAKE_CURRENT_SOURCE_DIR}/consumer/test_sdk_headers.cpp")
