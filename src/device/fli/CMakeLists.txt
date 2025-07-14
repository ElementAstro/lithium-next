# FLI Device Implementation

# Include common device configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/../DeviceConfig.cmake)

# Find FLI SDK using common function
find_device_sdk(fli libfli.h fli
    RESULT_VAR FLI_FOUND
    LIBRARY_VAR FLI_LIBRARY
    INCLUDE_VAR FLI_INCLUDE_DIR
    HEADER_NAMES libfli.h
    LIBRARY_NAMES fli FLI
    SEARCH_PATHS
        /usr/include
        /usr/local/include
        /opt/fli/include
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/fli/include
)

# FLI specific sources
set(FLI_SOURCES fli_camera.cpp)
set(FLI_HEADERS fli_camera.hpp)

# Create FLI vendor library using common function
create_vendor_library(fli
    TARGET_NAME lithium_device_fli
    SOURCES ${FLI_SOURCES}
    HEADERS ${FLI_HEADERS}
)

# Apply standard settings
apply_standard_settings(lithium_device_fli)

# SDK specific settings
if(FLI_FOUND)
    target_include_directories(lithium_device_fli PRIVATE ${FLI_INCLUDE_DIR})
    target_link_libraries(lithium_device_fli PRIVATE ${FLI_LIBRARY})
endif()
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
        )
        
        target_link_libraries(lithium_fli_camera
            PUBLIC
                ${FLI_LIBRARY}
                lithium_camera_template
                atom
            PRIVATE
                Threads::Threads
        )
        
        # Set properties
        set_target_properties(lithium_fli_camera PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
        )
        
        # Install library
        install(TARGETS lithium_fli_camera
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
        
        # Install headers
        install(FILES fli_camera.hpp
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/fli
        )
        
    else()
        message(WARNING "FLI SDK not found. FLI camera support will be disabled.")
        set(FLI_FOUND FALSE)
    endif()
else()
    message(STATUS "FLI camera support disabled by user")
    set(FLI_FOUND FALSE)
endif()

# Export variables for parent scope
set(FLI_FOUND ${FLI_FOUND} PARENT_SCOPE)
set(FLI_INCLUDE_DIR ${FLI_INCLUDE_DIR} PARENT_SCOPE)
set(FLI_LIBRARY ${FLI_LIBRARY} PARENT_SCOPE)
