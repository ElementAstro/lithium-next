cmake_minimum_required(VERSION 3.20)

# Project name and version, using C and C++ languages
project(lithium_config VERSION 1.0.0 LANGUAGES C CXX)

# Project sources and headers
set(PROJECT_SOURCES 
    configor.cpp
    config_cache.cpp
    config_validator.cpp
    config_serializer.cpp
    config_watcher.cpp
)

set(PROJECT_HEADERS 
    configor.hpp
    config_cache.hpp
    config_validator.hpp
    config_serializer.hpp
    config_watcher.hpp
    configor_macro.hpp
    json5.hpp
)

set(PROJECT_LIBS ${CMAKE_THREAD_LIBS_INIT} atom)

if (WIN32)
    list(APPEND PROJECT_LIBS ws2_32)
endif()

# Create static library
add_library(${PROJECT_NAME} STATIC ${PROJECT_SOURCES} ${PROJECT_HEADERS})

# Set static library properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}  # Version number
    SOVERSION 1                 # Compatibility version
    OUTPUT_NAME ${PROJECT_NAME} # Output name
    POSITION_INDEPENDENT_CODE ON # Position independent code
)

# Include directories so that project headers can be included
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries required by the project
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBS})

# Install target to install the static library to a specified location
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
