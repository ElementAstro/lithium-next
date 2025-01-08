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
# create_indi_module(lithium_client_indi_camera camera.cpp)
create_indi_module(lithium_client_indi_telescope telescope.cpp)
create_indi_module(lithium_client_indi_focuser focuser.cpp)
create_indi_module(lithium_client_indi_filterwheel filterwheel.cpp)
