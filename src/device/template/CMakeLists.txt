# Template Device Classes

add_library(lithium_device_template STATIC
    telescope.cpp
    telescope.hpp
    device.hpp
    camera.hpp
    focuser.hpp
    filterwheel.hpp
    dome.hpp
    rotator.hpp
    switch.hpp
)

target_link_libraries(lithium_device_template
    PUBLIC
        atom::utils
        atom::log
        atom::meta
)

target_include_directories(lithium_device_template
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Install targets
install(TARGETS lithium_device_template
    EXPORT lithium_device_template_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    telescope.hpp
    device.hpp
    camera.hpp
    focuser.hpp
    filterwheel.hpp
    dome.hpp
    rotator.hpp
    switch.hpp
    DESTINATION include/lithium/device/template
)
