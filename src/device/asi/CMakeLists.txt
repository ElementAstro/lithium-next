# ASI Camera Device Implementation
cmake_minimum_required(VERSION 3.20)

# Find ASI SDK
find_path(ASI_INCLUDE_DIR ASICamera2.h
    HINTS
    ${ASI_ROOT_DIR}/include
    ${ASI_ROOT_DIR}
    /usr/local/include
    /usr/include
    PATH_SUFFIXES asi zwo ASI
)

find_library(ASI_LIBRARY
    NAMES ASICamera2 libASICamera2
    HINTS
    ${ASI_ROOT_DIR}/lib
    ${ASI_ROOT_DIR}
    /usr/local/lib
    /usr/lib
    PATH_SUFFIXES x86_64 x64 lib64 armv6 armv7 armv8
)

if(ASI_INCLUDE_DIR AND ASI_LIBRARY)
    set(ASI_FOUND TRUE)
    message(STATUS "Found ASI SDK: ${ASI_LIBRARY}")
else()
    set(ASI_FOUND FALSE)
    message(WARNING "ASI SDK not found. ASI camera support will be disabled.")
endif()

# ASI Camera Implementation
if(ASI_FOUND)
    add_library(lithium_asi_camera STATIC
        camera/asi_camera.cpp
    )

    target_include_directories(lithium_asi_camera
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${ASI_INCLUDE_DIR}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(lithium_asi_camera
        PUBLIC
            lithium_device_template
            atom::atom
            ${ASI_LIBRARY}
        PRIVATE
            pthread
            ${CMAKE_DL_LIBS}
    )

    target_compile_features(lithium_asi_camera PUBLIC cxx_std_20)

    # Set compile definitions
    target_compile_definitions(lithium_asi_camera
        PRIVATE
            LITHIUM_ASI_CAMERA_ENABLED=1
    )

    # Platform-specific settings
    if(UNIX AND NOT APPLE)
        target_link_libraries(lithium_asi_camera PRIVATE udev)
    endif()

    # Add to main device library
    target_sources(lithium_device
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/camera/asi_camera.cpp
    )

    message(STATUS "ASI camera support enabled")
else()
    message(STATUS "ASI camera support disabled - SDK not found")
endif()
