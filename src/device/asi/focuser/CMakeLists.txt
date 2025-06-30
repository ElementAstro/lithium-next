cmake_minimum_required(VERSION 3.20)

# ASI Focuser module
project(lithium_asi_focuser LANGUAGES CXX)

# Add components subdirectory
add_subdirectory(components)

set(ASI_FOCUSER_SOURCES
    asi_focuser.hpp
    asi_focuser.cpp
    eaf_sdk_stub.hpp
    controller/asi_focuser_controller.hpp
    controller/asi_focuser_controller.cpp
    controller/asi_focuser_controller_v2.hpp
    controller/asi_focuser_controller_v2.cpp
    controller/controller_factory.hpp
    controller/controller_factory.cpp
)

# Create shared library
add_library(asi_focuser SHARED ${ASI_FOCUSER_SOURCES})
set_property(TARGET asi_focuser PROPERTY POSITION_INDEPENDENT_CODE 1)

# Target properties
target_compile_features(asi_focuser PRIVATE cxx_std_20)
target_compile_options(asi_focuser PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Find and link ASI EAF SDK if available
find_library(ASI_EAF_LIBRARY 
    NAMES EAF_focuser libEAF_focuser
    PATHS
        /usr/local/lib
        /usr/lib
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/lib
    DOC "ASI EAF SDK library"
)

if(ASI_EAF_LIBRARY)
    message(STATUS "Found ASI EAF SDK: ${ASI_EAF_LIBRARY}")
    add_compile_definitions(LITHIUM_ASI_EAF_ENABLED)
    target_link_libraries(asi_focuser PRIVATE ${ASI_EAF_LIBRARY})
    
    # Find EAF headers
    find_path(ASI_EAF_INCLUDE_DIR
        NAMES EAF_focuser.h
        PATHS
            /usr/local/include
            /usr/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/include
        DOC "ASI EAF SDK headers"
    )
    
    if(ASI_EAF_INCLUDE_DIR)
        target_include_directories(asi_focuser PRIVATE ${ASI_EAF_INCLUDE_DIR})
    endif()
else()
    message(STATUS "ASI EAF SDK not found, using stub implementation")
endif()

# Link common libraries
target_link_libraries(asi_focuser PUBLIC
    asi_focuser_components  # Link our components library
    atom
    atom
    pthread
)

# Include directories
target_include_directories(asi_focuser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:include>
)

# Installation
install(TARGETS asi_focuser
    EXPORT asi_focuser_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/device/asi/focuser
)

install(FILES asi_focuser.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/device/asi/focuser
)

install(EXPORT asi_focuser_targets
    FILE asi_focuser_targets.cmake
    NAMESPACE lithium::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lithium
)
