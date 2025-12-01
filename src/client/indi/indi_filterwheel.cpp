/*
 * indi_filterwheel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI FilterWheel Device Implementation

**************************************************/

#include "indi_filterwheel.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== Constructor/Destructor ====================

INDIFilterWheel::INDIFilterWheel(std::string name)
    : INDIDeviceBase(std::move(name)) {
    LOG_DEBUG("INDIFilterWheel created: {}", name_);
}

INDIFilterWheel::~INDIFilterWheel() {
    LOG_DEBUG("INDIFilterWheel destroyed: {}", name_);
}

// ==================== Connection ====================

auto INDIFilterWheel::connect(const std::string& deviceName, int timeout,
                              int maxRetry) -> bool {
    if (!INDIDeviceBase::connect(deviceName, timeout, maxRetry)) {
        return false;
    }

    setupPropertyWatchers();
    LOG_INFO("FilterWheel {} connected", deviceName);
    return true;
}

auto INDIFilterWheel::disconnect() -> bool {
    return INDIDeviceBase::disconnect();
}

// ==================== Position Control ====================

auto INDIFilterWheel::setPosition(int position) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set position: filterwheel not connected");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        if (position < positionInfo_.min || position > positionInfo_.max) {
            LOG_ERROR("Position {} out of range [{}, {}]", position,
                      positionInfo_.min, positionInfo_.max);
            return false;
        }
    }

    if (isMoving()) {
        LOG_WARN("FilterWheel already moving");
        return false;
    }

    LOG_INFO("Moving filterwheel to position: {}", position);

    filterWheelState_.store(FilterWheelState::Moving);
    isMoving_.store(true);

    if (!setNumberProperty("FILTER_SLOT", "FILTER_SLOT_VALUE",
                           static_cast<double>(position))) {
        LOG_ERROR("Failed to set filter position");
        filterWheelState_.store(FilterWheelState::Error);
        isMoving_.store(false);
        return false;
    }

    return true;
}

auto INDIFilterWheel::getPosition() const -> std::optional<int> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_.current;
}

auto INDIFilterWheel::getPositionInfo() const -> FilterWheelPosition {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_;
}

auto INDIFilterWheel::isMoving() const -> bool { return isMoving_.load(); }

auto INDIFilterWheel::waitForMove(std::chrono::milliseconds timeout) -> bool {
    if (!isMoving()) {
        return true;
    }

    std::unique_lock<std::mutex> lock(positionMutex_);
    return moveCondition_.wait_for(lock, timeout, [this] {
        return !isMoving_.load();
    });
}

// ==================== Filter Names ====================

auto INDIFilterWheel::getCurrentFilterName() const -> std::optional<std::string> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    int pos = positionInfo_.current;
    if (pos >= 1 && pos <= static_cast<int>(positionInfo_.slots.size())) {
        return positionInfo_.slots[pos - 1].name;
    }
    return std::nullopt;
}

auto INDIFilterWheel::getFilterName(int position) const
    -> std::optional<std::string> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    if (position >= 1 && position <= static_cast<int>(positionInfo_.slots.size())) {
        return positionInfo_.slots[position - 1].name;
    }
    return std::nullopt;
}

auto INDIFilterWheel::setFilterName(int position, const std::string& name)
    -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set filter name: filterwheel not connected");
        return false;
    }

    std::string elemName = "FILTER_SLOT_NAME_" + std::to_string(position);

    if (!setTextProperty("FILTER_NAME", elemName, name)) {
        LOG_ERROR("Failed to set filter name");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        if (position >= 1 &&
            position <= static_cast<int>(positionInfo_.slots.size())) {
            positionInfo_.slots[position - 1].name = name;
        }
    }

    return true;
}

auto INDIFilterWheel::getFilterNames() const -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    std::vector<std::string> names;
    names.reserve(positionInfo_.slots.size());
    for (const auto& slot : positionInfo_.slots) {
        names.push_back(slot.name);
    }
    return names;
}

auto INDIFilterWheel::setFilterNames(const std::vector<std::string>& names)
    -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set filter names: filterwheel not connected");
        return false;
    }

    bool success = true;
    for (size_t i = 0; i < names.size(); ++i) {
        if (!setFilterName(static_cast<int>(i + 1), names[i])) {
            success = false;
        }
    }

    return success;
}

