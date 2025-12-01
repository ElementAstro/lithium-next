/*
 * sections.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: Aggregated header for all configuration sections

**************************************************/

#ifndef LITHIUM_CONFIG_SECTIONS_HPP
#define LITHIUM_CONFIG_SECTIONS_HPP

// Core configuration sections
#include "logging_config.hpp"
#include "server_config.hpp"
#include "device_config.hpp"
#include "script_config.hpp"
#include "terminal_config.hpp"

// Future sections (uncomment as they are implemented):
// #include "target_config.hpp"

namespace lithium::config {

/**
 * @brief Helper to register all standard configuration sections
 *
 * Call this function to register all built-in configuration sections
 * with the ConfigRegistry.
 *
 * @param registry Reference to ConfigRegistry
 */
inline void registerAllSections(ConfigRegistry& registry) {
    registry.registerSection<LoggingConfig>();
    registry.registerSection<ServerConfig>();
    registry.registerSection<DeviceConfig>();
    registry.registerSection<ScriptConfig>();
    registry.registerSection<TerminalConfig>();
    // Future:
    // registry.registerSection<TargetConfig>();
}

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_SECTIONS_HPP
