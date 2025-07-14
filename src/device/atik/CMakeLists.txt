# Atik Device Implementation

# Include common device configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/../DeviceConfig.cmake)

# Find Atik SDK using common function
find_device_sdk(atik AtikCameras.h AtikCameras
    RESULT_VAR ATIK_FOUND
    LIBRARY_VAR ATIK_LIBRARY
    INCLUDE_VAR ATIK_INCLUDE_DIR
    HEADER_NAMES AtikCameras.h
    LIBRARY_NAMES AtikCameras atikcameras
    SEARCH_PATHS
        /usr/include
        /usr/local/include
        /opt/AtikSDK/include
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/atik/include
)

# Atik specific sources
set(ATIK_SOURCES atik_camera.cpp)
set(ATIK_HEADERS atik_camera.hpp)

# Create Atik vendor library using common function
create_vendor_library(atik
    TARGET_NAME lithium_device_atik
    SOURCES ${ATIK_SOURCES}
    HEADERS ${ATIK_HEADERS}
)

# Apply standard settings
apply_standard_settings(lithium_device_atik)

# SDK specific settings
if(ATIK_FOUND)
    target_include_directories(lithium_device_atik PRIVATE ${ATIK_INCLUDE_DIR})
    target_link_libraries(lithium_device_atik PRIVATE ${ATIK_LIBRARY})
endif()
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
        )
        
        target_link_libraries(lithium_atik_camera
            PUBLIC
                ${ATIK_LIBRARY}
                lithium_camera_template
                atom
            PRIVATE
                Threads::Threads
        )
        
        # Set properties
        set_target_properties(lithium_atik_camera PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
        )
        
        # Install library
        install(TARGETS lithium_atik_camera
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
        
        # Install headers
        install(FILES atik_camera.hpp
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/atik
        )
        
    else()
        message(WARNING "Atik SDK not found. Atik camera support will be disabled.")
        set(ATIK_FOUND FALSE)
    endif()
else()
    message(STATUS "Atik camera support disabled by user")
    set(ATIK_FOUND FALSE)
endif()

# Export variables for parent scope
set(ATIK_FOUND ${ATIK_FOUND} PARENT_SCOPE)
set(ATIK_INCLUDE_DIR ${ATIK_INCLUDE_DIR} PARENT_SCOPE)
set(ATIK_LIBRARY ${ATIK_LIBRARY} PARENT_SCOPE)
