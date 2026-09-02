# 测试注册：每个测试一个可执行文件，统一链接 doctest 与 pi_types。
# test_main.cpp 提供 doctest 主函数（DOCTEST_CONFIG_IMPLEMENT）。

function(picpp_add_test name src)
    add_executable(${name} "${src}" "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp")
    target_link_libraries(${name} PRIVATE doctest pi_types)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

picpp_add_test(test_smoke "${CMAKE_CURRENT_SOURCE_DIR}/test_smoke.cpp")
