cmake_minimum_required(VERSION 3.20)

project(lithium_image_examples
    VERSION 1.0.0
    LANGUAGES C CXX
)

file(GLOB CPP_FILES "*.cpp")

set(LIBRARIES_TO_LINK lithium_image)

foreach(CPP_FILE ${CPP_FILES})
    get_filename_component(EXE_NAME ${CPP_FILE} NAME_WE)
    add_executable(${EXE_NAME} ${CPP_FILE})
    target_link_libraries(${EXE_NAME} ${LIBRARIES_TO_LINK})
endforeach()
