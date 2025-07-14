# ASCOM Telescope Modular Implementation

# Create the telescope components library
add_library(
  lithium_device_ascom_telescope STATIC
  # Main files
  main.cpp
  controller.cpp
  legacy_telescope.cpp
  # Headers
  main.hpp
  controller.hpp
  legacy_telescope.hpp
  # Component implementations
  components/hardware_interface.cpp
  components/alignment_manager.cpp
  components/coordinate_manager.cpp
  components/guide_manager.cpp
  components/motion_controller.cpp
  components/parking_manager.cpp
  components/tracking_manager.cpp
  # Component headers
  components/hardware_interface.hpp
  components/alignment_manager.hpp
  components/coordinate_manager.hpp
  components/guide_manager.hpp
  components/motion_controller.hpp
  components/parking_manager.hpp
  components/tracking_manager.hpp
)

# Set properties
set_property(TARGET lithium_device_ascom_telescope PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_ascom_telescope PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_ascom_telescope
)

# Include directories
target_include_directories(
  lithium_device_ascom_telescope
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
         ${CMAKE_CURRENT_SOURCE_DIR}/../..
         ${CMAKE_CURRENT_SOURCE_DIR}/../../..
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# Link dependencies
target_link_libraries(
  lithium_device_ascom_telescope
  PUBLIC lithium_device_template
         atom
  PRIVATE lithium_atom_log
          lithium_atom_type
)

# Platform-specific settings
if(WIN32)
  target_link_libraries(lithium_device_ascom_telescope PRIVATE ole32 oleaut32 uuid comctl32 wbemuuid)
  target_compile_definitions(lithium_device_ascom_telescope PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CURL REQUIRED libcurl)
  target_link_libraries(lithium_device_ascom_telescope PRIVATE ${CURL_LIBRARIES})
  target_include_directories(lithium_device_ascom_telescope PRIVATE ${CURL_INCLUDE_DIRS})
endif()

# Install the telescope components library
install(
  TARGETS lithium_device_ascom_telescope
  EXPORT lithium_device_ascom_telescope_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

# Install headers
install(
  FILES controller.hpp
        main.hpp
        legacy_telescope.hpp
  DESTINATION include/lithium/device/ascom/telescope)

install(
  FILES components/hardware_interface.hpp
        components/alignment_manager.hpp
        components/coordinate_manager.hpp
        components/guide_manager.hpp
        components/motion_controller.hpp
        components/parking_manager.hpp
        components/tracking_manager.hpp
  DESTINATION include/lithium/device/ascom/telescope/components)
