cmake_minimum_required(VERSION 3.20)

project(lithium_components_tests LANGUAGES CXX)

# Add subdirectories for all component test modules
add_subdirectory(dependency)
add_subdirectory(loader)
add_subdirectory(manager)
add_subdirectory(tracker)
add_subdirectory(version)
add_subdirectory(system)
add_subdirectory(debug)
add_subdirectory(integration)
