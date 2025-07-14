# ASI Filterwheel Modular Implementation

cmake_minimum_required(VERSION 3.20)

# Add components subdirectory
add_subdirectory(components)

# Create the ASI filterwheel library
add_library(
    lithium_device_asi_filterwheel STATIC
    # Main files
    main.cpp
    controller.cpp
    # Headers
    main.hpp
    controller.hpp
    controller_impl.hpp
    controller_stub.hpp
)

# Set properties
set_property(TARGET lithium_device_asi_filterwheel PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_asi_filterwheel PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_asi_filterwheel
)

# Find and link ASI SDK
find_library(ASI_FILTERWHEEL_LIBRARY 
    NAMES ASICamera2 libASICamera2 ASIEFW libASIEFW
    PATHS
        /usr/local/lib
        /usr/lib
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/lib
    DOC "ASI Filterwheel SDK library"
)
find_library(ASI_EFW_LIBRARY 
    NAMES EFW_filter libEFW_filter
    PATHS
        /usr/local/lib
        /usr/lib
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/lib
    DOC "ASI EFW SDK library"
)

if(ASI_EFW_LIBRARY)
    message(STATUS "Found ASI EFW SDK: ${ASI_EFW_LIBRARY}")
    add_compile_definitions(LITHIUM_ASI_EFW_ENABLED)
    target_link_libraries(asi_filterwheel PRIVATE ${ASI_EFW_LIBRARY})

    # Find EFW headers
    find_path(ASI_EFW_INCLUDE_DIR
        NAMES EFW_filter.h
        PATHS
            /usr/local/include
            /usr/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/asi/include
        DOC "ASI EFW SDK headers"
    )

    if(ASI_EFW_INCLUDE_DIR)
        target_include_directories(asi_filterwheel PRIVATE ${ASI_EFW_INCLUDE_DIR})
    endif()
else()
    message(STATUS "ASI EFW SDK not found, using stub implementation")
endif()

# Link common libraries
target_link_libraries(asi_filterwheel PUBLIC
    atom
    atom
    pthread
    asi_filterwheel_components  # Link the modular components
)

# Include directories
target_include_directories(asi_filterwheel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
    $<INSTALL_INTERFACE:include>
)

# Installation
install(TARGETS asi_filterwheel
    EXPORT asi_filterwheel_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/device/asi/filterwheel
)

install(FILES asi_filterwheel.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/device/asi/filterwheel
)

install(EXPORT asi_filterwheel_targets
    FILE asi_filterwheel_targets.cmake
    NAMESPACE lithium::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lithium
)
