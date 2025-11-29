# CMakeLists.txt for venv module
# Part of Lithium-Next Script System

set(VENV_SOURCES
    process_utils.cpp
    package_manager.cpp
    conda_adapter.cpp
    venv_manager.cpp
)

set(VENV_HEADERS
    types.hpp
    process_utils.hpp
    package_manager.hpp
    conda_adapter.hpp
    venv_manager.hpp
)

# Create library
add_library(lithium_script_venv STATIC ${VENV_SOURCES} ${VENV_HEADERS})

target_include_directories(lithium_script_venv
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_script_venv
    PUBLIC
        spdlog::spdlog
)

# C++20 required
target_compile_features(lithium_script_venv PUBLIC cxx_std_20)

# Set properties
set_target_properties(lithium_script_venv PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Export headers for IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${VENV_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${VENV_SOURCES})
