# CMakeLists.txt for tools module
# Part of Lithium-Next Script System

set(TOOLS_SOURCES
    tool_info.cpp
    invocation.cpp
    tool_registry.cpp
)

set(TOOLS_HEADERS
    types.hpp
    tool_info.hpp
    invocation.hpp
    tool_registry.hpp
)

# Create library
add_library(lithium_script_tools STATIC ${TOOLS_SOURCES} ${TOOLS_HEADERS})

target_include_directories(lithium_script_tools
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(lithium_script_tools
    PUBLIC
        nlohmann_json::nlohmann_json
        pybind11::embed
    PRIVATE
        atom_log
)

# C++20 required
target_compile_features(lithium_script_tools PUBLIC cxx_std_20)

# Set properties
set_target_properties(lithium_script_tools PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Export headers for IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${TOOLS_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${TOOLS_SOURCES})
