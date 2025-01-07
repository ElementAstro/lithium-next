project(FindMainCpp)

file(GLOB_RECURSE MAIN_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*/main.cpp)

foreach(MAIN_CPP_FILE ${MAIN_CPP_FILES})
    get_filename_component(DIR_PATH ${MAIN_CPP_FILE} DIRECTORY)

    get_filename_component(TARGET_NAME ${DIR_PATH} NAME)

    add_executable(${TARGET_NAME} ${MAIN_CPP_FILE})

    target_link_libraries(${TARGET_NAME} PRIVATE lithium_config atom)
endforeach()
