project(FindMainCpp)

file(GLOB_RECURSE MAIN_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*/main.cpp)

foreach(MAIN_CPP_FILE ${MAIN_CPP_FILES})
    get_filename_component(DIR_PATH ${MAIN_CPP_FILE} DIRECTORY)

    get_filename_component(TARGET_NAME ${DIR_PATH} NAME)

    add_executable(${TARGET_NAME} ${MAIN_CPP_FILE})

    target_link_libraries(${TARGET_NAME} PRIVATE lithium_config atom lithium_task)
endforeach()

# Examples directly in the examples directory
add_executable(sequence_template_example sequence_template_example.cpp)
target_link_libraries(sequence_template_example PRIVATE lithium_config atom lithium_task)

# Integrated sequence example
add_executable(integrated_sequence_example integrated_sequence_example.cpp)
target_link_libraries(integrated_sequence_example PRIVATE lithium_config atom lithium_task)
