# ============================================================================
# CMakeLists.txt for IPC Tests
# ============================================================================

set(IPC_TEST_SOURCES
    test_message_types.cpp
    test_message.cpp
    test_serializer.cpp
    test_channel.cpp
)

add_library(lithium_script_ipc_tests OBJECT ${IPC_TEST_SOURCES})

target_include_directories(lithium_script_ipc_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_script_ipc_tests PRIVATE
    GTest::gtest
    lithium_script
)
