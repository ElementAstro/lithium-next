# ===========================================================================
# CMakeLists.txt for Lithium-Target-Online
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Target-Online
# Description: Online celestial database search module
# Author: Max Qian
# License: GPL3
#
# Architecture:
#   - client/       : Async HTTP client wrapper
#   - cache/        : Query result caching with TTL
#   - provider/     : Database providers (SIMBAD, VizieR, NED, JPL)
#   - parser/       : Response parsers (VOTable, JSON)
#   - rate_limiter/ : API rate limiting
#   - merger/       : Result merging strategies
#   - service/      : High-level service facade
# ===========================================================================

# ===========================================================================
# Source Files
# ===========================================================================

set(LITHIUM_TARGET_ONLINE_SOURCES
    client/http_client.cpp
    cache/query_cache.cpp
    provider/simbad_provider.cpp
    provider/vizier_provider.cpp
    provider/ned_provider.cpp
    provider/jpl_horizons_provider.cpp
    provider/open_ngc_provider.cpp
    parser/votable_parser.cpp
    parser/json_response_parser.cpp
    rate_limiter/api_rate_limiter.cpp
    merger/result_merger.cpp
    service/online_search_service.cpp
)

set(LITHIUM_TARGET_ONLINE_HEADERS
    online.hpp
    client/http_client.hpp
    client/connection_pool.hpp
    cache/query_cache.hpp
    provider/provider_interface.hpp
    provider/provider_config.hpp
    provider/simbad_provider.hpp
    provider/vizier_provider.hpp
    provider/ned_provider.hpp
    provider/jpl_horizons_provider.hpp
    provider/open_ngc_provider.hpp
    provider/stellarium_provider.hpp
    parser/response_parser.hpp
    parser/votable_parser.hpp
    parser/json_response_parser.hpp
    rate_limiter/api_rate_limiter.hpp
    merger/result_merger.hpp
    service/online_search_service.hpp
)

# ===========================================================================
# Object Library
# ===========================================================================

add_library(lithium_target_online_objects OBJECT
    ${LITHIUM_TARGET_ONLINE_SOURCES}
)

target_include_directories(lithium_target_online_objects
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(lithium_target_online_objects
    PUBLIC
        atom
        atom-utils
        nlohmann_json::nlohmann_json
        CURL::libcurl
)

# Apply standard compiler flags
if(COMMAND lithium_apply_compiler_flags)
    lithium_apply_compiler_flags(lithium_target_online_objects)
endif()

# ===========================================================================
# Export Variables to Parent Scope
# ===========================================================================

set(LITHIUM_TARGET_ONLINE_OBJECTS $<TARGET_OBJECTS:lithium_target_online_objects> PARENT_SCOPE)
set(LITHIUM_TARGET_ONLINE_HEADERS ${LITHIUM_TARGET_ONLINE_HEADERS} PARENT_SCOPE)
