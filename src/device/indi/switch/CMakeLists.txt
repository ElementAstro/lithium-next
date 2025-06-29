# Switch Component CMakeLists.txt

# Switch client library
add_library(lithium_indi_switch_client STATIC
    switch_client.cpp
    switch_manager.cpp
    switch_timer.cpp
    switch_power.cpp
    switch_safety.cpp
    switch_stats.cpp
    switch_persistence.cpp
)

target_include_directories(lithium_indi_switch_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lithium_indi_switch_client PUBLIC
    lithium_device_template
    ${INDI_CLIENT_LIBRARIES}
    spdlog::spdlog
    Threads::Threads
)

# Set compile features
target_compile_features(lithium_indi_switch_client PUBLIC cxx_std_20)

# Export headers
set(SWITCH_HEADERS
    switch_client.hpp
    switch_manager.hpp
    switch_timer.hpp
    switch_power.hpp
    switch_safety.hpp
    switch_stats.hpp
    switch_persistence.hpp
)

install(FILES ${SWITCH_HEADERS}
    DESTINATION include/lithium/device/indi/switch
    COMPONENT devel
)

install(TARGETS lithium_indi_switch_client
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    COMPONENT runtime
)
