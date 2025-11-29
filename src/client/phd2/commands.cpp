/*
 * commands.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: PHD2 RPC command builders implementation

*************************************************/

#include "commands.hpp"

namespace phd2 {

// ==================== Guiding Control ====================

auto Commands::guide(const SettleParams& settle, bool recalibrate) -> json {
    json params = json::object();
    params["settle"] = settle.toJson();
    params["recalibrate"] = recalibrate;
    return params;
}

auto Commands::dither(double amount, bool raOnly, const SettleParams& settle)
    -> json {
    json params = json::object();
    params["amount"] = amount;
    params["raOnly"] = raOnly;
    params["settle"] = settle.toJson();
    return params;
}

auto Commands::stopCapture() -> json { return json::array(); }

auto Commands::setPaused(bool paused, bool full) -> json {
    if (full) {
        return json::array({paused, "full"});
    }
    return json::array({paused});
}

auto Commands::loop() -> json { return json::array(); }

// ==================== Calibration ====================

auto Commands::clearCalibration(std::string_view which) -> json {
    return json::array({std::string(which)});
}

auto Commands::flipCalibration() -> json { return json::array(); }

// ==================== Star Selection ====================

auto Commands::findStar(const std::optional<std::array<int, 4>>& roi) -> json {
    if (roi) {
        return json::array({*roi});
    }
    return json::array();
}

auto Commands::setLockPosition(double x, double y, bool exact) -> json {
    return json::array({x, y, exact});
}

// ==================== Camera Control ====================

auto Commands::setExposure(int exposureMs) -> json {
    return json::array({exposureMs});
}

auto Commands::captureSingleFrame(std::optional<int> exposureMs,
                                  std::optional<std::array<int, 4>> subframe)
    -> json {
    json params = json::object();
    if (exposureMs) {
        params["exposure"] = *exposureMs;
    }
    if (subframe) {
        params["subframe"] = *subframe;
    }
    return params;
}

// ==================== Profile Management ====================

auto Commands::setProfile(int profileId) -> json {
    return json::array({profileId});
}

// ==================== Equipment ====================

auto Commands::setConnected(bool connect) -> json {
    return json::array({connect});
}

auto Commands::guidePulse(int amount, std::string_view direction,
                          std::string_view which) -> json {
    return json::array({amount, std::string(direction), std::string(which)});
}

// ==================== Algorithm Parameters ====================

auto Commands::setAlgoParam(std::string_view axis, std::string_view name,
                            double value) -> json {
    return json::array({std::string(axis), std::string(name), value});
}

auto Commands::getAlgoParam(std::string_view axis, std::string_view name)
    -> json {
    return json::array({std::string(axis), std::string(name)});
}

// ==================== Settings ====================

auto Commands::setDecGuideMode(std::string_view mode) -> json {
    return json::array({std::string(mode)});
}

auto Commands::setGuideOutputEnabled(bool enable) -> json {
    return json::array({enable});
}

auto Commands::setLockShiftEnabled(bool enable) -> json {
    return json::array({enable});
}

auto Commands::setLockShiftParams(const json& params) -> json { return params; }

// ==================== Misc ====================

auto Commands::saveImage() -> json { return json::array(); }

auto Commands::shutdown() -> json { return json::array(); }

}  // namespace phd2
