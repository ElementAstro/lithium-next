# tests/device/phd2/CMakeLists.txt

cmake_minimum_required(VERSION 3.20)

# PHD2 Client Tests
add_executable(test_phd2_client
    test_phd2_client.cpp
)

target_link_libraries(test_phd2_client
    PRIVATE
        lithium.client.phd2
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
)

target_include_directories(test_phd2_client
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs/atom
)

add_test(NAME test_phd2_client COMMAND test_phd2_client)

# Set test properties
set_tests_properties(test_phd2_client PROPERTIES
    LABELS "client;phd2;guider"
    TIMEOUT 60
)
