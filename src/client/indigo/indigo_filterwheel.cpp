/*
 * indigo_filterwheel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "indigo_filterwheel.hpp"
#include <mutex>
#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

class INDIGOFilterWheel::Impl {
public:
    explicit Impl(INDIGOFilterWheel* parent) : parent_(parent) {}

    void onConnected() {
        updateSlotInfo();
        LOG_F(INFO, "INDIGO FilterWheel[{}]: Connected, {} slots",
              parent_->getINDIGODeviceName(), slotInfo_.totalSlots);
    }

    void onDisconnected() {
        moving_ = false;
        LOG_F(INFO, "INDIGO FilterWheel[{}]: Disconnected", parent_->getINDIGODeviceName());
    }

    void onPropertyUpdated(const Property& property) {
        if (property.name == "WHEEL_SLOT") {
            for (const auto& elem : property.numberElements) {
                if (elem.name == "SLOT") {
                    slotInfo_.slotNumber = static_cast<int>(elem.value);
                    slotInfo_.targetSlot = static_cast<int>(elem.target);
                }
            }
            slotInfo_.isMoving = (property.state == PropertyState::Busy);
            moving_ = slotInfo_.isMoving;

            if (slotInfo_.slotNumber >= 1 &&
                slotInfo_.slotNumber <= static_cast<int>(filterNames_.size())) {
                slotInfo_.filterName = filterNames_[slotInfo_.slotNumber - 1];
            }

            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (movementCallback_) {
                movementCallback_(slotInfo_.slotNumber, slotInfo_.targetSlot);
            }

            if (!slotInfo_.isMoving && property.state == PropertyState::Ok) {
                LOG_F(INFO, "INDIGO FilterWheel[{}]: Moved to slot {} ({})",
                      parent_->getINDIGODeviceName(), slotInfo_.slotNumber,
                      slotInfo_.filterName);
            }
        } else if (property.name == "WHEEL_SLOT_NAME") {
            filterNames_.clear();
            for (const auto& elem : property.textElements) {
                filterNames_.push_back(elem.value);
            }
            slotInfo_.totalSlots = static_cast<int>(filterNames_.size());
        }
    }

    auto moveToSlot(int slotNumber) -> DeviceResult<bool> {
        if (!isValidSlot(slotNumber)) {
            return DeviceError{DeviceErrorCode::InvalidParameter,
                               "Invalid slot number: " + std::to_string(slotNumber)};
        }

        auto result = parent_->setNumberProperty("WHEEL_SLOT", "SLOT",
                                                  static_cast<double>(slotNumber));
        if (result.has_value()) {
            moving_ = true;
            slotInfo_.targetSlot = slotNumber;
            LOG_F(INFO, "INDIGO FilterWheel[{}]: Moving to slot {}",
                  parent_->getINDIGODeviceName(), slotNumber);
        }
        return result;
    }

    [[nodiscard]] auto getCurrentSlot() const -> int { return slotInfo_.slotNumber; }
    [[nodiscard]] auto getTargetSlot() const -> int { return slotInfo_.targetSlot; }
    [[nodiscard]] auto getSlotInfo() const -> FilterSlotInfo { return slotInfo_; }
    [[nodiscard]] auto isMoving() const -> bool { return moving_; }

    auto waitForMovement(std::chrono::milliseconds timeout) -> DeviceResult<bool> {
        return parent_->waitForPropertyState("WHEEL_SLOT", PropertyState::Ok, timeout);
    }

    auto abortMovement() -> DeviceResult<bool> {
        // Filter wheels typically don't support abort
        return DeviceError{DeviceErrorCode::NotSupported,
                           "Filter wheel abort not supported"};
    }

    [[nodiscard]] auto getNumberOfSlots() const -> int { return slotInfo_.totalSlots; }
    [[nodiscard]] auto getMaxSlot() const -> int { return slotInfo_.totalSlots; }

    [[nodiscard]] auto isValidSlot(int slotNumber) const -> bool {
        return slotNumber >= 1 && slotNumber <= slotInfo_.totalSlots;
    }

    auto setFilterName(int slotNumber, const std::string& filterName) -> DeviceResult<bool> {
        if (!isValidSlot(slotNumber)) {
            return DeviceError{DeviceErrorCode::InvalidParameter, "Invalid slot"};
        }
        std::string elemName = "SLOT_NAME_" + std::to_string(slotNumber);
        return parent_->setTextProperty("WHEEL_SLOT_NAME", elemName, filterName);
    }

    [[nodiscard]] auto getFilterName(int slotNumber) const -> std::optional<std::string> {
        if (slotNumber >= 1 && slotNumber <= static_cast<int>(filterNames_.size())) {
            return filterNames_[slotNumber - 1];
        }
        return std::nullopt;
    }

    [[nodiscard]] auto getCurrentFilterName() const -> std::string {
        return slotInfo_.filterName;
    }

    auto setAllFilterNames(const std::vector<std::string>& names) -> DeviceResult<bool> {
        for (size_t i = 0; i < names.size() && i < filterNames_.size(); ++i) {
            auto result = setFilterName(static_cast<int>(i + 1), names[i]);
            if (!result.has_value()) {
                return result;
            }
        }
        return DeviceResult<bool>(true);
    }

    [[nodiscard]] auto getAllFilterNames() const -> std::vector<std::string> {
        return filterNames_;
    }

    [[nodiscard]] auto getFilterNameInfo() const -> FilterNameInfo {
        return {filterNames_, slotInfo_.slotNumber};
    }

    void setMovementCallback(FilterWheelMovementCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        movementCallback_ = std::move(callback);
    }

    [[nodiscard]] auto getMovementStatus() const -> FilterWheelMovementStatus {
        if (moving_) return FilterWheelMovementStatus::Moving;
        return FilterWheelMovementStatus::Idle;
    }

    [[nodiscard]] auto getStatus() const -> json {
        return {
            {"connected", parent_->isConnected()},
            {"currentSlot", slotInfo_.slotNumber},
            {"targetSlot", slotInfo_.targetSlot},
            {"totalSlots", slotInfo_.totalSlots},
            {"filterName", slotInfo_.filterName},
            {"moving", moving_.load()},
            {"filterNames", filterNames_}
        };
    }

private:
    void updateSlotInfo() {
        auto prop = parent_->getProperty("WHEEL_SLOT");
        if (prop.has_value()) {
            for (const auto& elem : prop.value().numberElements) {
                if (elem.name == "SLOT") {
                    slotInfo_.slotNumber = static_cast<int>(elem.value);
                    slotInfo_.totalSlots = static_cast<int>(elem.max);
                }
            }
        }

        auto nameProp = parent_->getProperty("WHEEL_SLOT_NAME");
        if (nameProp.has_value()) {
            filterNames_.clear();
            for (const auto& elem : nameProp.value().textElements) {
                filterNames_.push_back(elem.value);
            }
            if (slotInfo_.totalSlots == 0) {
                slotInfo_.totalSlots = static_cast<int>(filterNames_.size());
            }
        }
    }

    INDIGOFilterWheel* parent_;
    std::atomic<bool> moving_{false};
    FilterSlotInfo slotInfo_;
    std::vector<std::string> filterNames_;
    std::mutex callbackMutex_;
    FilterWheelMovementCallback movementCallback_;
};

INDIGOFilterWheel::INDIGOFilterWheel(const std::string& deviceName)
    : INDIGODeviceBase(deviceName, "FilterWheel"),
      filterwheelImpl_(std::make_unique<Impl>(this)) {}

INDIGOFilterWheel::~INDIGOFilterWheel() = default;

auto INDIGOFilterWheel::moveToSlot(int slotNumber) -> DeviceResult<bool> {
    return filterwheelImpl_->moveToSlot(slotNumber);
}

auto INDIGOFilterWheel::getCurrentSlot() const -> int {
    return filterwheelImpl_->getCurrentSlot();
}

auto INDIGOFilterWheel::getTargetSlot() const -> int {
    return filterwheelImpl_->getTargetSlot();
}

auto INDIGOFilterWheel::getSlotInfo() const -> FilterSlotInfo {
    return filterwheelImpl_->getSlotInfo();
}

auto INDIGOFilterWheel::isMoving() const -> bool {
    return filterwheelImpl_->isMoving();
}

auto INDIGOFilterWheel::waitForMovement(std::chrono::milliseconds timeout) -> DeviceResult<bool> {
    return filterwheelImpl_->waitForMovement(timeout);
}

auto INDIGOFilterWheel::abortMovement() -> DeviceResult<bool> {
    return filterwheelImpl_->abortMovement();
}

auto INDIGOFilterWheel::getNumberOfSlots() const -> int {
    return filterwheelImpl_->getNumberOfSlots();
}

auto INDIGOFilterWheel::getMaxSlot() const -> int {
    return filterwheelImpl_->getMaxSlot();
}

auto INDIGOFilterWheel::isValidSlot(int slotNumber) const -> bool {
    return filterwheelImpl_->isValidSlot(slotNumber);
}

auto INDIGOFilterWheel::setFilterName(int slotNumber, const std::string& filterName)
    -> DeviceResult<bool> {
    return filterwheelImpl_->setFilterName(slotNumber, filterName);
}

auto INDIGOFilterWheel::getFilterName(int slotNumber) const -> std::optional<std::string> {
    return filterwheelImpl_->getFilterName(slotNumber);
}

auto INDIGOFilterWheel::getCurrentFilterName() const -> std::string {
    return filterwheelImpl_->getCurrentFilterName();
}

auto INDIGOFilterWheel::setAllFilterNames(const std::vector<std::string>& filterNames)
    -> DeviceResult<bool> {
    return filterwheelImpl_->setAllFilterNames(filterNames);
}

auto INDIGOFilterWheel::getAllFilterNames() const -> std::vector<std::string> {
    return filterwheelImpl_->getAllFilterNames();
}

auto INDIGOFilterWheel::getFilterNameInfo() const -> FilterNameInfo {
    return filterwheelImpl_->getFilterNameInfo();
}

void INDIGOFilterWheel::setMovementCallback(FilterWheelMovementCallback callback) {
    filterwheelImpl_->setMovementCallback(std::move(callback));
}

auto INDIGOFilterWheel::getMovementStatus() const -> FilterWheelMovementStatus {
    return filterwheelImpl_->getMovementStatus();
}

auto INDIGOFilterWheel::getStatus() const -> json {
    return filterwheelImpl_->getStatus();
}

void INDIGOFilterWheel::onConnected() {
    INDIGODeviceBase::onConnected();
    filterwheelImpl_->onConnected();
}

void INDIGOFilterWheel::onDisconnected() {
    INDIGODeviceBase::onDisconnected();
    filterwheelImpl_->onDisconnected();
}

void INDIGOFilterWheel::onPropertyUpdated(const Property& property) {
    INDIGODeviceBase::onPropertyUpdated(property);
    filterwheelImpl_->onPropertyUpdated(property);
}

}  // namespace lithium::client::indigo
