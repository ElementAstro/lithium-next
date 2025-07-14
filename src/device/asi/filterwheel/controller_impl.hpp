/*
 * controller_impl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Implementation header for ASI Filter Wheel Controller V2

This file provides the complete implementation details needed for
compilation in main.cpp

*************************************************/

#pragma once

#include "controller.hpp"

// Include all component implementations
#include "./components/hardware_interface.hpp"
#include "./components/position_manager.hpp"
#include "./components/configuration_manager.hpp"
#include "./components/sequence_manager.hpp"
#include "./components/monitoring_system.hpp"
#include "./components/calibration_system.hpp"

// This header ensures all necessary component definitions are available
// for compilation of the controller implementation
