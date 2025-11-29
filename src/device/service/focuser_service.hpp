/*
 * focuser_service.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: Focuser device service layer

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_FOCUSER_HPP
#define LITHIUM_DEVICE_SERVICE_FOCUSER_HPP

#include <memory>
#include <string>

#include "base_service.hpp"
#include "device/template/focuser.hpp"

namespace lithium::device {

/**
 * @brief Focuser service providing high-level focuser operations
 */
class FocuserService : public TypedDeviceService<FocuserService, AtomFocuser> {
public:
    FocuserService();
    ~FocuserService() override;

    // Disable copy
    FocuserService(const FocuserService&) = delete;
    FocuserService& operator=(const FocuserService&) = delete;

    /**
     * @brief List all available focusers
     */
    auto list() -> json;

    /**
     * @brief Get status of a specific focuser
     */
    auto getStatus(const std::string& deviceId) -> json;

    /**
     * @brief Connect or disconnect a focuser
     */
    auto connect(const std::string& deviceId, bool connected) -> json;

    /**
     * @brief Move focuser (absolute or relative)
     */
    auto move(const std::string& deviceId, const json& moveRequest) -> json;

    /**
     * @brief Update focuser settings
     */
    auto updateSettings(const std::string& deviceId, const json& settings)
        -> json;

    /**
     * @brief Halt focuser movement
     */
    auto halt(const std::string& deviceId) -> json;

    /**
     * @brief Get focuser capabilities
     */
    auto getCapabilities(const std::string& deviceId) -> json;

    /**
     * @brief Start autofocus routine
     */
    auto startAutofocus(const std::string& deviceId,
                        const json& autofocusRequest) -> json;

    /**
     * @brief Get autofocus status
     */
    auto getAutofocusStatus(const std::string& deviceId,
                            const std::string& autofocusId) -> json;

    // ========== INDI-specific operations ==========

    /**
     * @brief Get INDI-specific focuser properties
     */
    auto getINDIProperties(const std::string& deviceId) -> json;

    /**
     * @brief Set INDI-specific focuser property
     */
    auto setINDIProperty(const std::string& deviceId,
                         const std::string& propertyName, const json& value)
        -> json;

    /**
     * @brief Sync focuser position
     */
    auto syncPosition(const std::string& deviceId, int position) -> json;

    /**
     * @brief Get backlash compensation settings
     */
    auto getBacklash(const std::string& deviceId) -> json;

    /**
     * @brief Set backlash compensation
     */
    auto setBacklash(const std::string& deviceId, int steps) -> json;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_FOCUSER_HPP
