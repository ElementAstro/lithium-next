/*
 * telescope.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomTelescope Implementation

*************************************************/

#include "telescope.hpp"
#include <spdlog/spdlog.h>

// Notification methods implementation
void AtomTelescope::notifySlewComplete(bool success, const std::string &message) {
    LOG_F(INFO, "Slew complete: success={}, message={}", success, message);
    is_slewing_ = false;

    if (slew_callback_) {
        slew_callback_(success, message);
    }
}

void AtomTelescope::notifyTrackingChange(bool enabled) {
    LOG_F(INFO, "Tracking changed: enabled={}", enabled);
    is_tracking_ = enabled;

    if (tracking_callback_) {
        tracking_callback_(enabled);
    }
}

void AtomTelescope::notifyParkChange(bool parked) {
    LOG_F(INFO, "Park status changed: parked={}", parked);
    is_parked_ = parked;

    if (park_callback_) {
        park_callback_(parked);
    }
}

void AtomTelescope::notifyCoordinateUpdate(const EquatorialCoordinates &coords) {
    current_radec_ = coords;

    if (coordinate_callback_) {
        coordinate_callback_(coords);
    }
}
