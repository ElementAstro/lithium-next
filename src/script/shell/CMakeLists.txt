# CMakeLists.txt for shell module
# Part of Lithium-Next Script System

set(SHELL_SOURCES
    # Core types and manager
    script_manager.cpp

    # Execution engines
    execution/executor.cpp
    execution/shell_executor.cpp
    execution/powershell_executor.cpp
    execution/python_executor.cpp

    # Auxiliary modules
    hooks.cpp
    metadata.cpp
    resource_limits.cpp
    retry.cpp
    versioning.cpp
)

set(SHELL_HEADERS
    # Core types and manager
    types.hpp
    script_manager.hpp

    # Execution engines
    execution/executor.hpp
    execution/shell_executor.hpp
    execution/powershell_executor.hpp
    execution/python_executor.hpp

    # Auxiliary modules
    hooks.hpp
    metadata.hpp
    resource_limits.hpp
    retry.hpp
    versioning.hpp
)

# Create shell library
add_library(lithium_script_shell STATIC ${SHELL_SOURCES} ${SHELL_HEADERS})

target_include_directories(lithium_script_shell
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(lithium_script_shell
    PUBLIC
        spdlog::spdlog
    PRIVATE
        atom_system
)

# C++20 required
target_compile_features(lithium_script_shell PUBLIC cxx_std_20)

# Set properties
set_target_properties(lithium_script_shell PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Export headers for IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${SHELL_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SHELL_SOURCES})
