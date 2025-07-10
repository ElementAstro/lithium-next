# ASCOM Device Implementation

# Add the modular filterwheel subdirectory
add_subdirectory(filterwheel)

# Add the modular focuser subdirectory
add_subdirectory(focuser)

# Add the modular rotator subdirectory
add_subdirectory(rotator)

add_library(
  lithium_device_ascom STATIC
  # Core headers
  telescope/main.hpp
  telescope/controller.hpp
  telescope/legacy_telescope.hpp
  camera/main.hpp
  camera/controller.hpp
  camera/legacy_camera.hpp
  focuser/main.hpp
  focuser/controller.hpp
  # Modular rotator (new structure)
  rotator/main.hpp
  rotator/controller.hpp
  rotator/components/hardware_interface.hpp
  rotator/components/position_manager.hpp
  rotator/components/property_manager.hpp
  rotator/components/preset_manager.hpp
  dome.hpp
  switch.hpp
  # Modular switch (new structure)
  switch/main.hpp
  switch/controller.hpp
  switch/components/hardware_interface.hpp
  switch/components/switch_manager.hpp
  switch/components/group_manager.hpp
  switch/components/timer_manager.hpp
  switch/components/power_manager.hpp
  switch/components/state_manager.hpp
  # Legacy focuser (moved to legacy folder)
  legacy/focuser.hpp
  # Enhanced support components
  ascom_com_helper.hpp
  alpaca_client.hpp
  # Implementation files
  telescope/main.cpp
  telescope/controller.cpp
  telescope/legacy_telescope.cpp
  camera/main.cpp
  camera/controller.cpp
  camera/legacy_camera.cpp
  focuser/main.cpp
  focuser/controller.cpp
  legacy/focuser.cpp
  # Modular rotator implementation
  rotator/main.cpp
  rotator/controller.cpp
  rotator/components/hardware_interface.cpp
  rotator/components/position_manager.cpp
  rotator/components/property_manager.cpp
  rotator/components/preset_manager.cpp
  dome.cpp
  switch.cpp
  # Modular switch implementation
  switch/main.cpp
  switch/controller.cpp
  switch/components/hardware_interface.cpp
  switch/components/switch_manager.cpp
  switch/components/group_manager.cpp
  switch/components/timer_manager.cpp
  switch/components/power_manager.cpp
  switch/components/state_manager.cpp)

# Windows-specific COM support
if(WIN32)
  target_sources(lithium_device_ascom PRIVATE ascom_com_helper.cpp)
  target_link_libraries(lithium_device_ascom PRIVATE ole32 oleaut32 uuid
                                                     comctl32 wbemuuid)
endif()

# Unix-specific HTTP client support
if(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CURL REQUIRED libcurl)
  target_link_libraries(lithium_device_ascom PRIVATE ${CURL_LIBRARIES})
  target_include_directories(lithium_device_ascom PRIVATE ${CURL_INCLUDE_DIRS})
  target_sources(
    lithium_device_ascom
    PRIVATE alpaca_client.cpp)
endif()

# Link common dependencies
target_link_libraries(lithium_device_ascom PRIVATE 
    lithium_atom_log
    lithium_atom_type
    lithium_device_ascom_focuser
    lithium_device_ascom_rotator
)

target_link_libraries(
  lithium_device_ascom
  PUBLIC lithium_device_template atom
  PRIVATE $<$<PLATFORM_ID:Windows>:ole32> $<$<PLATFORM_ID:Windows>:oleaut32>
          $<$<NOT:$<PLATFORM_ID:Windows>>:curl>)

target_include_directories(lithium_device_ascom
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_definitions(
  lithium_device_ascom PRIVATE $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
                               $<$<PLATFORM_ID:Windows>:NOMINMAX>)

# Install targets
install(
  TARGETS lithium_device_ascom
  EXPORT lithium_device_ascom_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

install(
  FILES telescope.hpp
        camera.hpp
        focuser.hpp
        filterwheel.hpp
        dome.hpp
        rotator/main.hpp
        switch.hpp
  DESTINATION include/lithium/device/ascom)
