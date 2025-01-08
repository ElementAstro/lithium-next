cmake_minimum_required(VERSION 3.20)
project(example.component C CXX)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_RELEASE 0)
set(SOVERSION ${VERSION_MAJOR})
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_RELEASE}")

# Sources
set(SOURCES example1.cpp)

# Libraries
set(LIBS
    loguru
    atom
    ${ZLIB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# Build Object Library
add_library(${PROJECT_NAME}_OBJECT OBJECT ${SOURCES})
set_property(TARGET ${PROJECT_NAME}_OBJECT PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(${PROJECT_NAME}_OBJECT ${LIBS})

# Build Shared Library
add_library(${PROJECT_NAME} SHARED)
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_OBJECT ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${VERSION_STRING}
    SOVERSION ${SOVERSION}
    OUTPUT_NAME logger
)

# Install
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
