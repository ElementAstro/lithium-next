/*
 * indi_filterwheel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI FilterWheel Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_FILTERWHEEL_HPP
#define LITHIUM_CLIENT_INDI_FILTERWHEEL_HPP

#include "indi_device_base.hpp"

namespace lithium::client::indi {

/**
 * @brief FilterWheel state enumeration
 */
enum class FilterWheelState : uint8_t { Idle, Moving, Error, Unknown };

/**
 * @brief Filter slot information
 */
struct FilterSlot {
    int position{0};
    std::string name;
    std::string color;

    [[nodiscard]] auto toJson() const -> json {
        return {{"position", position}, {"name", name}, {"color", color}};
    }
};

/**
 * @brief FilterWheel position information
 */
struct FilterWheelPosition {
    int current{1};
    int min{1};
    int max{8};
    std::vector<FilterSlot> slots;

    [[nodiscard]] auto toJson() const -> json {
        json slotsJson = json::array();
        for (const auto& slot : slots) {
            slotsJson.push_back(slot.toJson());
        }
        return {{"current", current},
                {"min", min},
                {"max", max},
                {"slots", slotsJson}};
    }
};

/**
 * @brief INDI FilterWheel Device
 *
 * Provides filterwheel-specific functionality including:
 * - Position control
 * - Filter name management
 * - Slot configuration
 */
class INDIFilterWheel : public INDIDeviceBase {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Construct a new INDI FilterWheel
     * @param name Device name
     */
    explicit INDIFilterWheel(std::string name);

    /**
     * @brief Destructor
     */
    ~INDIFilterWheel() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "FilterWheel";
    }

    // ==================== Connection ====================

    auto connect(const std::string& deviceName, int timeout = DEFAULT_TIMEOUT_MS,
                 int maxRetry = 3) -> bool override;

    auto disconnect() -> bool override;

    // ==================== Position Control ====================

    /**
     * @brief Set filter position
     * @param position Target position (1-based)
     * @return true if move started
     */
    auto setPosition(int position) -> bool;

    /**
     * @brief Get current position
     * @return Position (1-based), or nullopt if not available
     */
    [[nodiscard]] auto getPosition() const -> std::optional<int>;

    /**
     * @brief Get position information
     * @return Position info including min/max
     */
    [[nodiscard]] auto getPositionInfo() const -> FilterWheelPosition;

    /**
     * @brief Check if filterwheel is moving
     * @return true if moving
     */
    [[nodiscard]] auto isMoving() const -> bool;

    /**
     * @brief Wait for move to complete
     * @param timeout Timeout in milliseconds
     * @return true if move completed
     */
    auto waitForMove(std::chrono::milliseconds timeout =
                         std::chrono::seconds(30)) -> bool;

    // ==================== Filter Names ====================

    /**
     * @brief Get current filter name
     * @return Filter name, or nullopt if not available
     */
    [[nodiscard]] auto getCurrentFilterName() const -> std::optional<std::string>;

    /**
     * @brief Get filter name by position
     * @param position Position (1-based)
     * @return Filter name, or nullopt if not available
     */
    [[nodiscard]] auto getFilterName(int position) const
        -> std::optional<std::string>;

    /**
     * @brief Set filter name
     * @param position Position (1-based)
     * @param name Filter name
     * @return true if set successfully
     */
    auto setFilterName(int position, const std::string& name) -> bool;

    /**
     * @brief Get all filter names
     * @return Vector of filter names
     */
    [[nodiscard]] auto getFilterNames() const -> std::vector<std::string>;

    /**
     * @brief Set all filter names
     * @param names Vector of filter names
     * @return true if set successfully
     */
    auto setFilterNames(const std::vector<std::string>& names) -> bool;

    // ==================== Filter Slots ====================

    /**
     * @brief Get number of filter slots
     * @return Number of slots
     */
    [[nodiscard]] auto getSlotCount() const -> int;

    /**
     * @brief Get filter slot information
     * @param position Position (1-based)
     * @return Filter slot info, or nullopt if not available
     */
    [[nodiscard]] auto getSlot(int position) const -> std::optional<FilterSlot>;

    /**
     * @brief Get all filter slots
     * @return Vector of filter slots
     */
    [[nodiscard]] auto getSlots() const -> std::vector<FilterSlot>;

    // ==================== Status ====================

    /**
     * @brief Get filterwheel state
     * @return FilterWheel state
     */
    [[nodiscard]] auto getFilterWheelState() const -> FilterWheelState;

    /**
     * @brief Get filterwheel status as JSON
     * @return Status JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

protected:
    // ==================== Property Handlers ====================

    void onPropertyDefined(const INDIProperty& property) override;
    void onPropertyUpdated(const INDIProperty& property) override;

private:
    // ==================== Internal Methods ====================

    void handleSlotProperty(const INDIProperty& property);
    void handleNameProperty(const INDIProperty& property);

    void setupPropertyWatchers();

    // ==================== Member Variables ====================

    // FilterWheel state
    std::atomic<FilterWheelState> filterWheelState_{FilterWheelState::Idle};
    std::atomic<bool> isMoving_{false};

    // Position
    FilterWheelPosition positionInfo_;
    mutable std::mutex positionMutex_;
    std::condition_variable moveCondition_;
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_FILTERWHEEL_HPP
