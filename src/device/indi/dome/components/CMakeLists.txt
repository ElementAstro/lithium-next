# Dome Components CMakeLists.txt

# Dome components library
add_library(lithium_indi_dome_components STATIC
    dome_motion.cpp
    dome_shutter.cpp
    dome_parking.cpp
    dome_weather.cpp
    dome_telescope.cpp
    dome_home.cpp
)

target_include_directories(lithium_indi_dome_components PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_indi_dome_components PUBLIC
    lithium_device_template
    ${INDI_CLIENT_LIBRARIES}
    spdlog::spdlog
    Threads::Threads
)

# Set compile features
target_compile_features(lithium_indi_dome_components PUBLIC cxx_std_20)

# Export headers
set(DOME_COMPONENT_HEADERS
    dome_motion.hpp
    dome_shutter.hpp
    dome_parking.hpp
    dome_weather.hpp
    dome_telescope.hpp
    dome_home.hpp
)

install(FILES ${DOME_COMPONENT_HEADERS}
    DESTINATION include/lithium/device/indi/dome/components
    COMPONENT devel
)

install(TARGETS lithium_indi_dome_components
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    COMPONENT runtime
)
