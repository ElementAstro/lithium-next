# ASCOM Dome Modular Implementation

# Create the dome components library
add_library(
  lithium_device_ascom_dome STATIC
  # Main files
  controller.cpp
  # Headers
  controller.hpp
  # Component implementations
  components/hardware_interface.cpp
  components/azimuth_manager.cpp
  components/shutter_manager.cpp
  components/configuration_manager.cpp
  components/home_manager.cpp
  components/monitoring_system.cpp
  components/parking_manager.cpp
  components/telescope_coordinator.cpp
  components/weather_monitor.cpp
  components/alpaca_client.cpp
  # Component headers
  components/hardware_interface.hpp
  components/azimuth_manager.hpp
  components/shutter_manager.hpp
  components/configuration_manager.hpp
  components/home_manager.hpp
  components/monitoring_system.hpp
  components/parking_manager.hpp
  components/telescope_coordinator.hpp
  components/weather_monitor.hpp
  components/alpaca_client.hpp
)

# Set properties
set_property(TARGET lithium_device_ascom_dome PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_ascom_dome PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_ascom_dome
)

# Include directories
target_include_directories(
  lithium_device_ascom_dome
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
         ${CMAKE_CURRENT_SOURCE_DIR}/../..
         ${CMAKE_CURRENT_SOURCE_DIR}/../../..
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# Link dependencies
target_link_libraries(
  lithium_device_ascom_dome
  PUBLIC lithium_device_template
         atom
  PRIVATE lithium_atom_log
          lithium_atom_type
)

# Platform-specific settings
if(WIN32)
  target_link_libraries(lithium_device_ascom_dome PRIVATE ole32 oleaut32 uuid comctl32 wbemuuid)
  target_compile_definitions(lithium_device_ascom_dome PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CURL REQUIRED libcurl)
  target_link_libraries(lithium_device_ascom_dome PRIVATE ${CURL_LIBRARIES})
  target_include_directories(lithium_device_ascom_dome PRIVATE ${CURL_INCLUDE_DIRS})
endif()

# Install the dome components library
install(
  TARGETS lithium_device_ascom_dome
  EXPORT lithium_device_ascom_dome_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

# Install headers
install(
  FILES controller.hpp
  DESTINATION include/lithium/device/ascom/dome)

install(
  FILES components/hardware_interface.hpp
        components/azimuth_manager.hpp
        components/shutter_manager.hpp
        components/configuration_manager.hpp
        components/home_manager.hpp
        components/monitoring_system.hpp
        components/parking_manager.hpp
        components/telescope_coordinator.hpp
        components/weather_monitor.hpp
        components/alpaca_client.hpp
  DESTINATION include/lithium/device/ascom/dome/components)
