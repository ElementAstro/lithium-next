# CMakeLists.txt for utils

# Add subdirectories
add_subdirectory(logging)

# Create utils library for common utilities
set(LITHIUM_UTILS_HEADERS
    format.hpp
    macro.hpp
)

# Header-only library for utilities
add_library(lithium_utils INTERFACE)

target_include_directories(lithium_utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set C++23 standard
target_compile_features(lithium_utils INTERFACE cxx_std_23)

# Add alias for convenience
add_library(lithium::utils ALIAS lithium_utils)

# Install headers
install(FILES ${LITHIUM_UTILS_HEADERS}
    DESTINATION include/lithium/utils
)
