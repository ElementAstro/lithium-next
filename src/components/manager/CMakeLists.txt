# Component Manager Library
# This library contains the core component management functionality

set(COMPONENT_MANAGER_SOURCES
    manager.cpp
    manager_impl.cpp
)

set(COMPONENT_MANAGER_HEADERS
    manager.hpp
    manager_impl.hpp
    types.hpp
    exceptions.hpp
)

# Create the component manager library (name matches parent CMakeLists.txt reference)
add_library(lithium_components_manager STATIC
    ${COMPONENT_MANAGER_SOURCES}
    ${COMPONENT_MANAGER_HEADERS}
)

# Set target properties
target_include_directories(lithium_components_manager
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs/atom
)

# Link required libraries
target_link_libraries(lithium_components_manager
    PUBLIC
        atom
        loguru
        nlohmann_json::nlohmann_json
)

# Set compiler features
target_compile_features(lithium_components_manager PUBLIC cxx_std_20)

# Add alias for better namespace support
add_library(lithium::components_manager ALIAS lithium_components_manager)
