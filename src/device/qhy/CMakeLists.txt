# QHY Device Implementation

# Include common device configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/../DeviceConfig.cmake)

# Find QHY SDK using common function
find_device_sdk(qhy qhyccd.h qhyccd
    RESULT_VAR QHY_FOUND
    LIBRARY_VAR QHY_LIBRARY
    INCLUDE_VAR QHY_INCLUDE_DIR
    HEADER_NAMES qhyccd.h
    LIBRARY_NAMES qhyccd libqhyccd
    SEARCH_PATHS
        ${QHY_ROOT_DIR}/include
        ${QHY_ROOT_DIR}
        /usr/local/include
        /usr/include
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/qhy/include
)

# Add subdirectories for each device type using common macro
add_device_subdirectory(camera)
add_device_subdirectory(filterwheel)

# QHY specific sources
set(QHY_SOURCES)
set(QHY_HEADERS qhyccd.h)

# Create QHY vendor library using common function
create_vendor_library(qhy
    TARGET_NAME lithium_device_qhy
    SOURCES ${QHY_SOURCES}
    HEADERS ${QHY_HEADERS}
    DEVICE_MODULES
        lithium_device_qhy_camera
        lithium_device_qhy_filterwheel
)

# Apply standard settings
apply_standard_settings(lithium_device_qhy)

# SDK specific settings
if(QHY_FOUND)
    target_include_directories(lithium_device_qhy PRIVATE ${QHY_INCLUDE_DIR})
    target_link_libraries(lithium_device_qhy PRIVATE ${QHY_LIBRARY})
endif()
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
