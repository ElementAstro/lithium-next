# tests/device/phd2/CMakeLists.txt

cmake_minimum_required(VERSION 3.20)

# PHD2 Device Adapter Tests
add_executable(test_phd2_device_adapter
    test_phd2_client.cpp
)

target_link_libraries(test_phd2_device_adapter
    PRIVATE
        lithium.client.phd2
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
)

target_include_directories(test_phd2_device_adapter
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs/atom
)

add_test(NAME test_phd2_device_adapter COMMAND test_phd2_device_adapter)

# Set test properties
set_tests_properties(test_phd2_device_adapter PROPERTIES
    LABELS "device;phd2;guider"
    TIMEOUT 60
)
