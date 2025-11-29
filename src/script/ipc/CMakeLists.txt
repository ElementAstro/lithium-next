# CMakeLists.txt for IPC module
# Part of Lithium-Next Script System

set(IPC_HEADERS
    message_types.hpp
    message.hpp
    serializer.hpp
    channel.hpp
)

set(IPC_SOURCES
    message.cpp
    serializer.cpp
    channel.cpp
)

# Create library
add_library(lithium_script_ipc ${IPC_SOURCES})

target_include_directories(lithium_script_ipc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Dependencies
target_link_libraries(lithium_script_ipc
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)