// ==================== Filter Slots ====================

auto INDIFilterWheel::getSlotCount() const -> int {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_.max - positionInfo_.min + 1;
}

auto INDIFilterWheel::getSlot(int position) const -> std::optional<FilterSlot> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    if (position >= 1 && position <= static_cast<int>(positionInfo_.slots.size())) {
        return positionInfo_.slots[position - 1];
    }
    return std::nullopt;
}

auto INDIFilterWheel::getSlots() const -> std::vector<FilterSlot> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_.slots;
}

// ==================== Status ====================

auto INDIFilterWheel::getFilterWheelState() const -> FilterWheelState {
    return filterWheelState_.load();
}

auto INDIFilterWheel::getStatus() const -> json {
    json status = INDIDeviceBase::getStatus();

    status["filterWheelState"] = static_cast<int>(filterWheelState_.load());
    status["isMoving"] = isMoving();
    status["position"] = getPositionInfo().toJson();

    auto currentName = getCurrentFilterName();
    status["currentFilter"] = currentName.value_or("");

    json filtersJson = json::array();
    auto names = getFilterNames();
    for (size_t i = 0; i < names.size(); ++i) {
        filtersJson.push_back({{"position", i + 1}, {"name", names[i]}});
    }
    status["filters"] = filtersJson;

    return status;
}

// ==================== Property Handlers ====================

void INDIFilterWheel::onPropertyDefined(const INDIProperty& property) {
    INDIDeviceBase::onPropertyDefined(property);

    if (property.name == "FILTER_SLOT") {
        handleSlotProperty(property);
    } else if (property.name == "FILTER_NAME") {
        handleNameProperty(property);
    }
}

void INDIFilterWheel::onPropertyUpdated(const INDIProperty& property) {
    INDIDeviceBase::onPropertyUpdated(property);

    if (property.name == "FILTER_SLOT") {
        handleSlotProperty(property);

        // Check if move completed
        if (property.state == PropertyState::Ok && isMoving()) {
            filterWheelState_.store(FilterWheelState::Idle);
            isMoving_.store(false);
            moveCondition_.notify_all();
        } else if (property.state == PropertyState::Alert) {
            filterWheelState_.store(FilterWheelState::Error);
            isMoving_.store(false);
            moveCondition_.notify_all();
        }
    } else if (property.name == "FILTER_NAME") {
        handleNameProperty(property);
    }
}

// ==================== Internal Methods ====================

void INDIFilterWheel::handleSlotProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(positionMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "FILTER_SLOT_VALUE") {
            positionInfo_.current = static_cast<int>(elem.value);
            positionInfo_.min = static_cast<int>(elem.min);
            positionInfo_.max = static_cast<int>(elem.max);

            // Initialize slots if needed
            int slotCount = positionInfo_.max - positionInfo_.min + 1;
            if (static_cast<int>(positionInfo_.slots.size()) != slotCount) {
                positionInfo_.slots.resize(slotCount);
                for (int i = 0; i < slotCount; ++i) {
                    positionInfo_.slots[i].position = positionInfo_.min + i;
                    positionInfo_.slots[i].name =
                        "Filter " + std::to_string(positionInfo_.min + i);
                }
            }
        }
    }
}

void INDIFilterWheel::handleNameProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(positionMutex_);

    // Resize slots if needed
    if (positionInfo_.slots.empty()) {
        int count = static_cast<int>(property.texts.size());
        positionInfo_.slots.resize(count);
        for (int i = 0; i < count; ++i) {
            positionInfo_.slots[i].position = i + 1;
        }
    }

    // Update filter names
    for (size_t i = 0; i < property.texts.size(); ++i) {
        if (i < positionInfo_.slots.size()) {
            positionInfo_.slots[i].name = property.texts[i].value;
        }
    }
}

void INDIFilterWheel::setupPropertyWatchers() {
    // Watch slot property
    watchProperty("FILTER_SLOT", [this](const INDIProperty& prop) {
        handleSlotProperty(prop);
    });

    // Watch name property
    watchProperty("FILTER_NAME", [this](const INDIProperty& prop) {
        handleNameProperty(prop);
    });
}

}  // namespace lithium::client::indi
