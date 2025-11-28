/*
 * filterwheel_service.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: Filter wheel device service layer

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_FILTERWHEEL_HPP
#define LITHIUM_DEVICE_SERVICE_FILTERWHEEL_HPP

#include <memory>
#include <string>

#include "base_service.hpp"
#include "device/template/filterwheel.hpp"

namespace lithium::device {

/**
 * @brief Filter wheel service providing high-level filter wheel operations
 */
class FilterWheelService
    : public TypedDeviceService<FilterWheelService, AtomFilterWheel> {
public:
    FilterWheelService();
    ~FilterWheelService() override;

    // Disable copy
    FilterWheelService(const FilterWheelService&) = delete;
    FilterWheelService& operator=(const FilterWheelService&) = delete;

    /**
     * @brief List all available filter wheels
     */
    auto list() -> json;

    /**
     * @brief Get status of a specific filter wheel
     */
    auto getStatus(const std::string& deviceId) -> json;

    /**
     * @brief Connect or disconnect a filter wheel
     */
    auto connect(const std::string& deviceId, bool connected) -> json;

    /**
     * @brief Set filter position by slot number
     */
    auto setPosition(const std::string& deviceId,
                     const json& requestBody) -> json;

    /**
     * @brief Set filter position by filter name
     */
    auto setByName(const std::string& deviceId,
                   const json& requestBody) -> json;

    /**
     * @brief Get filter wheel capabilities
     */
    auto getCapabilities(const std::string& deviceId) -> json;

    /**
     * @brief Configure filter names
     */
    auto configureNames(const std::string& deviceId,
                        const json& requestBody) -> json;

    /**
     * @brief Get filter offsets
     */
    auto getOffsets(const std::string& deviceId) -> json;

    /**
     * @brief Set filter offsets
     */
    auto setOffsets(const std::string& deviceId,
                    const json& requestBody) -> json;

    /**
     * @brief Halt filter wheel
     */
    auto halt(const std::string& deviceId) -> json;

    /**
     * @brief Calibrate filter wheel
     */
    auto calibrate(const std::string& deviceId) -> json;

    // ========== INDI-specific operations ==========

    /**
     * @brief Get INDI-specific filter wheel properties
     */
    auto getINDIProperties(const std::string& deviceId) -> json;

    /**
     * @brief Set INDI-specific filter wheel property
     */
    auto setINDIProperty(const std::string& deviceId,
                         const std::string& propertyName,
                         const json& value) -> json;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_FILTERWHEEL_HPP
