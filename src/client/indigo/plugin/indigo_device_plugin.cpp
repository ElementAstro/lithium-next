/*
 * indigo_device_plugin.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "indigo_device_plugin.hpp"

#include "../indigo_client.hpp"
#include "device/plugin/device_plugin_interface.hpp"
#include "../indigo_device_factory.hpp"
#include "../indigo_camera.hpp"
#include "../indigo_focuser.hpp"
#include "../indigo_filterwheel.hpp"
#include "../indigo_telescope.hpp"
#include "../indigo_dome.hpp"
#include "../indigo_rotator.hpp"
#include "../indigo_weather.hpp"
#include "../indigo_gps.hpp"

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

// ============================================================================
// INDIGO Backend Implementation
// ============================================================================

class INDIGOBackend : public device::DeviceBackend {
public:
    explicit INDIGOBackend(std::shared_ptr<INDIGOClient> client)
        : client_(std::move(client)) {}

    [[nodiscard]] auto getName() const noexcept -> std::string_view override {
        return "INDIGO";
    }

    [[nodiscard]] auto isConnected() const -> bool override {
        return client_ && client_->isConnected();
    }

    auto connect(const std::string& host, int port) -> device::DeviceResult<bool> override {
        if (!client_) {
            INDIGOClient::Config config;
            config.host = host;
            config.port = port;
            client_ = std::make_shared<INDIGOClient>(config);
        }
        return client_->connect(host, port);
    }

    auto disconnect() -> device::DeviceResult<bool> override {
        if (client_) {
            return client_->disconnect();
        }
        return device::DeviceResult<bool>(true);
    }

    auto discoverDevices() -> device::DeviceResult<std::vector<device::DiscoveredDeviceInfo>> override {
        if (!client_ || !client_->isConnected()) {
            return device::DeviceError{device::DeviceErrorCode::NotConnected,
                                       "Not connected to INDIGO server"};
        }

        auto result = client_->discoverDevices();
        if (!result.has_value()) {
            return device::DeviceError{result.error()};
        }

        std::vector<device::DiscoveredDeviceInfo> devices;
        for (const auto& dev : result.value()) {
            device::DiscoveredDeviceInfo info;
            info.name = dev.name;
            info.driver = dev.driver;
            info.version = dev.version;
            info.connected = dev.connected;
            info.backend = "INDIGO";

            // Map INDIGO interfaces to device type
            auto type = INDIGODeviceFactory::inferDeviceType(dev.interfaces);
            info.type = INDIGODeviceFactory::deviceTypeToString(type);

            devices.push_back(info);
        }

        return devices;
    }

    auto createDevice(const std::string& deviceName, const std::string& deviceType)
        -> device::DeviceResult<std::shared_ptr<device::AtomDriver>> override {
        auto result = INDIGODeviceFactory::getInstance().createDevice(
            deviceType, deviceName, client_);

        if (!result.has_value()) {
            return device::DeviceError{result.error()};
        }

        return std::static_pointer_cast<device::AtomDriver>(result.value());
    }

    [[nodiscard]] auto getClient() const -> std::shared_ptr<INDIGOClient> {
        return client_;
    }

private:
    std::shared_ptr<INDIGOClient> client_;
};

// ============================================================================
// INDIGODevicePlugin Implementation
// ============================================================================

class INDIGODevicePlugin::Impl {
public:
    Impl() = default;

    auto initialize() -> bool {
        LOG_F(INFO, "INDIGO Plugin: Initializing");

#if !INDIGO_PLATFORM_SUPPORTED
        LOG_F(WARNING, "INDIGO Plugin: Platform not supported (Windows)");
#endif

        initialized_ = true;
        return true;
    }

    auto shutdown() -> bool {
        LOG_F(INFO, "INDIGO Plugin: Shutting down");

        if (client_) {
            client_->disconnect();
            client_.reset();
        }

        INDIGODeviceFactory::getInstance().clearClientPool();
        initialized_ = false;
        return true;
    }

    [[nodiscard]] auto getDeviceTypes() const -> std::vector<device::DeviceTypeInfo> {
        std::vector<device::DeviceTypeInfo> types;

        // Camera
        types.push_back({
            .typeName = "INDIGO:Camera",
            .category = "Camera",
            .description = "INDIGO CCD/CMOS Camera",
            .capabilities = device::DeviceCapabilities::Exposure |
                           device::DeviceCapabilities::Temperature |
                           device::DeviceCapabilities::Gain
        });

        // Focuser
        types.push_back({
            .typeName = "INDIGO:Focuser",
            .category = "Focuser",
            .description = "INDIGO Focuser",
            .capabilities = device::DeviceCapabilities::Position |
                           device::DeviceCapabilities::Temperature
        });

        // FilterWheel
        types.push_back({
            .typeName = "INDIGO:FilterWheel",
            .category = "FilterWheel",
            .description = "INDIGO Filter Wheel",
            .capabilities = device::DeviceCapabilities::FilterSelection
        });

        // Telescope/Mount
        types.push_back({
            .typeName = "INDIGO:Telescope",
            .category = "Telescope",
            .description = "INDIGO Telescope/Mount",
            .capabilities = device::DeviceCapabilities::Goto |
                           device::DeviceCapabilities::Tracking |
                           device::DeviceCapabilities::Park
        });

        // Dome
        types.push_back({
            .typeName = "INDIGO:Dome",
            .category = "Dome",
            .description = "INDIGO Dome",
            .capabilities = device::DeviceCapabilities::Shutter |
                           device::DeviceCapabilities::Rotation |
                           device::DeviceCapabilities::Park
        });

        // Rotator
        types.push_back({
            .typeName = "INDIGO:Rotator",
            .category = "Rotator",
            .description = "INDIGO Rotator",
            .capabilities = device::DeviceCapabilities::Rotation
        });

        // Weather
        types.push_back({
            .typeName = "INDIGO:Weather",
            .category = "Weather",
            .description = "INDIGO Weather Station",
            .capabilities = device::DeviceCapabilities::Weather
        });

        // GPS
        types.push_back({
            .typeName = "INDIGO:GPS",
            .category = "GPS",
            .description = "INDIGO GPS Device",
            .capabilities = device::DeviceCapabilities::Location |
                           device::DeviceCapabilities::Time
        });

        return types;
    }

    void registerDeviceCreators(device::DeviceFactory& factory) {
        LOG_F(INFO, "INDIGO Plugin: Registering device creators");

        // Register creators for each device type
        factory.registerCreator("INDIGO:Camera",
            [](const std::string& name) -> std::shared_ptr<device::AtomDriver> {
                return std::make_shared<INDIGOCamera>(name);
            });

        factory.registerCreator("INDIGO:Focuser",
            [](const std::string& name) -> std::shared_ptr<device::AtomDriver> {
                return std::make_shared<INDIGOFocuser>(name);
            });

        factory.registerCreator("INDIGO:FilterWheel",
            [](const std::string& name) -> std::shared_ptr<device::AtomDriver> {
                return std::make_shared<INDIGOFilterWheel>(name);
            });

        factory.registerCreator("INDIGO:Telescope",
            [](const std::string& name) -> std::shared_ptr<device::AtomDriver> {
                return std::make_shared<INDIGOTelescope>(name);
            });

        factory.registerCreator("INDIGO:Dome",
            [](const std::string& name) -> std::shared_ptr<device::AtomDriver> {
                return std::make_shared<INDIGODome>(name);
            });

        factory.registerCreator("INDIGO:Rotator",
            [](const std::string& name) -> std::shared_ptr<device::AtomDriver> {
                return std::make_shared<INDIGORotator>(name);
            });

        factory.registerCreator("INDIGO:Weather",
            [](const std::string& name) -> std::shared_ptr<device::AtomDriver> {
                return std::make_shared<INDIGOWeather>(name);
            });

        factory.registerCreator("INDIGO:GPS",
            [](const std::string& name) -> std::shared_ptr<device::AtomDriver> {
                return std::make_shared<INDIGOGPS>(name);
            });
    }

    [[nodiscard]] auto createBackend() -> std::shared_ptr<device::DeviceBackend> {
        if (!client_) {
            client_ = std::make_shared<INDIGOClient>();
        }
        return std::make_shared<INDIGOBackend>(client_);
    }

    void enableBlobUrlMode(bool enable) {
        blobUrlMode_ = enable;
        if (client_ && client_->isConnected()) {
            client_->enableBlob("", true, enable);
        }
    }

    [[nodiscard]] auto getClient() -> std::shared_ptr<INDIGOClient> {
        if (!client_) {
            client_ = std::make_shared<INDIGOClient>();
        }
        return client_;
    }

private:
    bool initialized_{false};
    bool blobUrlMode_{true};
    std::shared_ptr<INDIGOClient> client_;
};

// ============================================================================
// INDIGODevicePlugin Public Interface
// ============================================================================

INDIGODevicePlugin::INDIGODevicePlugin() : impl_(std::make_unique<Impl>()) {}

INDIGODevicePlugin::~INDIGODevicePlugin() = default;

auto INDIGODevicePlugin::getName() const noexcept -> std::string_view {
    return PLUGIN_NAME;
}

auto INDIGODevicePlugin::getVersion() const noexcept -> std::string_view {
    return PLUGIN_VERSION;
}

auto INDIGODevicePlugin::getDescription() const noexcept -> std::string_view {
    return PLUGIN_DESCRIPTION;
}

auto INDIGODevicePlugin::initialize() -> bool {
    return impl_->initialize();
}

auto INDIGODevicePlugin::shutdown() -> bool {
    return impl_->shutdown();
}

auto INDIGODevicePlugin::getDeviceTypes() const -> std::vector<device::DeviceTypeInfo> {
    return impl_->getDeviceTypes();
}

void INDIGODevicePlugin::registerDeviceCreators(device::DeviceFactory& factory) {
    impl_->registerDeviceCreators(factory);
}

auto INDIGODevicePlugin::createBackend() -> std::shared_ptr<device::DeviceBackend> {
    return impl_->createBackend();
}

void INDIGODevicePlugin::enableBlobUrlMode(bool enable) {
    impl_->enableBlobUrlMode(enable);
}

auto INDIGODevicePlugin::getClient() -> std::shared_ptr<INDIGOClient> {
    return impl_->getClient();
}

}  // namespace lithium::client::indigo

// ============================================================================
// Plugin Export Functions
// ============================================================================

extern "C" {

// Standard device plugin entry point
LITHIUM_PLUGIN_EXPORT auto createDevicePlugin() -> lithium::device::IDevicePlugin* {
    return new lithium::client::indigo::INDIGODevicePlugin();
}

// Standard device plugin destruction
LITHIUM_PLUGIN_EXPORT void destroyDevicePlugin(lithium::device::IDevicePlugin* plugin) {
    delete plugin;
}

// Device plugin API version
LITHIUM_PLUGIN_EXPORT auto getDevicePluginApiVersion() -> int {
    return lithium::device::DEVICE_PLUGIN_API_VERSION;
}

// Backward compatibility - kept for legacy support
LITHIUM_PLUGIN_EXPORT auto createPlugin() -> lithium::device::IDevicePlugin* {
    return createDevicePlugin();
}

LITHIUM_PLUGIN_EXPORT void destroyPlugin(lithium::device::IDevicePlugin* plugin) {
    destroyDevicePlugin(plugin);
}

LITHIUM_PLUGIN_EXPORT auto getPluginInfo() -> const char* {
    return R"({
        "name": "INDIGO",
        "version": "1.0.0",
        "description": "INDIGO device driver plugin for Lithium",
        "author": "Max Qian",
        "license": "GPL3",
        "backend": "INDIGO",
        "supportsHotPlug": true,
        "supportsAutoDiscovery": true,
        "deviceTypes": ["Camera", "Focuser", "FilterWheel", "Telescope", "Dome", "Rotator", "Weather", "GPS"]
    })";
}

}
