# ASI Focuser Components CMakeLists.txt

# Define component sources
set(ASI_FOCUSER_COMPONENT_SOURCES
    hardware_interface.cpp
    position_manager.cpp
    temperature_system.cpp
    configuration_manager.cpp
    monitoring_system.cpp
    calibration_system.cpp
)

# Define component headers
set(ASI_FOCUSER_COMPONENT_HEADERS
    hardware_interface.hpp
    position_manager.hpp
    temperature_system.hpp
    configuration_manager.hpp
    monitoring_system.hpp
    calibration_system.hpp
)

# Create components library
add_library(asi_focuser_components STATIC
    ${ASI_FOCUSER_COMPONENT_SOURCES}
    ${ASI_FOCUSER_COMPONENT_HEADERS}
)

# Target properties
target_include_directories(asi_focuser_components
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link dependencies
target_link_libraries(asi_focuser_components
    PUBLIC
        spdlog::spdlog
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Conditional EAF support
if(LITHIUM_ASI_EAF_ENABLED)
    target_compile_definitions(asi_focuser_components PRIVATE LITHIUM_ASI_EAF_ENABLED)
    target_link_libraries(asi_focuser_components PRIVATE ${EAF_LIBRARIES})
endif()

# C++ standard
target_compile_features(asi_focuser_components PUBLIC cxx_std_20)

# Compiler options
target_compile_options(asi_focuser_components PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Install targets
install(TARGETS asi_focuser_components
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${ASI_FOCUSER_COMPONENT_HEADERS}
    DESTINATION include/lithium/device/asi/focuser/components
)
