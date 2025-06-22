# CMakeLists.txt for SBIG Camera Support

option(ENABLE_SBIG_CAMERA "Enable SBIG camera support" ON)

if(ENABLE_SBIG_CAMERA)
    # Try to find SBIG Universal Driver
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
        message(STATUS "SBIG Universal Driver found: ${SBIG_LIBRARY}")
        
        # Define macro for conditional compilation
        add_definitions(-DLITHIUM_SBIG_CAMERA_ENABLED)
        
        # Create SBIG camera library
        add_library(lithium_sbig_camera SHARED
            sbig_camera.cpp
        )
        
        target_include_directories(lithium_sbig_camera
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${SBIG_INCLUDE_DIR}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
        )
        
        target_link_libraries(lithium_sbig_camera
            PUBLIC
                ${SBIG_LIBRARY}
                lithium_camera_template
                atom::log
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
