# CMakeLists.txt for Atik Camera Support

option(ENABLE_ATIK_CAMERA "Enable Atik camera support" ON)

if(ENABLE_ATIK_CAMERA)
    # Try to find Atik SDK
    find_path(ATIK_INCLUDE_DIR
        NAMES AtikCameras.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/AtikSDK/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/atik/include
    )

    find_library(ATIK_LIBRARY
        NAMES AtikCameras atikcameras
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/AtikSDK/lib
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/atik/lib
    )

    if(ATIK_INCLUDE_DIR AND ATIK_LIBRARY)
        set(ATIK_FOUND TRUE)
        message(STATUS "Atik SDK found: ${ATIK_LIBRARY}")
        
        # Define macro for conditional compilation
        add_definitions(-DLITHIUM_ATIK_CAMERA_ENABLED)
        
        # Create Atik camera library
        add_library(lithium_atik_camera SHARED
            atik_camera.cpp
        )
        
        target_include_directories(lithium_atik_camera
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${ATIK_INCLUDE_DIR}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
        )
        
        target_link_libraries(lithium_atik_camera
            PUBLIC
                ${ATIK_LIBRARY}
                lithium_camera_template
                atom::log
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
