cmake_minimum_required(VERSION 3.20)
project(lithium_client_indi)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(${CMAKE_SOURCE_DIR}/cmake/ScanModule.cmake)

# Common libraries
set(COMMON_LIBS
    loguru atom-system atom-io atom-utils atom-component atom-error)

if (NOT WIN32)
    find_package(INDI 2.0 REQUIRED)
    list(APPEND COMMON_LIBS indiclient)
endif()

# Create shared library target with PIC
function(create_indi_module NAME SOURCE)
    add_library(${NAME} SHARED ${SOURCE})
    set_property(TARGET ${NAME} PROPERTY POSITION_INDEPENDENT_CODE 1)
    target_link_libraries(${NAME} PUBLIC ${COMMON_LIBS})
endfunction()

# Create modules
# Add the new component-based camera subdirectory
add_subdirectory(camera)

# Add the new modular focuser subdirectory
add_subdirectory(focuser)

# Link the component-based camera to the compatibility layer
create_indi_module(lithium_client_indi_camera camera.cpp)
target_link_libraries(lithium_client_indi_camera PUBLIC indi_camera_components)

create_indi_module(lithium_client_indi_telescope telescope.cpp)

# Create legacy focuser module that uses the modular implementation
create_indi_module(lithium_client_indi_focuser focuser.cpp)
target_link_libraries(lithium_client_indi_focuser PUBLIC lithium_focuser_indi)

create_indi_module(lithium_client_indi_filterwheel filterwheel.cpp)
