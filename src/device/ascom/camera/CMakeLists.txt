# ASCOM Camera Modular Implementation

# Create the camera components library
add_library(
  lithium_device_ascom_camera STATIC
  # Main files
  main.cpp
  controller.cpp
  legacy_camera.cpp
  # Headers
  main.hpp
  controller.hpp
  legacy_camera.hpp
  # Component implementations
  components/hardware_interface.cpp
  components/exposure_manager.cpp
  components/temperature_controller.cpp
  components/property_manager.cpp
  components/image_processor.cpp
  components/sequence_manager.cpp
  components/video_manager.cpp
  # Component headers
  components/hardware_interface.hpp
  components/exposure_manager.hpp
  components/temperature_controller.hpp
  components/property_manager.hpp
  components/image_processor.hpp
  components/sequence_manager.hpp
  components/video_manager.hpp
)

# Set properties
set_property(TARGET lithium_device_ascom_camera PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_ascom_camera PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_ascom_camera
)

# Include directories
target_include_directories(
  lithium_device_ascom_camera
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
         ${CMAKE_CURRENT_SOURCE_DIR}/../..
         ${CMAKE_CURRENT_SOURCE_DIR}/../../..
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# Link dependencies
target_link_libraries(
  lithium_device_ascom_camera
  PUBLIC lithium_device_template
         atom
  PRIVATE lithium_atom_log
          lithium_atom_type
)

# Platform-specific settings
if(WIN32)
  target_link_libraries(lithium_device_ascom_camera PRIVATE ole32 oleaut32 uuid comctl32 wbemuuid)
  target_compile_definitions(lithium_device_ascom_camera PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CURL REQUIRED libcurl)
  target_link_libraries(lithium_device_ascom_camera PRIVATE ${CURL_LIBRARIES})
  target_include_directories(lithium_device_ascom_camera PRIVATE ${CURL_INCLUDE_DIRS})
endif()

# Install the camera components library
install(
  TARGETS lithium_device_ascom_camera
  EXPORT lithium_device_ascom_camera_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

# Install headers
install(
  FILES controller.hpp
        main.hpp
        legacy_camera.hpp
  DESTINATION include/lithium/device/ascom/camera)

install(
  FILES components/hardware_interface.hpp
        components/exposure_manager.hpp
        components/temperature_controller.hpp
        components/property_manager.hpp
        components/image_processor.hpp
        components/sequence_manager.hpp
        components/video_manager.hpp
  DESTINATION include/lithium/device/ascom/camera/components)
