# CMakeLists.txt for PlayerOne Camera Support

option(ENABLE_PLAYERONE_CAMERA "Enable PlayerOne camera support" ON)

if(ENABLE_PLAYERONE_CAMERA)
    # Try to find PlayerOne SDK
    find_path(PLAYERONE_INCLUDE_DIR
        NAMES PlayerOneCamera.h POACamera.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/playerone/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/playerone/include
    )

    find_library(PLAYERONE_LIBRARY
        NAMES PlayerOneCamera POACamera playeronecamera
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/playerone/lib
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/playerone/lib
    )

    if(PLAYERONE_INCLUDE_DIR AND PLAYERONE_LIBRARY)
        set(PLAYERONE_FOUND TRUE)
        message(STATUS "PlayerOne SDK found: ${PLAYERONE_LIBRARY}")

        # Define macro for conditional compilation
        add_definitions(-DLITHIUM_PLAYERONE_CAMERA_ENABLED)

        # Create PlayerOne camera library
        add_library(lithium_playerone_camera SHARED
            playerone_camera.cpp
        )

        target_include_directories(lithium_playerone_camera
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${PLAYERONE_INCLUDE_DIR}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
        )

        target_link_libraries(lithium_playerone_camera
            PUBLIC
                ${PLAYERONE_LIBRARY}
                lithium_camera_template
                atom::log
            PRIVATE
                Threads::Threads
        )

        # Set properties
        set_target_properties(lithium_playerone_camera PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
        )

        # Install library
        install(TARGETS lithium_playerone_camera
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )

        # Install headers
        install(FILES playerone_camera.hpp
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/playerone
        )

    else()
        message(WARNING "PlayerOne SDK not found. PlayerOne camera support will be disabled.")
        set(PLAYERONE_FOUND FALSE)
    endif()
else()
    message(STATUS "PlayerOne camera support disabled by user")
    set(PLAYERONE_FOUND FALSE)
endif()

# Export variables for parent scope
set(PLAYERONE_FOUND ${PLAYERONE_FOUND} PARENT_SCOPE)
set(PLAYERONE_INCLUDE_DIR ${PLAYERONE_INCLUDE_DIR} PARENT_SCOPE)
set(PLAYERONE_LIBRARY ${PLAYERONE_LIBRARY} PARENT_SCOPE)
