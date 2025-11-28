/*
 * device_service.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: Aggregator header for all device services

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_HPP
#define LITHIUM_DEVICE_SERVICE_HPP

// Base infrastructure
#include "base_service.hpp"
#include "device_registry.hpp"
#include "indi_adapter.hpp"

// Device services
#include "camera_service.hpp"
#include "dome_service.hpp"
#include "filterwheel_service.hpp"
#include "focuser_service.hpp"
#include "mount_service.hpp"

#endif  // LITHIUM_DEVICE_SERVICE_HPP
