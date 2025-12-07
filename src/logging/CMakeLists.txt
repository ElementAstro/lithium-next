# Logging Module CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(lithium_logging LANGUAGES CXX)

# Source files
set(LOGGING_SOURCES
    # Core module
    core/types.cpp
    core/logger_registry.cpp
    core/logging_manager.cpp
    # Sinks module
    sinks/ring_buffer_sink.cpp
    sinks/sink_factory.cpp
    # Utils module
    utils/log_statistics.cpp
    utils/log_exporter.cpp
)

# Header files
set(LOGGING_HEADERS
    # Main entry point
    logging.hpp
    # Core module
    core/core.hpp
    core/types.hpp
    core/logger_registry.hpp
    core/logging_manager.hpp
    # Sinks module
    sinks/sinks.hpp
    sinks/ring_buffer_sink.hpp
    sinks/sink_factory.hpp
    # Utils module
    utils/utils.hpp
    utils/log_statistics.hpp
    utils/log_exporter.hpp
)

# Create logging library
add_library(lithium_logging STATIC ${LOGGING_SOURCES} ${LOGGING_HEADERS})

# Include directories
target_include_directories(lithium_logging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link dependencies
target_link_libraries(lithium_logging
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Set C++ standard
target_compile_features(lithium_logging PUBLIC cxx_std_20)

# Add alias for namespace support
add_library(lithium::logging ALIAS lithium_logging)

# Install headers
install(FILES logging.hpp
    DESTINATION include/lithium/logging
)

install(FILES
    core/core.hpp
    core/types.hpp
    core/logger_registry.hpp
    core/logging_manager.hpp
    DESTINATION include/lithium/logging/core
)

install(FILES
    sinks/sinks.hpp
    sinks/ring_buffer_sink.hpp
    sinks/sink_factory.hpp
    DESTINATION include/lithium/logging/sinks
)

install(FILES
    utils/utils.hpp
    utils/log_statistics.hpp
    utils/log_exporter.hpp
    DESTINATION include/lithium/logging/utils
)
