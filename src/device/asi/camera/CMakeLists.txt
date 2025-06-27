cmake_minimum_required(VERSION 3.20)

# ASI Camera module
project(lithium_asi_camera LANGUAGES CXX)

# Add components subdirectory
add_subdirectory(components)

set(ASI_CAMERA_SOURCES
    main.hpp
    main.cpp
    controller.hpp
    controller.cpp
    controller_impl.hpp
)

# Create shared library
add_library(asi_camera SHARED ${ASI_CAMERA_SOURCES})
set_property(TARGET asi_camera PROPERTY POSITION_INDEPENDENT_CODE 1)

# Target properties
target_compile_features(asi_camera PRIVATE cxx_std_20)
target_compile_options(asi_camera PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Find and link ASI Camera SDK if available
find_library(ASI_CAMERA_LIBRARY 
    NAMES ASICamera2 libASICamera2
    PATHS
        /usr/local/lib
        /usr/lib
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/lib
    DOC "ASI Camera SDK library"
)

if(ASI_CAMERA_LIBRARY)
    message(STATUS "Found ASI Camera SDK: ${ASI_CAMERA_LIBRARY}")
    add_compile_definitions(LITHIUM_ASI_CAMERA_ENABLED)
    target_link_libraries(asi_camera PRIVATE ${ASI_CAMERA_LIBRARY})
    
    # Find ASI Camera headers
    find_path(ASI_CAMERA_INCLUDE_DIR
        NAMES ASICamera2.h
        PATHS
            /usr/local/include
            /usr/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/include
        DOC "ASI Camera SDK include directory"
    )
    
    if(ASI_CAMERA_INCLUDE_DIR)
        target_include_directories(asi_camera PRIVATE ${ASI_CAMERA_INCLUDE_DIR})
    endif()
else()
    message(STATUS "ASI Camera SDK not found, using stub implementation")
    add_compile_definitions(LITHIUM_ASI_CAMERA_STUB)
endif()

# Link common libraries
target_link_libraries(asi_camera PUBLIC
    loguru
    atom-system
    atom-io
    atom-utils
    atom-component
    atom-error
    atom-type
    asi_camera_components
)

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(asi_camera PRIVATE Threads::Threads)

# Installation
install(TARGETS asi_camera
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES
    main.hpp
    controller.hpp
    controller_impl.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/asi/camera
)
