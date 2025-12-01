# ============================================================================
# dependency_targets.cmake
# ---------------------------------------------------------------------------
# Centralized helper to ensure external dependency targets exist even when the
# host toolchain lacks official CMake packages.
# ============================================================================

include(FetchContent)

# ----------------------------------------------------------------------------
# SQLite3 bridging
# ----------------------------------------------------------------------------

lithium_find_package(SQLite3 REQUIRED)

if(SQLite3_FOUND)
    # Upstream FindSQLite3 (since CMake 3.22) defines SQLite::SQLite3, while
    # legacy code in this repository expects SQLite3::SQLite3. Provide the alias
    # when only the new-style target exists.
    if(TARGET SQLite::SQLite3 AND NOT TARGET SQLite3::SQLite3)
        lithium_message(INFO "Creating compatibility target SQLite3::SQLite3 -> SQLite::SQLite3")
        add_library(SQLite3::SQLite3 INTERFACE IMPORTED)
        target_link_libraries(SQLite3::SQLite3 INTERFACE SQLite::SQLite3)
    elseif(NOT TARGET SQLite3::SQLite3 AND SQLite3_LIBRARIES)
        lithium_message(INFO "Creating compatibility target SQLite3::SQLite3 from cache variables")
        add_library(SQLite3::SQLite3 INTERFACE IMPORTED)
        target_link_libraries(SQLite3::SQLite3 INTERFACE ${SQLite3_LIBRARIES})
        if(SQLite3_INCLUDE_DIRS)
            target_include_directories(SQLite3::SQLite3 INTERFACE ${SQLite3_INCLUDE_DIRS})
        endif()
    endif()
else()
    lithium_message(WARNING "SQLite3 not found; database tests will be unavailable")
endif()

# ----------------------------------------------------------------------------
# libcurl provisioning
# ----------------------------------------------------------------------------

find_package(CURL QUIET)

if(CURL_FOUND)
    # Some environments expose CURL::libcurl; ensure the target exists even if
    # the module defines only CURL::libcurl_shared/static.
    if(TARGET CURL::libcurl)
        # nothing to do
    elseif(TARGET CURL::libcurl_shared)
        add_library(CURL::libcurl INTERFACE IMPORTED)
        target_link_libraries(CURL::libcurl INTERFACE CURL::libcurl_shared)
    elseif(TARGET CURL::libcurl_static)
        add_library(CURL::libcurl INTERFACE IMPORTED)
        target_link_libraries(CURL::libcurl INTERFACE CURL::libcurl_static)
    else()
        add_library(CURL::libcurl INTERFACE IMPORTED)
        target_link_libraries(CURL::libcurl INTERFACE ${CURL_LIBRARIES})
        target_include_directories(CURL::libcurl INTERFACE ${CURL_INCLUDE_DIRS})
    endif()
else()
    lithium_message(WARNING "cURL not found; online target will be disabled")
endif()

# ----------------------------------------------------------------------------
# nlohmann_json provisioning
# ----------------------------------------------------------------------------

set(_lithium_nlohmann_min_version 3.11.3)
find_package(nlohmann_json ${_lithium_nlohmann_min_version} QUIET)

if(NOT nlohmann_json_FOUND)
    # Prefer vendored copy in libs/atom (mirrors upstream 3.11.3).
    set(_lithium_vendor_json_dir "${CMAKE_SOURCE_DIR}/libs/atom/atom/type")
    if(EXISTS "${_lithium_vendor_json_dir}/json.hpp")
        add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
        target_include_directories(
            nlohmann_json::nlohmann_json
            INTERFACE
                ${_lithium_vendor_json_dir}
        )
        set(nlohmann_json_FOUND TRUE CACHE BOOL "" FORCE)
    else()
        lithium_message(INFO "Vendored nlohmann_json not found; fetching ${_lithium_nlohmann_min_version}")
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v${_lithium_nlohmann_min_version}
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(nlohmann_json)
    endif()
endif()

# Some toolchains ship header-only JSON without exporting the canonical target.
if(NOT TARGET nlohmann_json::nlohmann_json)
    if(DEFINED nlohmann_json_SOURCE_DIR)
        add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
        target_include_directories(
            nlohmann_json::nlohmann_json
            INTERFACE
                ${nlohmann_json_SOURCE_DIR}/include
        )
    elseif(DEFINED nlohmann_json_INCLUDE_DIRS)
        add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
        target_include_directories(
            nlohmann_json::nlohmann_json
            INTERFACE
                ${nlohmann_json_INCLUDE_DIRS}
        )
    endif()
endif()

unset(_lithium_vendor_json_dir)
unset(_lithium_nlohmann_min_version)
