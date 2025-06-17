/*
 * module.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Component registration for modular INDI FilterWheel

*************************************************/

#include "filterwheel.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include "atom/components/component.hpp"

// Component registration
extern "C" {

// Factory function for creating filterwheel instances
std::shared_ptr<INDIFilterwheel> createModularINDIFilterwheel(const std::string& name) {
    return std::make_shared<INDIFilterwheel>(name);
}

// Register all filterwheel methods
void registerFilterwheelMethods() {
    auto logger = spdlog::get("filterwheel_indi");
    if (!logger) {
        logger = spdlog::stdout_color_mt("filterwheel_indi");
    }
    
    logger->info("Modular INDI FilterWheel module initialized");
    logger->info("Available methods:");
    logger->info("  - Connection: connect, disconnect, scan, is_connected");
    logger->info("  - Control: get_position, set_position, is_moving, abort_motion");
    logger->info("  - Filters: get_filter_count, get_slot_name, select_filter_by_name");
    logger->info("  - Statistics: get_total_moves, get_average_move_time");
    logger->info("  - Configuration: save_configuration, load_configuration");
    logger->info("  - Temperature: get_temperature, has_temperature_sensor");
    logger->info("Total: 25+ methods available via factory function");
}

} // extern "C"
