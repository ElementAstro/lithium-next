# QHY Camera Device Implementation
cmake_minimum_required(VERSION 3.20)

# Find QHY SDK
find_path(QHY_INCLUDE_DIR qhyccd.h
    HINTS
    ${QHY_ROOT_DIR}/include
    ${QHY_ROOT_DIR}
    /usr/local/include
    /usr/include
    PATH_SUFFIXES qhy qhyccd
)

find_library(QHY_LIBRARY
    NAMES qhyccd libqhyccd
    HINTS
    ${QHY_ROOT_DIR}/lib
    ${QHY_ROOT_DIR}
    /usr/local/lib
    /usr/lib
    PATH_SUFFIXES x86_64 x64 lib64
)

if(QHY_INCLUDE_DIR AND QHY_LIBRARY)
    set(QHY_FOUND TRUE)
    message(STATUS "Found QHY SDK: ${QHY_LIBRARY}")
else()
    set(QHY_FOUND FALSE)
    message(WARNING "QHY SDK not found. QHY camera support will be disabled.")
endif()

# QHY Camera Implementation
if(QHY_FOUND)
    add_library(lithium_qhy_camera STATIC
        camera/qhy_camera.cpp
    )

    target_include_directories(lithium_qhy_camera
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${QHY_INCLUDE_DIR}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(lithium_qhy_camera
        PUBLIC
            lithium_device_template
            atom
            ${QHY_LIBRARY}
        PRIVATE
            pthread
            ${CMAKE_DL_LIBS}
    )

    target_compile_features(lithium_qhy_camera PUBLIC cxx_std_20)

    # Set compile definitions
    target_compile_definitions(lithium_qhy_camera
        PRIVATE
            LITHIUM_QHY_CAMERA_ENABLED=1
    )

    # Platform-specific settings
    if(UNIX AND NOT APPLE)
        target_link_libraries(lithium_qhy_camera PRIVATE udev)
    endif()

    # Add to main device library
    target_sources(lithium_device
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/camera/qhy_camera.cpp
    )

    message(STATUS "QHY camera support enabled")
else()
    message(STATUS "QHY camera support disabled - SDK not found")
endif()
