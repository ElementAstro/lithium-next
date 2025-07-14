# ASCOM Device Implementation

# Include common device configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/../DeviceConfig.cmake)

# Add all modular device subdirectories using common macro
add_device_subdirectory(camera)
add_device_subdirectory(dome)
add_device_subdirectory(filterwheel)
add_device_subdirectory(focuser)
add_device_subdirectory(rotator)
add_device_subdirectory(switch)
add_device_subdirectory(telescope)

# ASCOM specific sources
set(ASCOM_SOURCES
    # Legacy focuser (moved to legacy folder)
    legacy/focuser.cpp
)

set(ASCOM_HEADERS
    # Core support files
    alpaca_client.hpp
    com_helper.hpp
    legacy/focuser.hpp
)

# Platform-specific sources
if(WIN32)
    list(APPEND ASCOM_SOURCES com_helper.cpp)
endif()

if(UNIX)
    list(APPEND ASCOM_SOURCES alpaca_client.cpp)
endif()

# Create ASCOM vendor library using common function
create_vendor_library(ascom
    TARGET_NAME lithium_device_ascom
    SOURCES ${ASCOM_SOURCES}
    HEADERS ${ASCOM_HEADERS}
    DEVICE_MODULES
        lithium_device_ascom_camera
        lithium_device_ascom_dome
        lithium_device_ascom_filterwheel
        lithium_device_ascom_focuser
        lithium_device_ascom_rotator
        lithium_device_ascom_switch
        lithium_device_ascom_telescope
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(lithium_device_ascom PRIVATE ole32 oleaut32 uuid comctl32 wbemuuid)
    target_compile_definitions(lithium_device_ascom PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CURL REQUIRED libcurl)
    target_link_libraries(lithium_device_ascom PRIVATE ${CURL_LIBRARIES})
    target_include_directories(lithium_device_ascom PRIVATE ${CURL_INCLUDE_DIRS})
endif()

# Apply standard settings
apply_standard_settings(lithium_device_ascom)
