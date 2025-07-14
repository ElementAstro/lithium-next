# ASI Focuser Modular Implementation

cmake_minimum_required(VERSION 3.20)

# Add components subdirectory
add_subdirectory(components)

# Create the ASI focuser library
add_library(
    lithium_device_asi_focuser STATIC
    # Main files
    main.cpp
    controller.cpp
    # Headers
    main.hpp
    controller.hpp
)

# Set properties
set_property(TARGET lithium_device_asi_focuser PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_asi_focuser PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_asi_focuser
)

# Find and link ASI SDK
find_library(ASI_FOCUSER_LIBRARY 
    NAMES ASICamera2 libASICamera2 ASIEAF libASIEAF
    PATHS
        /usr/local/lib
        /usr/lib
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/lib
    DOC "ASI Focuser SDK library"
)
)

# Find and link ASI EAF SDK if available
find_library(ASI_EAF_LIBRARY 
    NAMES EAF_focuser libEAF_focuser
    PATHS
        /usr/local/lib
        /usr/lib
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/lib
    DOC "ASI EAF SDK library"
)

if(ASI_EAF_LIBRARY)
    message(STATUS "Found ASI EAF SDK: ${ASI_EAF_LIBRARY}")
    add_compile_definitions(LITHIUM_ASI_EAF_ENABLED)
    target_link_libraries(asi_focuser PRIVATE ${ASI_EAF_LIBRARY})
    
    # Find EAF headers
    find_path(ASI_EAF_INCLUDE_DIR
        NAMES EAF_focuser.h
        PATHS
            /usr/local/include
            /usr/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/include
        DOC "ASI EAF SDK headers"
    )
    
    if(ASI_EAF_INCLUDE_DIR)
        target_include_directories(asi_focuser PRIVATE ${ASI_EAF_INCLUDE_DIR})
    endif()
else()
    message(STATUS "ASI EAF SDK not found, using stub implementation")
endif()

# Link common libraries
target_link_libraries(asi_focuser PUBLIC
    asi_focuser_components  # Link our components library
    atom
    atom
    pthread
)

# Include directories
target_include_directories(asi_focuser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:include>
)

# Installation
install(TARGETS asi_focuser
    EXPORT asi_focuser_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/device/asi/focuser
)

install(FILES asi_focuser.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/device/asi/focuser
)

install(EXPORT asi_focuser_targets
    FILE asi_focuser_targets.cmake
    NAMESPACE lithium::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lithium
)
