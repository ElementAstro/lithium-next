# ASCOM Device Implementation

add_library(
  lithium_device_ascom STATIC
  # Core headers
  telescope.hpp
  camera.hpp
  focuser.hpp
  filterwheel.hpp
  dome.hpp
  rotator.hpp
  switch.hpp
  # Enhanced support components
  ascom_com_helper.hpp
  ascom_alpaca_client.hpp
  # Implementation files
  telescope.cpp
  camera.cpp
  focuser.cpp
  filterwheel.cpp
  dome.cpp
  rotator.cpp
  switch.cpp)

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
    PRIVATE ascom_alpaca_client.cpp ascom_alpaca_client_v9.cpp
            ascom_alpaca_imagebytes.cpp ascom_alpaca_utils.cpp)
endif()

# Link common dependencies
target_link_libraries(lithium_device_ascom PRIVATE lithium_atom_log
                                                   lithium_atom_type)

target_link_libraries(
  lithium_device_ascom
  PUBLIC lithium_device_template atom::log
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
        rotator.hpp
        switch.hpp
  DESTINATION include/lithium/device/ascom)
