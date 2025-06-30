# Telescope modular component library
cmake_minimum_required(VERSION 3.16)

# Telescope component sources
set(TELESCOPE_COMPONENT_SOURCES
    components/hardware_interface.cpp
    components/motion_controller.cpp
    components/motion_controller_impl.cpp
    components/tracking_manager.cpp
    components/parking_manager.cpp
    components/coordinate_manager.cpp
    components/guide_manager.cpp
    telescope_controller.cpp
    controller_factory.cpp
)

# Telescope component headers
set(TELESCOPE_COMPONENT_HEADERS
    components/hardware_interface.hpp
    components/motion_controller.hpp
    components/tracking_manager.hpp
    components/parking_manager.hpp
    components/coordinate_manager.hpp
    components/guide_manager.hpp
    telescope_controller.hpp
    controller_factory.hpp
)

# Create telescope modular component library
add_library(telescope_modular_components STATIC ${TELESCOPE_COMPONENT_SOURCES})

target_include_directories(telescope_modular_components PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../..
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)

target_link_libraries(telescope_modular_components
    ${INDI_LIBRARIES}
    spdlog::spdlog
    atom-component
    # nlohmann_json is header-only, no linking needed
)

# Install headers
install(FILES ${TELESCOPE_COMPONENT_HEADERS}
    DESTINATION include/lithium/device/indi/telescope
)

# Install component headers
install(DIRECTORY components/
    DESTINATION include/lithium/device/indi/telescope/components
    FILES_MATCHING PATTERN "*.hpp"
)

# Install library
install(TARGETS telescope_modular_components
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
