# 测试注册：每个测试一个可执行文件，统一链接 doctest 与 pi_types。
# test_main.cpp 提供 doctest 主函数（DOCTEST_CONFIG_IMPLEMENT）。

function(picpp_add_test name src)
    add_executable(${name} "${src}" "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp")
    # 源码目录宏（fixtures 等测试数据的定位）
    target_compile_definitions(${name} PRIVATE PI_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${name} PRIVATE doctest pi_types)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

picpp_add_test(test_smoke "${CMAKE_CURRENT_SOURCE_DIR}/test_smoke.cpp")
picpp_add_test(test_string "${CMAKE_CURRENT_SOURCE_DIR}/util/test_string.cpp")
picpp_add_test(test_cancel_token "${CMAKE_CURRENT_SOURCE_DIR}/util/test_cancel_token.cpp")
picpp_add_test(test_thread_guard "${CMAKE_CURRENT_SOURCE_DIR}/util/test_thread_guard.cpp")
picpp_add_test(test_message_json "${CMAKE_CURRENT_SOURCE_DIR}/agent/test_message_json.cpp")
