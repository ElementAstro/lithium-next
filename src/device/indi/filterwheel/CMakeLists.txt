# FilterWheel INDI Component Module

set(FILTERWHEEL_INDI_SOURCES
    base.cpp
    control.cpp
    filter_manager.cpp
    statistics.cpp
    configuration.cpp
    filterwheel.cpp
    module.cpp
)

set(FILTERWHEEL_INDI_HEADERS
    base.hpp
    control.hpp
    filter_manager.hpp
    statistics.hpp
    configuration.hpp
    filterwheel.hpp
)

# Create the filterwheel INDI library
add_library(filterwheel_indi_lib STATIC ${FILTERWHEEL_INDI_SOURCES} ${FILTERWHEEL_INDI_HEADERS})

target_include_directories(filterwheel_indi_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(filterwheel_indi_lib
    ${INDI_LIBRARIES}
    atom-component
    atom-utils
    spdlog::spdlog
    Threads::Threads
)

# Compiler flags
target_compile_features(filterwheel_indi_lib PUBLIC cxx_std_20)
target_compile_options(filterwheel_indi_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Set properties
set_target_properties(filterwheel_indi_lib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Optional: Build example executable
option(BUILD_FILTERWHEEL_EXAMPLE "Build filterwheel example" OFF)
if(BUILD_FILTERWHEEL_EXAMPLE)
    add_executable(filterwheel_example example.cpp)
    target_link_libraries(filterwheel_example filterwheel_indi_lib)
    target_compile_features(filterwheel_example PUBLIC cxx_std_20)
endif()

# Add to parent scope for linking
set(FILTERWHEEL_INDI_LIBRARY filterwheel_indi_lib PARENT_SCOPE)
