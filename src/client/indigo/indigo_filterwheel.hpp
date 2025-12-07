/*
 * indigo_filterwheel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Filter Wheel Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_FILTERWHEEL_HPP
#define LITHIUM_CLIENT_INDIGO_FILTERWHEEL_HPP

#include "indigo_device_base.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lithium::client::indigo {

/**
 * @brief Filter wheel movement status
 */
enum class FilterWheelMovementStatus : uint8_t {
    Idle,       // Not moving
    Moving,     // Currently moving to slot
    Stopped,    // Movement stopped
    Error       // Error occurred
};

/**
 * @brief Filter slot information
 */
struct FilterSlotInfo {
    int slotNumber{1};           // Current slot (1-indexed)
    int targetSlot{1};           // Target slot to move to
    int totalSlots{0};           // Total number of filter slots
    std::string filterName;      // Name of filter in current slot
    bool isMoving{false};        // Currently moving

    [[nodiscard]] auto toJson() const -> json {
        return {
            {"slotNumber", slotNumber},
            {"targetSlot", targetSlot},
            {"totalSlots", totalSlots},
            {"filterName", filterName},
            {"isMoving", isMoving}
        };
    }
};

/**
 * @brief Filter names and metadata
 */
struct FilterNameInfo {
    std::vector<std::string> names;  // Filter name for each slot
    int currentSlot{1};              // Current slot index (1-indexed)

    [[nodiscard]] auto toJson() const -> json {
        return {
            {"names", names},
            {"currentSlot", currentSlot}
        };
    }

    [[nodiscard]] auto getFilterName(int slot) const -> std::optional<std::string> {
        if (slot >= 1 && slot <= static_cast<int>(names.size())) {
            return names[slot - 1];  // Convert 1-indexed to 0-indexed
        }
        return std::nullopt;
    }
};

/**
 * @brief Movement callback for position changes
 */
using FilterWheelMovementCallback =
    std::function<void(int currentSlot, int targetSlot)>;

/**
 * @brief INDIGO Filter Wheel Device
 *
 * Provides filter wheel control functionality for INDIGO-connected wheels:
 * - Slot/position control (absolute movement)
 * - Filter name management
 * - Number of slots query
 * - Movement status tracking
 * - Movement progress callbacks
 */
class INDIGOFilterWheel : public INDIGODeviceBase {
public:
    /**
     * @brief Constructor
     * @param deviceName INDIGO device name
     */
    explicit INDIGOFilterWheel(const std::string& deviceName);

    /**
     * @brief Destructor
     */
    ~INDIGOFilterWheel() override;

    // ==================== Slot Control ====================

    /**
     * @brief Move to filter slot
     * @param slotNumber Target slot number (1-indexed)
     * @return Success or error
     */
    auto moveToSlot(int slotNumber) -> DeviceResult<bool>;

    /**
     * @brief Get current filter slot
     * @return Current slot number (1-indexed)
     */
    [[nodiscard]] auto getCurrentSlot() const -> int;

    /**
     * @brief Get target filter slot
     * @return Target slot number (1-indexed)
     */
    [[nodiscard]] auto getTargetSlot() const -> int;

    /**
     * @brief Get filter slot information
     */
    [[nodiscard]] auto getSlotInfo() const -> FilterSlotInfo;

    /**
     * @brief Check if filter wheel is moving
     */
    [[nodiscard]] auto isMoving() const -> bool;

    /**
     * @brief Wait for movement to complete
     * @param timeout Timeout in milliseconds
     * @return Success or error
     */
    auto waitForMovement(std::chrono::milliseconds timeout =
                             std::chrono::seconds(60)) -> DeviceResult<bool>;

    /**
     * @brief Abort current movement
     * @return Success or error
     */
    auto abortMovement() -> DeviceResult<bool>;

    // ==================== Slot Limits ====================

    /**
     * @brief Get total number of filter slots
     */
    [[nodiscard]] auto getNumberOfSlots() const -> int;

    /**
     * @brief Get minimum slot number
     */
    [[nodiscard]] auto getMinSlot() const -> int { return 1; }

    /**
     * @brief Get maximum slot number
     */
    [[nodiscard]] auto getMaxSlot() const -> int;

    /**
     * @brief Check if slot number is valid
     * @param slotNumber Slot number to validate
     */
    [[nodiscard]] auto isValidSlot(int slotNumber) const -> bool;

    // ==================== Filter Names ====================

    /**
     * @brief Set filter name for a slot
     * @param slotNumber Slot number (1-indexed)
     * @param filterName Name of the filter
     * @return Success or error
     */
    auto setFilterName(int slotNumber, const std::string& filterName)
        -> DeviceResult<bool>;

    /**
     * @brief Get filter name for a slot
     * @param slotNumber Slot number (1-indexed)
     * @return Filter name or error
     */
    [[nodiscard]] auto getFilterName(int slotNumber) const
        -> std::optional<std::string>;

    /**
     * @brief Get filter name for current slot
     * @return Filter name or empty string if not set
     */
    [[nodiscard]] auto getCurrentFilterName() const -> std::string;

    /**
     * @brief Set all filter names
     * @param filterNames Vector of filter names (indices 0-based)
     * @return Success or error
     */
    auto setAllFilterNames(const std::vector<std::string>& filterNames)
        -> DeviceResult<bool>;

    /**
     * @brief Get all filter names
     */
    [[nodiscard]] auto getAllFilterNames() const -> std::vector<std::string>;

    /**
     * @brief Get filter name information
     */
    [[nodiscard]] auto getFilterNameInfo() const -> FilterNameInfo;

    // ==================== Movement Callbacks ====================

    /**
     * @brief Set movement progress callback
     * @param callback Callback function (called with current slot and target slot)
     */
    void setMovementCallback(FilterWheelMovementCallback callback);

    // ==================== Status ====================

    /**
     * @brief Get filter wheel movement status
     */
    [[nodiscard]] auto getMovementStatus() const -> FilterWheelMovementStatus;

    /**
     * @brief Get full filter wheel status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json;

protected:
    void onConnected() override;
    void onDisconnected() override;
    void onPropertyUpdated(const Property& property) override;

private:
    class Impl;
    std::unique_ptr<Impl> filterwheelImpl_;
};

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_FILTERWHEEL_HPP
