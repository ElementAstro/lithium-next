# ASI Device Implementation

# Include common device configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/../DeviceConfig.cmake)

# Find ASI SDK using common function
find_device_sdk(asi ASICamera2.h ASICamera2
    RESULT_VAR ASI_FOUND
    LIBRARY_VAR ASI_LIBRARY
    INCLUDE_VAR ASI_INCLUDE_DIR
    HEADER_NAMES ASICamera2.h ASIEFW.h ASIEAF.h
    LIBRARY_NAMES ASICamera2 libASICamera2 ASIEFW libASIEFW ASIEAF libASIEAF
    SEARCH_PATHS
        ${ASI_ROOT_DIR}/include
        ${ASI_ROOT_DIR}
        /usr/local/include
        /usr/include
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/include
)

# Add subdirectories for each device type using common macro
add_device_subdirectory(camera)
add_device_subdirectory(filterwheel)
add_device_subdirectory(focuser)

# Create ASI vendor library using common function
create_vendor_library(asi
    TARGET_NAME lithium_device_asi
    DEVICE_MODULES
        lithium_device_asi_camera
        lithium_device_asi_filterwheel
        lithium_device_asi_focuser
)

# Apply standard settings
apply_standard_settings(lithium_device_asi)

# SDK specific settings
if(ASI_FOUND)
    target_include_directories(lithium_device_asi PRIVATE ${ASI_INCLUDE_DIR})
    target_link_libraries(lithium_device_asi PRIVATE ${ASI_LIBRARY})
endif()
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
