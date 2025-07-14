# ASCOM Switch Modular Implementation

# Create the switch components library
add_library(
  lithium_device_ascom_switch STATIC
  # Main files
  main.cpp
  controller.cpp
  # Headers
  main.hpp
  controller.hpp
  # Component implementations (when they exist)
  components/hardware_interface.cpp
  components/switch_manager.cpp
  components/group_manager.cpp
  components/timer_manager.cpp
  components/power_manager.cpp
  components/state_manager.cpp
  # Component headers
  components/hardware_interface.hpp
  components/switch_manager.hpp
  components/group_manager.hpp
  components/timer_manager.hpp
  components/power_manager.hpp
  components/state_manager.hpp
)

# Set properties
set_property(TARGET lithium_device_ascom_switch PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(lithium_device_ascom_switch PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    OUTPUT_NAME lithium_device_ascom_switch
)

# Include directories
target_include_directories(
  lithium_device_ascom_switch
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
         ${CMAKE_CURRENT_SOURCE_DIR}/../..
         ${CMAKE_CURRENT_SOURCE_DIR}/../../..
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# Link dependencies
target_link_libraries(
  lithium_device_ascom_switch
  PUBLIC lithium_device_template
         atom
  PRIVATE lithium_atom_log
          lithium_atom_type
)

# Platform-specific settings
if(WIN32)
  target_link_libraries(lithium_device_ascom_switch PRIVATE ole32 oleaut32 uuid comctl32 wbemuuid)
  target_compile_definitions(lithium_device_ascom_switch PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CURL REQUIRED libcurl)
  target_link_libraries(lithium_device_ascom_switch PRIVATE ${CURL_LIBRARIES})
  target_include_directories(lithium_device_ascom_switch PRIVATE ${CURL_INCLUDE_DIRS})
endif()

# Install the switch components library
install(
  TARGETS lithium_device_ascom_switch
  EXPORT lithium_device_ascom_switch_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

# Install headers
install(
  FILES controller.hpp
        main.hpp
  DESTINATION include/lithium/device/ascom/switch)

install(
  FILES components/hardware_interface.hpp
        components/switch_manager.hpp
        components/group_manager.hpp
        components/timer_manager.hpp
        components/power_manager.hpp
        components/state_manager.hpp
  DESTINATION include/lithium/device/ascom/switch/components)
