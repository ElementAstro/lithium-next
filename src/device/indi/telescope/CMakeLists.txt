# Telescope component library
cmake_minimum_required(VERSION 3.16)

# Telescope component sources
set(TELESCOPE_COMPONENT_SOURCES
    connection.cpp
    motion.cpp
    tracking.cpp
    coordinates.cpp
    parking.cpp
    manager.cpp
)

# Telescope component headers
set(TELESCOPE_COMPONENT_HEADERS
    connection.hpp
    motion.hpp
    tracking.hpp
    coordinates.hpp
    parking.hpp
    manager.hpp
)

# Create telescope component library
add_library(telescope_components STATIC ${TELESCOPE_COMPONENT_SOURCES})

target_include_directories(telescope_components PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../..
)

target_link_libraries(telescope_components
    ${INDI_LIBRARIES}
    spdlog::spdlog
    atom-component
)

# Install headers
install(FILES ${TELESCOPE_COMPONENT_HEADERS}
    DESTINATION include/lithium/device/indi/telescope
)

# Install library
install(TARGETS telescope_components
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
