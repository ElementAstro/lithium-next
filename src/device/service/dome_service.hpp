/*
 * dome_service.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: Dome device service layer

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_DOME_HPP
#define LITHIUM_DEVICE_SERVICE_DOME_HPP

#include <memory>
#include <string>

#include "base_service.hpp"
#include "device/template/dome.hpp"

namespace lithium::device {

/**
 * @brief Dome service providing high-level dome operations
 *
 * Note: Currently uses a mock implementation for simulation.
 * Real INDI dome integration should be added.
 */
class DomeService : public BaseDeviceService {
public:
    DomeService();
    ~DomeService() override;

    // Disable copy
    DomeService(const DomeService&) = delete;
    DomeService& operator=(const DomeService&) = delete;

    /**
     * @brief List all available domes
     */
    auto list() -> json;

    /**
     * @brief Get status of a specific dome
     */
    auto getStatus(const std::string& deviceId) -> json;

    /**
     * @brief Connect or disconnect a dome
     */
    auto connect(const std::string& deviceId, bool connected) -> json;

    /**
     * @brief Slew dome to azimuth
     */
    auto slew(const std::string& deviceId, double azimuth) -> json;

    /**
     * @brief Control shutter (open/close)
     */
    auto shutterControl(const std::string& deviceId, bool open) -> json;

    /**
     * @brief Park dome
     */
    auto park(const std::string& deviceId) -> json;

    /**
     * @brief Unpark dome
     */
    auto unpark(const std::string& deviceId) -> json;

    /**
     * @brief Home dome
     */
    auto home(const std::string& deviceId) -> json;

    /**
     * @brief Stop dome motion
     */
    auto stop(const std::string& deviceId) -> json;

    /**
     * @brief Get dome capabilities
     */
    auto getCapabilities(const std::string& deviceId) -> json;

    // ========== INDI-specific operations ==========

    /**
     * @brief Get INDI-specific dome properties
     */
    auto getINDIProperties(const std::string& deviceId) -> json;

    /**
     * @brief Set INDI-specific dome property
     */
    auto setINDIProperty(const std::string& deviceId,
                         const std::string& propertyName, const json& value)
        -> json;

    /**
     * @brief Set dome slaved to mount
     */
    auto setSlaved(const std::string& deviceId, bool slaved) -> json;

    /**
     * @brief Get dome slaved status
     */
    auto getSlaved(const std::string& deviceId) -> json;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_DOME_HPP
