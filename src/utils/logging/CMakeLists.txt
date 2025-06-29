# CMakeLists.txt for logging utilities

set(LITHIUM_LOGGING_SOURCES
    spdlog_config.cpp
)

set(LITHIUM_LOGGING_HEADERS
    spdlog_config.hpp
)

# Find required packages
find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)

# Create logging library
add_library(lithium_logging STATIC ${LITHIUM_LOGGING_SOURCES})

target_include_directories(lithium_logging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_logging PUBLIC
    spdlog::spdlog
    Threads::Threads
    atom
)

# Set C++23 standard
target_compile_features(lithium_logging PUBLIC cxx_std_23)

# Enable modern C++ optimizations
target_compile_options(lithium_logging PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O3 -march=native>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /O2>
)

# Add alias for convenience
add_library(lithium::logging ALIAS lithium_logging)

# Install headers
install(FILES ${LITHIUM_LOGGING_HEADERS}
    DESTINATION include/lithium/utils/logging
)

# Install library
install(TARGETS lithium_logging
    EXPORT lithium-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
