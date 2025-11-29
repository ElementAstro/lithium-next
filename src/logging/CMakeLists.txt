# Logging Module CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(lithium_logging LANGUAGES CXX)

# Source files
set(LOGGING_SOURCES
    types.cpp
    logger_registry.cpp
    logging_manager.cpp
    log_statistics.cpp
    log_exporter.cpp
    sinks/ring_buffer_sink.cpp
    sinks/sink_factory.cpp
)

# Header files
set(LOGGING_HEADERS
    types.hpp
    logger_registry.hpp
    logging_manager.hpp
    log_statistics.hpp
    log_exporter.hpp
    logging.hpp
    sinks/ring_buffer_sink.hpp
    sinks/sink_factory.hpp
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
install(FILES ${LOGGING_HEADERS}
    DESTINATION include/lithium/logging
)

install(FILES 
    sinks/ring_buffer_sink.hpp
    sinks/sink_factory.hpp
    DESTINATION include/lithium/logging/sinks
)
