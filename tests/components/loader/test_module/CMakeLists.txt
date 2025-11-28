cmake_minimum_required(VERSION 3.20)

project(test_module LANGUAGES CXX)

# Create shared library for testing
add_library(${PROJECT_NAME} SHARED
    test_module.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        atom
        nlohmann_json::nlohmann_json
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Set output name based on platform
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "test_module"
    PREFIX ""  # Remove lib prefix on Unix
)

# On Windows, also remove any extra prefixes/suffixes
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".dll"
    )
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".so"
    )
endif()
