# ============================================================================
# CMakeLists.txt for Lithium Examples
# ============================================================================
# This directory contains example programs demonstrating Lithium usage
# ============================================================================

project(lithium_examples)

lithium_print_subsection("Configuring Example Programs")

# Find all main.cpp files in subdirectories
file(GLOB_RECURSE MAIN_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*/main.cpp)

set(example_count 0)

foreach(MAIN_CPP_FILE ${MAIN_CPP_FILES})
    get_filename_component(DIR_PATH ${MAIN_CPP_FILE} DIRECTORY)
    get_filename_component(TARGET_NAME ${DIR_PATH} NAME)

    # Create executable for each example
    add_executable(${TARGET_NAME} ${MAIN_CPP_FILE})

    # Link common libraries
    target_link_libraries(${TARGET_NAME} PRIVATE
        lithium_config
        atom
        lithium_task
    )

    # Apply compiler flags
    lithium_apply_compiler_flags(${TARGET_NAME})

    # Set folder for IDE organization
    lithium_set_target_folder(${TARGET_NAME} "Examples")

    lithium_message(STATUS "  ├─ Example: ${BoldCyan}${TARGET_NAME}${ColorReset}")
    math(EXPR example_count "${example_count} + 1")
endforeach()

if(example_count EQUAL 0)
    lithium_message(WARNING "No example programs found")
else()
    lithium_message(SUCCESS "Configured ${example_count} example program(s)")
endif()

message(STATUS "")
