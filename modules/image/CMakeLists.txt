cmake_minimum_required(VERSION 3.20)
project(lithium_image 
    VERSION 1.0.0
    LANGUAGES C CXX
)

find_package(OpenCV 4 REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(pybind11 REQUIRED)

# 编译器标志
add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-deprecated-enum-enum-conversion>)

# 源文件
set(SOURCES
    src/thumbhash.cpp
)

# 创建共享库
add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        atom
        ${CMAKE_THREAD_LIBS_INIT}
        ${OpenCV_LIBS}
        ${CFITSIO_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME image
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
