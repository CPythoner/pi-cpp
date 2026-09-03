# Public behavior tests consume canonical SDK headers/targets. Private util tests may access src/ support headers.

function(picpp_add_test name src sdk_target)
    add_executable(${name} "${src}" "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp")
    target_compile_definitions(${name} PRIVATE PI_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${name} PRIVATE doctest ${sdk_target})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

function(picpp_add_private_test name src sdk_target)
    add_executable(${name} "${src}" "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp")
    target_compile_definitions(${name} PRIVATE PI_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(${name} PRIVATE doctest ${sdk_target})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

function(picpp_add_sdk_consumer_test name src sdk_target)
    add_executable(${name} "${src}" "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp")
    target_link_libraries(${name} PRIVATE doctest ${sdk_target})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

picpp_add_test(test_smoke "${CMAKE_CURRENT_SOURCE_DIR}/test_smoke.cpp" pi::ai)
picpp_add_private_test(test_string "${CMAKE_CURRENT_SOURCE_DIR}/util/test_string.cpp" pi::ai)
picpp_add_test(test_cancel_token "${CMAKE_CURRENT_SOURCE_DIR}/util/test_cancel_token.cpp" pi::ai)
picpp_add_test(test_event_stream "${CMAKE_CURRENT_SOURCE_DIR}/ai/test_event_stream.cpp" pi::ai)
picpp_add_test(test_provider "${CMAKE_CURRENT_SOURCE_DIR}/ai/test_provider.cpp" pi::ai)
picpp_add_private_test(test_sse_parser "${CMAKE_CURRENT_SOURCE_DIR}/ai/test_sse_parser.cpp" pi::ai)
picpp_add_private_test(test_streaming_json "${CMAKE_CURRENT_SOURCE_DIR}/ai/test_streaming_json.cpp" pi::ai)
picpp_add_private_test(test_openai_request "${CMAKE_CURRENT_SOURCE_DIR}/ai/test_openai_request.cpp" pi::ai)
picpp_add_private_test(test_openai_compatible "${CMAKE_CURRENT_SOURCE_DIR}/ai/test_openai_compatible.cpp" pi::ai)
picpp_add_private_test(test_openai_tool_streaming "${CMAKE_CURRENT_SOURCE_DIR}/ai/test_openai_tool_streaming.cpp" pi::ai)
picpp_add_private_test(test_openai_provider "${CMAKE_CURRENT_SOURCE_DIR}/ai/test_openai_provider.cpp" pi::ai)
picpp_add_private_test(test_retry "${CMAKE_CURRENT_SOURCE_DIR}/ai/test_retry.cpp" pi::ai)
picpp_add_private_test(test_thread_guard "${CMAKE_CURRENT_SOURCE_DIR}/util/test_thread_guard.cpp" pi::ai)
picpp_add_test(test_message_json "${CMAKE_CURRENT_SOURCE_DIR}/agent/test_message_json.cpp" pi::agent)
picpp_add_test(test_events_json "${CMAKE_CURRENT_SOURCE_DIR}/agent/test_events_json.cpp" pi::agent)

picpp_add_sdk_consumer_test(
    test_ai_sdk
    "${CMAKE_CURRENT_SOURCE_DIR}/consumer/test_ai_sdk.cpp"
    pi::ai
)
picpp_add_sdk_consumer_test(
    test_agent_sdk
    "${CMAKE_CURRENT_SOURCE_DIR}/consumer/test_agent_sdk.cpp"
    pi::agent
)
picpp_add_sdk_consumer_test(
    test_coding_agent_sdk
    "${CMAKE_CURRENT_SOURCE_DIR}/consumer/test_coding_agent_sdk.cpp"
    pi::coding-agent
)

add_test(NAME picpp_version COMMAND $<TARGET_FILE:picpp> --version)
set_tests_properties(picpp_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^picpp v0\\.0\\.2"
)
