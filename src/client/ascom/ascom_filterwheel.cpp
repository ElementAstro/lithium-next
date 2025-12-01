/*
 * ascom_filterwheel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Filter Wheel Device Implementation

**************************************************/

#include "ascom_filterwheel.hpp"

#include <spdlog/spdlog.h>
#include <thread>

namespace lithium::client::ascom {

// ==================== Constructor/Destructor ====================

ASCOMFilterWheel::ASCOMFilterWheel(std::string name, int deviceNumber)
    : ASCOMDeviceBase(std::move(name), ASCOMDeviceType::FilterWheel, deviceNumber) {
    spdlog::debug("ASCOMFilterWheel created: {}", name_);
}

ASCOMFilterWheel::~ASCOMFilterWheel() {
    spdlog::debug("ASCOMFilterWheel destroyed: {}", name_);
}

// ==================== Connection ====================

auto ASCOMFilterWheel::connect(int timeout) -> bool {
    if (!ASCOMDeviceBase::connect(timeout)) {
        return false;
    }

    // Get filter info
    slotCount_ = getSlotCount();
    filterNames_ = getFilterNames();
    focusOffsets_ = getFocusOffsets();

    spdlog::info("FilterWheel {} connected with {} slots", name_, slotCount_);
    return true;
}

// ==================== Position Control ====================

auto ASCOMFilterWheel::setPosition(int position) -> bool {
    if (!isConnected()) {
        setError("FilterWheel not connected");
        return false;
    }

    if (position < 0 || position >= slotCount_) {
        setError("Invalid filter position");
        return false;
    }

    auto response = setProperty("position", {{"Position", std::to_string(position)}});
    if (!response.isSuccess()) {
        setError("Failed to set filter position: " + response.errorMessage);
        return false;
    }

    filterWheelState_.store(FilterWheelState::Moving);
    spdlog::info("FilterWheel {} moving to position {}", name_, position);
    return true;
}

auto ASCOMFilterWheel::getPosition() -> int {
    auto pos = getIntProperty("position");
    if (pos.has_value()) {
        if (pos.value() == -1) {
            filterWheelState_.store(FilterWheelState::Moving);
        } else {
            filterWheelState_.store(FilterWheelState::Idle);
        }
        return pos.value();
    }
    return -1;
}

auto ASCOMFilterWheel::isMoving() -> bool {
    return getPosition() == -1;
}

auto ASCOMFilterWheel::waitForMove(std::chrono::milliseconds timeout) -> bool {
    auto start = std::chrono::steady_clock::now();

    while (isMoving()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    filterWheelState_.store(FilterWheelState::Idle);
    return true;
}

// ==================== Filter Info ====================

auto ASCOMFilterWheel::getSlotCount() -> int {
    auto response = getProperty("names");
    if (response.isSuccess() && response.value.is_array()) {
        return static_cast<int>(response.value.size());
    }
    return 0;
}

auto ASCOMFilterWheel::getFilterNames() -> std::vector<std::string> {
    std::vector<std::string> names;
    auto response = getProperty("names");
    if (response.isSuccess() && response.value.is_array()) {
        for (const auto& n : response.value) {
            if (n.is_string()) {
                names.push_back(n.get<std::string>());
            }
        }
    }
    return names;
}

auto ASCOMFilterWheel::setFilterNames(const std::vector<std::string>& names) -> bool {
    // ASCOM doesn't have a standard way to set filter names via Alpaca
    // This would typically be done through driver configuration
    filterNames_ = names;
    return true;
}

auto ASCOMFilterWheel::getFocusOffsets() -> std::vector<int> {
    std::vector<int> offsets;
    auto response = getProperty("focusoffsets");
    if (response.isSuccess() && response.value.is_array()) {
        for (const auto& o : response.value) {
            if (o.is_number_integer()) {
                offsets.push_back(o.get<int>());
            }
        }
    }
    return offsets;
}

auto ASCOMFilterWheel::getFilters() -> std::vector<FilterInfo> {
    std::vector<FilterInfo> filters;
    auto names = getFilterNames();
    auto offsets = getFocusOffsets();

    for (size_t i = 0; i < names.size(); ++i) {
        FilterInfo info;
        info.position = static_cast<int>(i);
        info.name = names[i];
        if (i < offsets.size()) {
            info.focusOffset = offsets[i];
        }
        filters.push_back(info);
    }

    return filters;
}

// ==================== Status ====================

auto ASCOMFilterWheel::getStatus() const -> json {
    json status = ASCOMDeviceBase::getStatus();

    status["filterWheelState"] = static_cast<int>(filterWheelState_.load());
    status["slotCount"] = slotCount_;
    status["filterNames"] = filterNames_;
    status["focusOffsets"] = focusOffsets_;

    return status;
}

}  // namespace lithium::client::ascom
