/*
 * main.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Filter Wheel Main Header

*************************************************/

#pragma once

#include "controller.hpp"

namespace lithium::device::ascom::filterwheel {

/**
 * @brief Factory function to create an ASCOM filterwheel controller
 * 
 * @param name The name for the filterwheel instance
 * @return std::unique_ptr<ASCOMFilterwheelController> 
 */
auto createASCOMFilterwheel(const std::string& name) -> std::unique_ptr<ASCOMFilterwheelController>;

/**
 * @brief Get the version of the ASCOM filterwheel module
 * 
 * @return std::string Version string
 */
auto getModuleVersion() -> std::string;

/**
 * @brief Get the build information of the ASCOM filterwheel module
 * 
 * @return std::string Build information
 */
auto getBuildInfo() -> std::string;

/**
 * @brief Test if ASCOM drivers are available on this system
 * 
 * @return true If ASCOM drivers are available
 * @return false If ASCOM drivers are not available
 */
auto isASCOMAvailable() -> bool;

/**
 * @brief Get a list of available ASCOM filterwheel drivers
 * 
 * @return std::vector<std::string> List of available driver ProgIDs
 */
auto getAvailableDrivers() -> std::vector<std::string>;

} // namespace lithium::device::ascom::filterwheel
