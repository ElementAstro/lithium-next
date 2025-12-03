# CMakeLists.txt for server plugin tests
#
# Copyright (C) 2023-2024 Max Qian <lightapt.com>

# Plugin Interface Tests
lithium_add_test(
    NAME test_plugin_interface
    SOURCES test_plugin_interface.cpp
    LIBRARIES lithium_server GTest::gtest GTest::gtest_main
)

# Plugin Loader Tests
lithium_add_test(
    NAME test_plugin_loader
    SOURCES test_plugin_loader.cpp
    LIBRARIES lithium_server GTest::gtest GTest::gtest_main
)

# Plugin Manager Tests
lithium_add_test(
    NAME test_plugin_manager
    SOURCES test_plugin_manager.cpp
    LIBRARIES lithium_server GTest::gtest GTest::gtest_main
)

# Base Plugin Tests
lithium_add_test(
    NAME test_base_plugin
    SOURCES test_base_plugin.cpp
    LIBRARIES lithium_server GTest::gtest GTest::gtest_main
)
