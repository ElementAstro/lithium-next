#include "indi_camera_core.hpp"
#include "../component_base.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace lithium::device::indi::camera {

INDICameraCore::INDICameraCore(const std::string& deviceName)
    : deviceName_(deviceName), name_(deviceName) {
    spdlog::info("Creating INDI camera core for device: {}", deviceName);
}

auto INDICameraCore::initialize() -> bool {
    spdlog::info("Initializing INDI camera core for device: {}", deviceName_);

    // Initialize all registered components
    std::lock_guard<std::mutex> lock(componentsMutex_);
    for (auto& component : components_) {
        if (!component->initialize()) {
            spdlog::error("Failed to initialize component: {}", component->getComponentName());
            return false;
        }
    }

    return true;
}

auto INDICameraCore::destroy() -> bool {
    spdlog::info("Destroying INDI camera core for device: {}", deviceName_);

    // Disconnect if connected
    if (isConnected()) {
        disconnect();
    }

    // Destroy all registered components
    std::lock_guard<std::mutex> lock(componentsMutex_);
    for (auto& component : components_) {
        component->destroy();
    }
    components_.clear();

    return true;
}

auto INDICameraCore::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    if (isConnected()) {
        spdlog::warn("Already connected to device: {}", deviceName_);
        return true;
    }

    deviceName_ = deviceName;
    spdlog::info("Connecting to INDI server and watching for device {}...", deviceName_);

    // Set server host and port
    setServer("localhost", 7624);

    // Connect to INDI server
    if (!connectServer()) {
        spdlog::error("Failed to connect to INDI server");
        return false;
    }

    // Setup device watching
    watchDevice(deviceName_.c_str(), [this](INDI::BaseDevice device) {
        spdlog::info("Device {} is now available", device.getDeviceName());
        device_ = device;
        connectDevice(deviceName_.c_str());
    });

    return true;
}

auto INDICameraCore::disconnect() -> bool {
    if (!isConnected()) {
        spdlog::warn("Not connected to any device");
        return true;
    }

    spdlog::info("Disconnecting from {}...", deviceName_);

    // Disconnect the specific device first
    if (!deviceName_.empty()) {
        disconnectDevice(deviceName_.c_str());
    }

    // Disconnect from INDI server
    disconnectServer();

    isConnected_.store(false);
    serverConnected_.store(false);
    updateCameraState(CameraState::IDLE);

    return true;
}

auto INDICameraCore::isConnected() const -> bool {
    return isConnected_.load();
}

auto INDICameraCore::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;
    for (auto& device : getDevices()) {
        devices.push_back(device.getDeviceName());
    }
    return devices;
}

auto INDICameraCore::getDevice() -> INDI::BaseDevice& {
    if (!isConnected()) {
        throw std::runtime_error("Device not connected");
    }
    return device_;
}

auto INDICameraCore::getDevice() const -> const INDI::BaseDevice& {
    if (!isConnected()) {
        throw std::runtime_error("Device not connected");
    }
    return device_;
}

auto INDICameraCore::getDeviceName() const -> const std::string& {
    return deviceName_;
}

auto INDICameraCore::registerComponent(std::shared_ptr<ComponentBase> component) -> void {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    components_.push_back(component);
    spdlog::debug("Registered component: {}", component->getComponentName());
}

auto INDICameraCore::unregisterComponent(ComponentBase* component) -> void {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    components_.erase(
        std::remove_if(components_.begin(), components_.end(),
                      [component](const std::weak_ptr<ComponentBase>& weak_comp) {
                          if (auto comp = weak_comp.lock()) {
                              return comp.get() == component;
                          }
                          return true; // Remove expired weak_ptr
                      }),
        components_.end()
    );
}

auto INDICameraCore::isServerConnected() const -> bool {
    return serverConnected_.load();
}

auto INDICameraCore::updateCameraState(CameraState state) -> void {
    currentState_ = state;
    spdlog::debug("Camera state updated to: {}", static_cast<int>(state));
}

auto INDICameraCore::getCameraState() const -> CameraState {
    return currentState_;
}

auto INDICameraCore::getCurrentFrame() -> std::shared_ptr<AtomCameraFrame> {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_;
}

auto INDICameraCore::setCurrentFrame(std::shared_ptr<AtomCameraFrame> frame) -> void {
    std::lock_guard<std::mutex> lock(frameMutex_);
    currentFrame_ = frame;
}

// INDI BaseClient callback methods
void INDICameraCore::newDevice(INDI::BaseDevice device) {
    if (!device.isValid()) {
        return;
    }

    std::string deviceName = device.getDeviceName();
    spdlog::info("New device discovered: {}", deviceName);

    // Add to devices list
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        devices_.push_back(device);
    }

    // Check if we have a callback for this device
    auto it = deviceCallbacks_.find(deviceName);
    if (it != deviceCallbacks_.end()) {
        it->second(device);
    }
}

void INDICameraCore::removeDevice(INDI::BaseDevice device) {
    if (!device.isValid()) {
        return;
    }

    std::string deviceName = device.getDeviceName();
    spdlog::info("Device removed: {}", deviceName);

    // Remove from devices list
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        devices_.erase(
            std::remove_if(devices_.begin(), devices_.end(),
                          [&deviceName](const INDI::BaseDevice& dev) {
                              return dev.getDeviceName() == deviceName;
                          }),
            devices_.end()
        );
    }

    // If this was our target device, mark as disconnected
    if (deviceName == deviceName_) {
        isConnected_.store(false);
        updateCameraState(CameraState::ERROR);
    }
}

