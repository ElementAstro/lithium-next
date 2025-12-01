/*
 * ascom_filterwheel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Filter Wheel Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_FILTERWHEEL_HPP
#define LITHIUM_CLIENT_ASCOM_FILTERWHEEL_HPP

#include "ascom_device_base.hpp"

#include <condition_variable>

namespace lithium::client::ascom {

/**
 * @brief Filter wheel state enumeration
 */
enum class FilterWheelState : uint8_t { Idle, Moving, Error };

/**
 * @brief Filter information
 */
struct FilterInfo {
    int position{0};
    std::string name;
    int focusOffset{0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"position", position},
                {"name", name},
                {"focusOffset", focusOffset}};
    }
};

/**
 * @brief ASCOM Filter Wheel Device
 *
 * Provides filter wheel functionality including:
 * - Position control
 * - Filter names
 * - Focus offsets
 */
class ASCOMFilterWheel : public ASCOMDeviceBase {
public:
    /**
     * @brief Construct a new ASCOM Filter Wheel
     * @param name Device name
     * @param deviceNumber Device number
     */
    explicit ASCOMFilterWheel(std::string name, int deviceNumber = 0);

    /**
     * @brief Destructor
     */
    ~ASCOMFilterWheel() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "FilterWheel";
    }

    // ==================== Connection ====================

    auto connect(int timeout = DEFAULT_TIMEOUT_MS) -> bool override;

    // ==================== Position Control ====================

    /**
     * @brief Set filter position
     * @param position Filter position (0-based)
     * @return true if move started
     */
    auto setPosition(int position) -> bool;

    /**
     * @brief Get current position
     * @return Current position, or -1 if moving
     */
    [[nodiscard]] auto getPosition() -> int;

    /**
     * @brief Check if filter wheel is moving
     */
    [[nodiscard]] auto isMoving() -> bool;

    /**
     * @brief Wait for movement to complete
     * @param timeout Maximum wait time
     * @return true if movement completed
     */
    auto waitForMove(std::chrono::milliseconds timeout = std::chrono::seconds(30))
        -> bool;

    // ==================== Filter Info ====================

    /**
     * @brief Get number of filter positions
     */
    [[nodiscard]] auto getSlotCount() -> int;

    /**
     * @brief Get filter names
     */
    [[nodiscard]] auto getFilterNames() -> std::vector<std::string>;

    /**
     * @brief Set filter names
     * @param names Filter names
     * @return true if set successfully
     */
    auto setFilterNames(const std::vector<std::string>& names) -> bool;

    /**
     * @brief Get focus offsets
     */
    [[nodiscard]] auto getFocusOffsets() -> std::vector<int>;

    /**
     * @brief Get filter info for all positions
     */
    [[nodiscard]] auto getFilters() -> std::vector<FilterInfo>;

    // ==================== Status ====================

    /**
     * @brief Get filter wheel state
     */
    [[nodiscard]] auto getFilterWheelState() const -> FilterWheelState {
        return filterWheelState_.load();
    }

    /**
     * @brief Get filter wheel status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

private:
    std::atomic<FilterWheelState> filterWheelState_{FilterWheelState::Idle};
    int slotCount_{0};
    std::vector<std::string> filterNames_;
    std::vector<int> focusOffsets_;

    mutable std::mutex moveMutex_;
    std::condition_variable moveCV_;
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_FILTERWHEEL_HPP
