# CMakeLists.txt for modular ASCOM Rotator implementation
cmake_minimum_required(VERSION 3.20)

# Modular rotator library
add_library(lithium_device_ascom_rotator STATIC
    main.cpp
    controller.cpp
    components/hardware_interface.cpp
    components/position_manager.cpp
    components/property_manager.cpp
    components/preset_manager.cpp
)

# Set properties
set_property(TARGET lithium_device_ascom_rotator PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_ascom_rotator PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_ascom_rotator
)

# Include directories
target_include_directories(lithium_device_ascom_rotator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# Link dependencies
target_link_libraries(lithium_device_ascom_rotator
    PUBLIC
        lithium_device_template
        atom
    PRIVATE
        lithium_atom_log
        lithium_atom_type
)

# Platform-specific dependencies
if(WIN32)
    target_link_libraries(lithium_device_ascom_rotator PRIVATE
        ole32 oleaut32 uuid comctl32 wbemuuid)
    target_compile_definitions(lithium_device_ascom_rotator PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

if(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CURL REQUIRED libcurl)
    target_link_libraries(lithium_device_ascom_rotator PRIVATE ${CURL_LIBRARIES})
    target_include_directories(lithium_device_ascom_rotator PRIVATE ${CURL_INCLUDE_DIRS})

    # Find Boost for asio (if needed)
    find_package(Boost REQUIRED COMPONENTS system)
    target_link_libraries(lithium_device_ascom_rotator PRIVATE Boost::system)
endif()

# Integration test (if testing is enabled)
if(BUILD_TESTING)
    add_executable(rotator_integration_test rotator_integration_test.cpp)
    target_link_libraries(rotator_integration_test PRIVATE
        lithium_device_ascom_rotator
        lithium_device_template
        atom
    )
    target_include_directories(rotator_integration_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Add test
    add_test(NAME RotatorModularIntegrationTest COMMAND rotator_integration_test)
endif()

# Install targets
install(TARGETS lithium_device_ascom_rotator
    EXPORT lithium_device_ascom_rotator_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES
    main.hpp
    controller.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/ascom/rotator
)

install(FILES
    components/hardware_interface.hpp
    components/position_manager.hpp
    components/property_manager.hpp
    components/preset_manager.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/ascom/rotator/components
)

# Export targets
install(EXPORT lithium_device_ascom_rotator_targets
    FILE lithium_device_ascom_rotator_targets.cmake
    NAMESPACE lithium::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lithium
)
