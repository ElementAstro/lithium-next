# Standardized SBIG Device Implementation

cmake_minimum_required(VERSION 3.20)

option(ENABLE_SBIG_CAMERA "Enable SBIG camera support" ON)

# Find SBIG Universal Driver
find_path(SBIG_INCLUDE_DIR
    NAMES sbigudrv.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/sbig/include
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/sbig/include
)

find_library(SBIG_LIBRARY
    NAMES sbigudrv SBIGUDrv
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/sbig/lib
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/sbig/lib
)

if(SBIG_INCLUDE_DIR AND SBIG_LIBRARY)
    set(SBIG_FOUND TRUE)
    message(STATUS "Found SBIG Universal Driver: ${SBIG_LIBRARY}")
    add_compile_definitions(LITHIUM_SBIG_ENABLED)
else()
    set(SBIG_FOUND FALSE)
    message(WARNING "SBIG Universal Driver not found. SBIG device support will be disabled.")
endif()

# Main SBIG library
add_library(lithium_device_sbig STATIC
    sbig_camera.cpp
    sbig_camera.hpp
)

# Set properties
set_property(TARGET lithium_device_sbig PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_sbig PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_sbig
)

# Link dependencies
target_link_libraries(lithium_device_sbig
    PUBLIC
        lithium_device_template
        atom
    PRIVATE
        lithium_atom_log
        lithium_atom_type
)

# SDK specific settings
if(SBIG_FOUND)
    target_include_directories(lithium_device_sbig PRIVATE ${SBIG_INCLUDE_DIR})
    target_link_libraries(lithium_device_sbig PRIVATE ${SBIG_LIBRARY})
endif()

# Include directories
target_include_directories(lithium_device_sbig
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Install targets
install(
    TARGETS lithium_device_sbig
    EXPORT lithium_device_sbig_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(
    FILES sbig_camera.hpp
    DESTINATION include/lithium/device/sbig
)
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
        )

        target_link_libraries(lithium_sbig_camera
            PUBLIC
                ${SBIG_LIBRARY}
                lithium_camera_template
                atom
            PRIVATE
                Threads::Threads
        )

        # Set properties
        set_target_properties(lithium_sbig_camera PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
        )

        # Install library
        install(TARGETS lithium_sbig_camera
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )

        # Install headers
        install(FILES sbig_camera.hpp
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/sbig
        )

    else()
        message(WARNING "SBIG Universal Driver not found. SBIG camera support will be disabled.")
        set(SBIG_FOUND FALSE)
    endif()
else()
    message(STATUS "SBIG camera support disabled by user")
    set(SBIG_FOUND FALSE)
endif()

# Export variables for parent scope
set(SBIG_FOUND ${SBIG_FOUND} PARENT_SCOPE)
set(SBIG_INCLUDE_DIR ${SBIG_INCLUDE_DIR} PARENT_SCOPE)
set(SBIG_LIBRARY ${SBIG_LIBRARY} PARENT_SCOPE)
