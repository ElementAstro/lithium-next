# ASCOM Focuser Modular Implementation

# Create the focuser components library
add_library(
  lithium_device_ascom_focuser STATIC
  # Component headers
  components/hardware_interface.hpp
  components/movement_controller.hpp
  components/temperature_controller.hpp
  components/position_manager.hpp
  components/backlash_compensator.hpp
  components/property_manager.hpp
  # Component implementations
  components/hardware_interface.cpp
  components/movement_controller.cpp
  components/temperature_controller.cpp
  components/position_manager.cpp
  components/backlash_compensator.cpp
  components/property_manager.cpp)

# Link dependencies
target_link_libraries(
  lithium_device_ascom_focuser
  PUBLIC lithium_atom_log
         lithium_atom_type
         lithium_device_template
  PRIVATE atom)

# Include directories
target_include_directories(
  lithium_device_ascom_focuser
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
         ${CMAKE_CURRENT_SOURCE_DIR}/../..
         ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

# Platform-specific settings
if(WIN32)
  target_link_libraries(lithium_device_ascom_focuser PRIVATE ole32 oleaut32 uuid comctl32 wbemuuid)
  target_compile_definitions(lithium_device_ascom_focuser PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CURL REQUIRED libcurl)
  target_link_libraries(lithium_device_ascom_focuser PRIVATE ${CURL_LIBRARIES})
  target_include_directories(lithium_device_ascom_focuser PRIVATE ${CURL_INCLUDE_DIRS})
endif()

# Install the focuser components library
install(
  TARGETS lithium_device_ascom_focuser
  EXPORT lithium_device_ascom_focuser_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

# Install headers
install(
  FILES controller.hpp
        main.hpp
  DESTINATION include/lithium/device/ascom/focuser)

install(
  FILES components/hardware_interface.hpp
        components/movement_controller.hpp
        components/temperature_controller.hpp
        components/position_manager.hpp
        components/backlash_compensator.hpp
        components/property_manager.hpp
  DESTINATION include/lithium/device/ascom/focuser/components)
