# CMakeLists.txt for server tests
#
# Copyright (C) 2023-2024 Max Qian <lightapt.com>

cmake_minimum_required(VERSION 3.20)

# Find required packages
find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/server
)

# ============================================================================
# Core Server Tests
# ============================================================================

# Test: Command Dispatcher
add_executable(test_command
    test_command.cpp
)
target_link_libraries(test_command
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_server
        spdlog::spdlog
)
add_test(NAME ServerCommandTest COMMAND test_command)

# Test: Event Loop
add_executable(test_eventloop
    test_eventloop.cpp
)
target_link_libraries(test_eventloop
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_server
        spdlog::spdlog
)
add_test(NAME ServerEventLoopTest COMMAND test_eventloop)

# Test: Rate Limiter
add_executable(test_rate_limiter
    test_rate_limiter.cpp
)
target_link_libraries(test_rate_limiter
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_server
        spdlog::spdlog
)
add_test(NAME ServerRateLimiterTest COMMAND test_rate_limiter)

# Test: Task Manager
add_executable(test_task_manager
    test_task_manager.cpp
)
target_link_libraries(test_task_manager
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_server
        spdlog::spdlog
)
add_test(NAME ServerTaskManagerTest COMMAND test_task_manager)

# Test: WebSocket Server
add_executable(test_websocket
    test_websocket.cpp
)
target_link_libraries(test_websocket
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        lithium_server
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerWebSocketTest COMMAND test_websocket)

# Test: Device Manager Commands
add_executable(test_device_manager_commands
    test_device_manager_commands.cpp
)
target_link_libraries(test_device_manager_commands
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_server
        spdlog::spdlog
)
add_test(NAME ServerDeviceManagerCommandsTest COMMAND test_device_manager_commands)

# ============================================================================
# Middleware Tests
# ============================================================================

# Test: Auth Middleware
add_executable(test_middleware_auth
    middleware/test_auth.cpp
)
target_link_libraries(test_middleware_auth
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_server
        spdlog::spdlog
)
add_test(NAME ServerMiddlewareAuthTest COMMAND test_middleware_auth)

# Test: Image Middleware
add_executable(test_middleware_image
    middleware/test_image.cpp
)
target_link_libraries(test_middleware_image
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        spdlog::spdlog
)
add_test(NAME ServerMiddlewareImageTest COMMAND test_middleware_image)

# ============================================================================
# Models Tests
# ============================================================================

# Test: API Models
add_executable(test_models_api
    models/test_api.cpp
)
target_link_libraries(test_models_api
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_server
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerModelsApiTest COMMAND test_models_api)

# ============================================================================
# Utils Tests
# ============================================================================

# Test: Response Builder
add_executable(test_utils_response
    utils/test_response.cpp
)
target_link_libraries(test_utils_response
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_server
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerUtilsResponseTest COMMAND test_utils_response)

# ============================================================================
# Command Tests
# ============================================================================

# Test: Command Response
add_executable(test_command_response
    command/test_response.cpp
)
target_link_libraries(test_command_response
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerCommandResponseTest COMMAND test_command_response)

# ============================================================================
# Controller Tests
# ============================================================================

# Test: Logging Controller
add_executable(test_controller_logging
    controller/test_logging_controller.cpp
)
target_link_libraries(test_controller_logging
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        lithium_server
        spdlog::spdlog
)
add_test(NAME ServerControllerLoggingTest COMMAND test_controller_logging)

# Test: System Config Controller
add_executable(test_controller_system_config
    controller/test_system_config.cpp
)
target_link_libraries(test_controller_system_config
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerControllerSystemConfigTest COMMAND test_controller_system_config)

# Test: Server Status Controller
add_executable(test_controller_server_status
    controller/test_server_status.cpp
)
target_link_libraries(test_controller_server_status
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerControllerServerStatusTest COMMAND test_controller_server_status)

# Test: Device Controller
add_executable(test_controller_device
    controller/test_device_controller.cpp
)
target_link_libraries(test_controller_device
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerControllerDeviceTest COMMAND test_controller_device)

# Test: Script Controller
add_executable(test_controller_script
    controller/test_script_controller.cpp
)
target_link_libraries(test_controller_script
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerControllerScriptTest COMMAND test_controller_script)

# Test: Sequencer Controller
add_executable(test_controller_sequencer
    controller/test_sequencer_controller.cpp
)
target_link_libraries(test_controller_sequencer
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerControllerSequencerTest COMMAND test_controller_sequencer)

# ============================================================================
# WebSocket Tests
# ============================================================================

# Test: Log Stream
add_executable(test_websocket_log_stream
    websocket/test_log_stream.cpp
)
target_link_libraries(test_websocket_log_stream
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        lithium_server
        lithium_logging
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)
add_test(NAME ServerWebSocketLogStreamTest COMMAND test_websocket_log_stream)

# ============================================================================
# Plugin Tests
# ============================================================================

add_subdirectory(plugin)

# ============================================================================
# Custom Target for All Server Tests
# ============================================================================

add_custom_target(server_tests
    DEPENDS
        test_command
        test_eventloop
        test_rate_limiter
        test_task_manager
        test_websocket
        test_device_manager_commands
        test_middleware_auth
        test_middleware_image
        test_models_api
        test_utils_response
        test_command_response
        test_controller_logging
        test_controller_system_config
        test_controller_server_status
        test_controller_device
        test_controller_script
        test_controller_sequencer
        test_websocket_log_stream
)
