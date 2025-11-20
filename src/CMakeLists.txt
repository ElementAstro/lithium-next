# ============================================================================
# Lithium Source Modules
# ============================================================================
# This directory contains the core Lithium modules

lithium_print_subsection("Discovering Lithium Core Modules")

# Recursively add all subdirectories with CMakeLists.txt
lithium_add_subdirectories_recursive(
    ${CMAKE_CURRENT_SOURCE_DIR}
    MESSAGE_PREFIX "  ├─ Core module"
)
