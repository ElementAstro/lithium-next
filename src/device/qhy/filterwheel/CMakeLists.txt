# QHY Filterwheel Modular Implementation

cmake_minimum_required(VERSION 3.20)

# Create the QHY filterwheel library
add_library(
    lithium_device_qhy_filterwheel STATIC
    # Main files
    filterwheel_controller.cpp
    # Headers
    filterwheel_controller.hpp
)

# Set properties
set_property(TARGET lithium_device_qhy_filterwheel PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_qhy_filterwheel PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_qhy_filterwheel
)

# Find and link QHY SDK
find_library(QHY_FILTERWHEEL_LIBRARY 
    NAMES qhyccd libqhyccd
    PATHS
        /usr/local/lib
        /usr/lib
        ${CMAKE_SOURCE_DIR}/libs/thirdparty/qhy/lib
    DOC "QHY Filterwheel SDK library"
)

if(QHY_FILTERWHEEL_LIBRARY)
    message(STATUS "Found QHY Filterwheel SDK: ${QHY_FILTERWHEEL_LIBRARY}")
    add_compile_definitions(LITHIUM_QHY_FILTERWHEEL_ENABLED)
    
    # Find QHY headers
    find_path(QHY_FILTERWHEEL_INCLUDE_DIR
        NAMES qhyccd.h
        PATHS
            /usr/local/include
            /usr/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/qhy/include
    )
    
    if(QHY_FILTERWHEEL_INCLUDE_DIR)
        target_include_directories(lithium_device_qhy_filterwheel PRIVATE ${QHY_FILTERWHEEL_INCLUDE_DIR})
    endif()
    
    target_link_libraries(lithium_device_qhy_filterwheel PRIVATE ${QHY_FILTERWHEEL_LIBRARY})
endif()

# Include directories
target_include_directories(
    lithium_device_qhy_filterwheel
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
           ${CMAKE_CURRENT_SOURCE_DIR}/../..
           ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# Link dependencies
target_link_libraries(
    lithium_device_qhy_filterwheel
    PUBLIC lithium_device_template
           atom
    PRIVATE lithium_atom_log
            lithium_atom_type
)

# Install the filterwheel library
install(
    TARGETS lithium_device_qhy_filterwheel
    EXPORT lithium_device_qhy_filterwheel_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(
    FILES filterwheel_controller.hpp
    DESTINATION include/lithium/device/qhy/filterwheel
)
