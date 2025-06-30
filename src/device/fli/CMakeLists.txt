# CMakeLists.txt for FLI Camera Support

option(ENABLE_FLI_CAMERA "Enable FLI camera support" ON)

if(ENABLE_FLI_CAMERA)
    # Try to find FLI SDK
    find_path(FLI_INCLUDE_DIR
        NAMES libfli.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/fli/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/fli/include
    )

    find_library(FLI_LIBRARY
        NAMES fli FLI
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/fli/lib
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/fli/lib
    )

    if(FLI_INCLUDE_DIR AND FLI_LIBRARY)
        set(FLI_FOUND TRUE)
        message(STATUS "FLI SDK found: ${FLI_LIBRARY}")
        
        # Define macro for conditional compilation
        add_definitions(-DLITHIUM_FLI_CAMERA_ENABLED)
        
        # Create FLI camera library
        add_library(lithium_fli_camera SHARED
            fli_camera.cpp
        )
        
        target_include_directories(lithium_fli_camera
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${FLI_INCLUDE_DIR}
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