void INDICameraCore::newProperty(INDI::Property property) {
    if (!property.isValid()) {
        return;
    }

    std::string deviceName = property.getDeviceName();
    std::string propertyName = property.getName();

    spdlog::debug("New property: {}.{}", deviceName, propertyName);

    // Handle device-specific properties
    if (deviceName == deviceName_) {
        notifyComponents(property);
    }
}

void INDICameraCore::updateProperty(INDI::Property property) {
    if (!property.isValid()) {
        return;
    }

    std::string deviceName = property.getDeviceName();
    std::string propertyName = property.getName();

    spdlog::debug("Property updated: {}.{}", deviceName, propertyName);

    // Handle device-specific properties
    if (deviceName == deviceName_) {
        notifyComponents(property);
    }
}

void INDICameraCore::removeProperty(INDI::Property property) {
    if (!property.isValid()) {
        return;
    }

    std::string deviceName = property.getDeviceName();
    std::string propertyName = property.getName();

    spdlog::debug("Property removed: {}.{}", deviceName, propertyName);
}

void INDICameraCore::serverConnected() {
    serverConnected_.store(true);
    spdlog::info("Connected to INDI server");
}

void INDICameraCore::serverDisconnected(int exit_code) {
    serverConnected_.store(false);
    isConnected_.store(false);
    updateCameraState(CameraState::ERROR);

    // Clear devices list
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        devices_.clear();
    }

    spdlog::warn("Disconnected from INDI server (exit code: {})", exit_code);
}

void INDICameraCore::sendNewProperty(INDI::Property property) {
    if (!property.isValid()) {
        spdlog::error("Invalid property");
        return;
    }

    if (!serverConnected_.load()) {
        spdlog::error("Not connected to INDI server");
        return;
    }

    INDI::BaseClient::sendNewProperty(property);
}

auto INDICameraCore::getDevices() const -> std::vector<INDI::BaseDevice> {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    return devices_;
}

void INDICameraCore::setPropertyNumber(std::string_view propertyName, double value) {
    if (!isConnected()) {
        spdlog::error("Device not connected");
        return;
    }

    INDI::PropertyNumber property = device_.getProperty(propertyName.data());
    if (property.isValid()) {
        property[0].setValue(value);
        sendNewProperty(property);
    } else {
        spdlog::error("Property {} not found", propertyName);
    }
}

void INDICameraCore::watchDevice(const char* deviceName,
                                const std::function<void(INDI::BaseDevice)>& callback) {
    if (!deviceName) {
        return;
    }

    std::string name(deviceName);
    deviceCallbacks_[name] = callback;

    // Check if device already exists
    std::lock_guard<std::mutex> lock(devicesMutex_);
    for (const auto& device : devices_) {
        if (device.getDeviceName() == name) {
            callback(device);
            return;
        }
    }

    spdlog::info("Watching for device: {}", name);
}

void INDICameraCore::connectDevice(const char* deviceName) {
    if (!deviceName) {
        return;
    }

    if (!serverConnected_.load()) {
        spdlog::error("Not connected to INDI server");
        return;
    }

    // Find device
    INDI::BaseDevice device = findDevice(deviceName);
    if (!device.isValid()) {
        spdlog::error("Device {} not found", deviceName);
        return;
    }

    // Get CONNECTION property
    INDI::PropertySwitch connectProperty = device.getProperty("CONNECTION");
    if (!connectProperty.isValid()) {
        spdlog::error("CONNECTION property not found for device {}", deviceName);
        return;
    }

    // Set CONNECT switch to ON
    connectProperty.reset();
    connectProperty[0].setState(ISS_ON);  // CONNECT
    connectProperty[1].setState(ISS_OFF); // DISCONNECT

    sendNewProperty(connectProperty);
    spdlog::info("Connecting to device: {}", deviceName);
}

void INDICameraCore::disconnectDevice(const char* deviceName) {
    if (!deviceName) {
        return;
    }

    if (!serverConnected_.load()) {
        spdlog::error("Not connected to INDI server");
        return;
    }

    // Find device
    INDI::BaseDevice device = findDevice(deviceName);
    if (!device.isValid()) {
        spdlog::error("Device {} not found", deviceName);
        return;
    }

    // Get CONNECTION property
    INDI::PropertySwitch connectProperty = device.getProperty("CONNECTION");
    if (!connectProperty.isValid()) {
        spdlog::error("CONNECTION property not found for device {}", deviceName);
        return;
    }

    // Set DISCONNECT switch to ON
    connectProperty.reset();
    connectProperty[0].setState(ISS_OFF);  // CONNECT
    connectProperty[1].setState(ISS_ON);   // DISCONNECT

    sendNewProperty(connectProperty);
    spdlog::info("Disconnecting from device: {}", deviceName);
}

void INDICameraCore::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    spdlog::info("New message from {}.{}", baseDevice.getDeviceName(), messageID);
}

// Private helper methods
auto INDICameraCore::findDevice(const std::string& name) -> INDI::BaseDevice {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    for (const auto& device : devices_) {
        if (device.getDeviceName() == name) {
            return device;
        }
    }
    return INDI::BaseDevice();
}

void INDICameraCore::notifyComponents(INDI::Property property) {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    for (auto& component : components_) {
        component->handleProperty(property);
    }
}

} // namespace lithium::device::indi::camera
