# ASI Camera Modular Implementation

cmake_minimum_required(VERSION 3.20)

# Add components subdirectory
add_subdirectory(components)

# Create the ASI camera library
add_library(
    lithium_device_asi_camera STATIC
    # Main files
    main.cpp
    controller.cpp
    # Headers
    main.hpp
    controller.hpp
    controller_impl.hpp
)

# Set properties
set_property(TARGET lithium_device_asi_camera PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_asi_camera PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_asi_camera
)

# Find and link ASI Camera SDK
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

    # Find ASI Camera headers
    find_path(ASI_CAMERA_INCLUDE_DIR
        NAMES ASICamera2.h
        PATHS
            /usr/local/include
            /usr/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/include
    )

    if(ASI_CAMERA_INCLUDE_DIR)
        target_include_directories(lithium_device_asi_camera PRIVATE ${ASI_CAMERA_INCLUDE_DIR})
    endif()

    target_link_libraries(lithium_device_asi_camera PRIVATE ${ASI_CAMERA_LIBRARY})
endif()

# Include directories
target_include_directories(
    lithium_device_asi_camera
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
           ${CMAKE_CURRENT_SOURCE_DIR}/../..
           ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# Link dependencies
target_link_libraries(
    lithium_device_asi_camera
    PUBLIC lithium_device_template
           atom
    PRIVATE lithium_atom_log
            lithium_atom_type
            asi_camera_components
)

# Install the camera library
install(
    TARGETS lithium_device_asi_camera
    EXPORT lithium_device_asi_camera_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(
    FILES controller.hpp
          main.hpp
          controller_impl.hpp
    DESTINATION include/lithium/device/asi/camera
)
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
