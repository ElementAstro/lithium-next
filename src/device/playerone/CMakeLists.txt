# Standardized PlayerOne Device Implementation

cmake_minimum_required(VERSION 3.20)

option(ENABLE_PLAYERONE_CAMERA "Enable PlayerOne camera support" ON)

# Find PlayerOne SDK
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
    message(STATUS "Found PlayerOne SDK: ${PLAYERONE_LIBRARY}")
    add_compile_definitions(LITHIUM_PLAYERONE_ENABLED)
else()
    set(PLAYERONE_FOUND FALSE)
    message(WARNING "PlayerOne SDK not found. PlayerOne device support will be disabled.")
endif()

# Main PlayerOne library
add_library(lithium_device_playerone STATIC
    playerone_camera.cpp
    playerone_camera.hpp
)

# Set properties
set_property(TARGET lithium_device_playerone PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_playerone PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_playerone
)

# Link dependencies
target_link_libraries(lithium_device_playerone
    PUBLIC
        lithium_device_template
        atom
    PRIVATE
        lithium_atom_log
        lithium_atom_type
)

# SDK specific settings
if(PLAYERONE_FOUND)
    target_include_directories(lithium_device_playerone PRIVATE ${PLAYERONE_INCLUDE_DIR})
    target_link_libraries(lithium_device_playerone PRIVATE ${PLAYERONE_LIBRARY})
endif()

# Include directories
target_include_directories(lithium_device_playerone
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Install targets
install(
    TARGETS lithium_device_playerone
    EXPORT lithium_device_playerone_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(
    FILES playerone_camera.hpp
    DESTINATION include/lithium/device/playerone
)
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
        )

        target_link_libraries(lithium_playerone_camera
            PUBLIC
                ${PLAYERONE_LIBRARY}
                lithium_camera_template
                atom
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
