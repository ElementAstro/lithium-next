/*
 * indigo_device_plugin.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Device Plugin for Lithium

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_DEVICE_PLUGIN_HPP
#define LITHIUM_CLIENT_INDIGO_DEVICE_PLUGIN_HPP

#include "device/plugin/device_plugin_interface.hpp"

#include <memory>
#include <string>
#include <vector>

namespace lithium::client::indigo {

class INDIGOClient;

/**
 * @brief INDIGO Device Plugin
 *
 * Plugin that provides INDIGO device support for Lithium.
 * Registers device types and creators for all INDIGO device classes.
 */
class INDIGODevicePlugin : public device::DevicePluginBase {
public:
    static constexpr const char* PLUGIN_NAME = "INDIGO";
    static constexpr const char* PLUGIN_VERSION = "1.0.0";
    static constexpr const char* PLUGIN_DESCRIPTION =
        "INDIGO device driver plugin for Lithium";

    INDIGODevicePlugin();
    ~INDIGODevicePlugin() override;

    // ==================== IPlugin Interface ====================

    [[nodiscard]] auto getName() const noexcept -> std::string_view override;
    [[nodiscard]] auto getVersion() const noexcept -> std::string_view override;
    [[nodiscard]] auto getDescription() const noexcept -> std::string_view override;

    auto initialize() -> bool override;
    auto shutdown() -> bool override;

    // ==================== IDevicePlugin Interface ====================

    [[nodiscard]] auto getDeviceTypes() const
        -> std::vector<device::DeviceTypeInfo> override;

    void registerDeviceCreators(device::DeviceFactory& factory) override;

    [[nodiscard]] auto createBackend()
        -> std::shared_ptr<device::DeviceBackend> override;

    [[nodiscard]] auto supportsHotPlug() const noexcept -> bool override {
        return true;
    }

    // ==================== INDIGO Specific ====================

    /**
     * @brief Enable BLOB URL mode for efficient image transfer
     */
    void enableBlobUrlMode(bool enable);

    /**
     * @brief Get shared INDIGO client instance
     */
    [[nodiscard]] auto getClient() -> std::shared_ptr<INDIGOClient>;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::client::indigo

// Plugin export macros
extern "C" {
    LITHIUM_PLUGIN_EXPORT auto createPlugin() -> lithium::device::IDevicePlugin*;
    LITHIUM_PLUGIN_EXPORT void destroyPlugin(lithium::device::IDevicePlugin* plugin);
    LITHIUM_PLUGIN_EXPORT auto getPluginInfo() -> const char*;
}

#endif  // LITHIUM_CLIENT_INDIGO_DEVICE_PLUGIN_HPP
