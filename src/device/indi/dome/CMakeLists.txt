# Dome Component CMakeLists.txt

# Add components subdirectory
add_subdirectory(components)

# Dome client library
add_library(lithium_indi_dome_client STATIC
    dome_client.cpp
)

target_include_directories(lithium_indi_dome_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_indi_dome_client PUBLIC
    lithium_device_template
    lithium_indi_dome_components
    ${INDI_CLIENT_LIBRARIES}
    spdlog::spdlog
    Threads::Threads
)

# Set compile features
target_compile_features(lithium_indi_dome_client PUBLIC cxx_std_20)

# Export headers
install(FILES dome_client.hpp
    DESTINATION include/lithium/device/indi/dome
    COMPONENT devel
)

install(TARGETS lithium_indi_dome_client
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    COMPONENT runtime
)
