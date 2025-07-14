# Common Device Configuration
# This file provides common settings and macros for all device modules

cmake_minimum_required(VERSION 3.20)

# Common function to create device libraries with consistent settings
function(create_device_library TARGET_NAME VENDOR_NAME DEVICE_TYPE)
    # Parse additional arguments
    set(options OPTIONAL)
    set(oneValueArgs SDK_LIBRARY SDK_INCLUDE_DIR)
    set(multiValueArgs SOURCES HEADERS DEPENDENCIES)
    cmake_parse_arguments(DEVICE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Create the library
    add_library(${TARGET_NAME} STATIC ${DEVICE_SOURCES} ${DEVICE_HEADERS})
    
    # Set standard properties
    set_property(TARGET ${TARGET_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_target_properties(${TARGET_NAME} PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
        OUTPUT_NAME ${TARGET_NAME}
    )
    
    # Standard include directories
    target_include_directories(${TARGET_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
               ${CMAKE_CURRENT_SOURCE_DIR}/../..
               ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    )
    
    # Standard dependencies
    target_link_libraries(${TARGET_NAME}
        PUBLIC
            lithium_device_template
            atom
        PRIVATE
            lithium_atom_log
            lithium_atom_type
            ${DEVICE_DEPENDENCIES}
    )
    
    # SDK specific settings
    if(DEVICE_SDK_LIBRARY AND DEVICE_SDK_INCLUDE_DIR)
        target_include_directories(${TARGET_NAME} PRIVATE ${DEVICE_SDK_INCLUDE_DIR})
        target_link_libraries(${TARGET_NAME} PRIVATE ${DEVICE_SDK_LIBRARY})
    endif()
    
    # Install targets
    install(
        TARGETS ${TARGET_NAME}
        EXPORT ${TARGET_NAME}_targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    
    # Install headers
    if(DEVICE_HEADERS)
        install(
            FILES ${DEVICE_HEADERS}
            DESTINATION include/lithium/device/${VENDOR_NAME}/${DEVICE_TYPE}
        )
    endif()
endfunction()

# Common function to find and validate SDK
function(find_device_sdk VENDOR_NAME SDK_HEADER SDK_LIBRARY_NAME)
    set(options OPTIONAL)
    set(oneValueArgs RESULT_VAR LIBRARY_VAR INCLUDE_VAR)
    set(multiValueArgs SEARCH_PATHS LIBRARY_NAMES HEADER_NAMES)
    cmake_parse_arguments(SDK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Default search paths
    if(NOT SDK_SEARCH_PATHS)
        set(SDK_SEARCH_PATHS
            /usr/include
            /usr/local/include
            /opt/${VENDOR_NAME}/include
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/${VENDOR_NAME}/include
        )
    endif()
    
    # Find include directory
    find_path(${SDK_INCLUDE_VAR}
        NAMES ${SDK_HEADER} ${SDK_HEADER_NAMES}
        PATHS ${SDK_SEARCH_PATHS}
        PATH_SUFFIXES ${VENDOR_NAME}
    )
    
    # Find library
    find_library(${SDK_LIBRARY_VAR}
        NAMES ${SDK_LIBRARY_NAME} ${SDK_LIBRARY_NAMES}
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/${VENDOR_NAME}/lib
            ${CMAKE_SOURCE_DIR}/libs/thirdparty/${VENDOR_NAME}/lib
        PATH_SUFFIXES x86_64 x64 lib64 armv6 armv7 armv8
    )
    
    # Set result
    if(${SDK_INCLUDE_VAR} AND ${SDK_LIBRARY_VAR})
        set(${SDK_RESULT_VAR} TRUE PARENT_SCOPE)
        message(STATUS "Found ${VENDOR_NAME} SDK: ${${SDK_LIBRARY_VAR}}")
        add_compile_definitions(LITHIUM_${VENDOR_NAME}_ENABLED)
    else()
        set(${SDK_RESULT_VAR} FALSE PARENT_SCOPE)
        message(WARNING "${VENDOR_NAME} SDK not found. ${VENDOR_NAME} device support will be disabled.")
    endif()
endfunction()

# Common function to create vendor main library
function(create_vendor_library VENDOR_NAME)
    set(options OPTIONAL)
    set(oneValueArgs TARGET_NAME)
    set(multiValueArgs DEVICE_MODULES SOURCES HEADERS)
    cmake_parse_arguments(VENDOR "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Default target name
    if(NOT VENDOR_TARGET_NAME)
        set(VENDOR_TARGET_NAME lithium_device_${VENDOR_NAME})
    endif()
    
    # Create main vendor library
    add_library(${VENDOR_TARGET_NAME} STATIC ${VENDOR_SOURCES} ${VENDOR_HEADERS})
    
    # Set standard properties
    set_property(TARGET ${VENDOR_TARGET_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_target_properties(${VENDOR_TARGET_NAME} PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
        OUTPUT_NAME ${VENDOR_TARGET_NAME}
    )
    
    # Standard dependencies
    target_link_libraries(${VENDOR_TARGET_NAME}
        PUBLIC
            lithium_device_template
            atom
        PRIVATE
            lithium_atom_log
            lithium_atom_type
            ${VENDOR_DEVICE_MODULES}
    )
    
    # Include directories
    target_include_directories(${VENDOR_TARGET_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    
    # Install targets
    install(
        TARGETS ${VENDOR_TARGET_NAME}
        EXPORT ${VENDOR_TARGET_NAME}_targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    
    # Install headers
    if(VENDOR_HEADERS)
        install(
            FILES ${VENDOR_HEADERS}
            DESTINATION include/lithium/device/${VENDOR_NAME}
        )
    endif()
endfunction()

# Standard compiler flags and definitions
set(LITHIUM_DEVICE_COMPILE_OPTIONS
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -O2>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -O2>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /O2>
)

# Common function to apply standard settings
function(apply_standard_settings TARGET_NAME)
    target_compile_options(${TARGET_NAME} PRIVATE ${LITHIUM_DEVICE_COMPILE_OPTIONS})
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
endfunction()

# Macro to add device subdirectories conditionally
macro(add_device_subdirectory SUBDIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/CMakeLists.txt")
        add_subdirectory(${SUBDIR})
    else()
        message(STATUS "Skipping ${SUBDIR} - no CMakeLists.txt found")
    endif()
endmacro()

# Common device types
set(LITHIUM_DEVICE_TYPES
    camera
    telescope
    focuser
    filterwheel
    rotator
    dome
    switch
    guider
    weather
    safety_monitor
)
