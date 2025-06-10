cmake_minimum_required(VERSION 3.15)
project(AstrometryNetClient VERSION 1.0.0 LANGUAGES CXX)

# Library target
add_library(astrometry_client 
    client.cpp
    utils.cpp
)

# Include directories
target_include_directories(astrometry_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(astrometry_client
    PUBLIC
        CURL::libcurl
        spdlog::spdlog
        Threads::Threads
)

# Example application
add_executable(astrometry_example examples/astrometry_example.cpp)
target_link_libraries(astrometry_example PRIVATE astrometry_client)

# Unit tests
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    find_package(Catch2 REQUIRED)
    add_executable(astrometry_tests tests/test_astrometry_client.cpp)
    target_link_libraries(astrometry_tests PRIVATE astrometry_client Catch2::Catch2)
    include(CTest)
    include(Catch)
    catch_discover_tests(astrometry_tests)
endif()