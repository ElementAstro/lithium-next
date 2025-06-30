/*
 * qhy_camera_core.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Core QHY camera functionality implementation

*************************************************/

#include "qhy_camera_core.hpp"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <thread>
#include <chrono>

namespace lithium::device::qhy::camera {

QHYCameraCore::QHYCameraCore(const std::string& deviceName)
    : deviceName_(deviceName)
    , name_(deviceName)
    , cameraHandle_(nullptr) {
    LOG_F(INFO, "Created QHY camera core instance: {}", deviceName);
}

QHYCameraCore::~QHYCameraCore() {
    if (isConnected_) {
        disconnect();
    }
    if (isInitialized_) {
        destroy();
    }
    LOG_F(INFO, "Destroyed QHY camera core instance: {}", name_);
}

auto QHYCameraCore::initialize() -> bool {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    
    if (isInitialized_) {
        LOG_F(WARNING, "QHY camera core already initialized");
        return true;
    }

    if (!initializeQHYSDK()) {
        LOG_F(ERROR, "Failed to initialize QHY SDK");
        return false;
    }

    // Initialize all registered components
    for (auto& component : components_) {
        if (!component->initialize()) {
            LOG_F(ERROR, "Failed to initialize component: {}", component->getComponentName());
            return false;
        }
    }

    isInitialized_ = true;
    LOG_F(INFO, "QHY camera core initialized successfully");
    return true;
}

auto QHYCameraCore::destroy() -> bool {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    
    if (!isInitialized_) {
        return true;
    }

    if (isConnected_) {
        disconnect();
    }

    // Destroy all components in reverse order
    for (auto it = components_.rbegin(); it != components_.rend(); ++it) {
        (*it)->destroy();
    }

    shutdownQHYSDK();
    isInitialized_ = false;
    LOG_F(INFO, "QHY camera core destroyed successfully");
    return true;
}

auto QHYCameraCore::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    if (isConnected_) {
        LOG_F(WARNING, "QHY camera already connected");
        return true;
    }

    if (!isInitialized_) {
        LOG_F(ERROR, "QHY camera core not initialized");
        return false;
    }

    // Try to connect with retries
    for (int retry = 0; retry < maxRetry; ++retry) {
        LOG_F(INFO, "Attempting to connect to QHY camera: {} (attempt {}/{})", 
              deviceName, retry + 1, maxRetry);

        cameraId_ = findCameraByName(deviceName.empty() ? deviceName_ : deviceName);
        if (cameraId_.empty()) {
            LOG_F(ERROR, "QHY camera not found: {}", deviceName);
            if (retry < maxRetry - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            continue;
        }

#ifdef LITHIUM_QHY_CAMERA_ENABLED
        cameraHandle_ = OpenQHYCCD(const_cast<char*>(cameraId_.c_str()));
        if (!cameraHandle_) {
            LOG_F(ERROR, "Failed to open QHY camera: {}", cameraId_);
            continue;
        }

        uint32_t result = InitQHYCCD(cameraHandle_);
        if (result != QHYCCD_SUCCESS) {
            LOG_F(ERROR, "Failed to initialize QHY camera: {}", result);
            CloseQHYCCD(cameraHandle_);
            cameraHandle_ = nullptr;
            continue;
        }
#else
        // Stub implementation
        cameraHandle_ = reinterpret_cast<QHYCamHandle*>(0x12345678);
#endif

        if (!loadCameraCapabilities()) {
            LOG_F(ERROR, "Failed to load camera capabilities");
            continue;
        }

        isConnected_ = true;
        updateCameraState(CameraState::IDLE);
        LOG_F(INFO, "Connected to QHY camera successfully: {}", getCameraModel());
        return true;
    }

    LOG_F(ERROR, "Failed to connect to QHY camera after {} attempts", maxRetry);
    return false;
}

auto QHYCameraCore::disconnect() -> bool {
    if (!isConnected_) {
        return true;
    }

#ifdef LITHIUM_QHY_CAMERA_ENABLED
    if (cameraHandle_) {
        CloseQHYCCD(cameraHandle_);
        cameraHandle_ = nullptr;
    }
#else
    cameraHandle_ = nullptr;
#endif

    isConnected_ = false;
    updateCameraState(CameraState::IDLE);
    LOG_F(INFO, "Disconnected from QHY camera");
    return true;
}

auto QHYCameraCore::isConnected() const -> bool {
    return isConnected_;
}

auto QHYCameraCore::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;

#ifdef LITHIUM_QHY_CAMERA_ENABLED
    uint32_t cameraCount = ScanQHYCCD();
    char cameraId[32];
    
    for (uint32_t i = 0; i < cameraCount; ++i) {
        if (GetQHYCCDId(i, cameraId) == QHYCCD_SUCCESS) {
            devices.emplace_back(cameraId);
        }
    }
#else
    // Stub implementation
    devices.emplace_back("QHY268M-12345");
    devices.emplace_back("QHY294C-67890");
    devices.emplace_back("QHY600M-11111");
#endif

    LOG_F(INFO, "Found {} QHY cameras", devices.size());
    return devices;
}

auto QHYCameraCore::getCameraHandle() const -> QHYCamHandle* {
    return cameraHandle_;
}

auto QHYCameraCore::getDeviceName() const -> const std::string& {
    return deviceName_;
}

auto QHYCameraCore::getCameraId() const -> const std::string& {
    return cameraId_;
}

auto QHYCameraCore::registerComponent(std::shared_ptr<ComponentBase> component) -> void {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    components_.push_back(component);
    LOG_F(INFO, "Registered component: {}", component->getComponentName());
}

auto QHYCameraCore::unregisterComponent(ComponentBase* component) -> void {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    components_.erase(
        std::remove_if(components_.begin(), components_.end(),
                      [component](const std::weak_ptr<ComponentBase>& weak_comp) {
                          auto comp = weak_comp.lock();
                          return !comp || comp.get() == component;
                      }),
        components_.end());
    LOG_F(INFO, "Unregistered component");
}

auto QHYCameraCore::updateCameraState(CameraState state) -> void {
    CameraState oldState = currentState_;
    currentState_ = state;
    
    if (oldState != state) {
        LOG_F(INFO, "Camera state changed: {} -> {}", 
              static_cast<int>(oldState), static_cast<int>(state));
        
        notifyComponents(state);
        
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        if (stateChangeCallback_) {
            stateChangeCallback_(state);
        }
    }
}

auto QHYCameraCore::getCameraState() const -> CameraState {
    return currentState_;
}

auto QHYCameraCore::getCurrentFrame() -> std::shared_ptr<AtomCameraFrame> {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_;
}

auto QHYCameraCore::setCurrentFrame(std::shared_ptr<AtomCameraFrame> frame) -> void {
    std::lock_guard<std::mutex> lock(frameMutex_);
    currentFrame_ = frame;
}

auto QHYCameraCore::setControlValue(CONTROL_ID controlId, double value) -> bool {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    if (!isConnected_ || !cameraHandle_) {
        return false;
    }
    
    uint32_t result = SetQHYCCDParam(cameraHandle_, controlId, value);
    if (result == QHYCCD_SUCCESS) {
        LOG_F(INFO, "Set QHY control {} to {}", controlId, value);
        return true;
    } else {
        LOG_F(ERROR, "Failed to set QHY control {}: {}", controlId, result);
        return false;
    }
#else
    LOG_F(INFO, "Set QHY control {} to {} [STUB]", controlId, value);
    return true;
#endif
}

auto QHYCameraCore::getControlValue(CONTROL_ID controlId, double* value) -> bool {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    if (!isConnected_ || !cameraHandle_ || !value) {
        return false;
    }
    
    *value = GetQHYCCDParam(cameraHandle_, controlId);
    return true;
#else
    if (value) *value = 100.0;  // Stub value
    return true;
#endif
}

auto QHYCameraCore::getControlMinMaxStep(CONTROL_ID controlId, double* min, double* max, double* step) -> bool {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    if (!isConnected_ || !cameraHandle_) {
        return false;
    }
    
    uint32_t result = GetQHYCCDParamMinMaxStep(cameraHandle_, controlId, min, max, step);
    return result == QHYCCD_SUCCESS;
#else
    if (min) *min = 0.0;
    if (max) *max = 1000.0;
    if (step) *step = 1.0;
    return true;
#endif
}

auto QHYCameraCore::isControlAvailable(CONTROL_ID controlId) -> bool {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    if (!isConnected_ || !cameraHandle_) {
        return false;
    }
    
    uint32_t result = IsQHYCCDControlAvailable(cameraHandle_, controlId);
    return result == QHYCCD_SUCCESS;
#else
    return true;  // Stub - assume all controls available
#endif
}

auto QHYCameraCore::setParameter(const std::string& name, double value) -> void {
    {
        std::lock_guard<std::mutex> lock(parametersMutex_);
        parameters_[name] = value;
    }
    
    notifyParameterChange(name, value);
    
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    if (parameterChangeCallback_) {
        parameterChangeCallback_(name, value);
    }
}

auto QHYCameraCore::getParameter(const std::string& name) -> double {
    std::lock_guard<std::mutex> lock(parametersMutex_);
    auto it = parameters_.find(name);
    return (it != parameters_.end()) ? it->second : 0.0;
}

auto QHYCameraCore::hasParameter(const std::string& name) const -> bool {
    std::lock_guard<std::mutex> lock(parametersMutex_);
    return parameters_.find(name) != parameters_.end();
}

auto QHYCameraCore::setStateChangeCallback(std::function<void(CameraState)> callback) -> void {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    stateChangeCallback_ = callback;
}

auto QHYCameraCore::setParameterChangeCallback(std::function<void(const std::string&, double)> callback) -> void {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    parameterChangeCallback_ = callback;
}

auto QHYCameraCore::getSDKVersion() const -> std::string {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    uint32_t year, month, day, subday;
    GetQHYCCDSDKVersion(&year, &month, &day, &subday);
    return std::to_string(year) + "." + std::to_string(month) + "." + 
           std::to_string(day) + "." + std::to_string(subday);
#else
    return "2023.12.18.1 (Stub)";
#endif
}

auto QHYCameraCore::getFirmwareVersion() const -> std::string {
    return firmwareVersion_;
}

auto QHYCameraCore::getCameraModel() const -> std::string {
    return cameraType_;
}

auto QHYCameraCore::getSerialNumber() const -> std::string {
    return serialNumber_;
}

auto QHYCameraCore::enableUSB3Traffic(bool enable) -> bool {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    if (!isConnected_ || !cameraHandle_) {
        return false;
    }
    
    if (isControlAvailable(CONTROL_USBTRAFFIC)) {
        double traffic = enable ? 100.0 : 30.0;  // Default values
        return setControlValue(CONTROL_USBTRAFFIC, traffic);
    }
#endif
    return true;
}

auto QHYCameraCore::setUSB3Traffic(int traffic) -> bool {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    if (!isConnected_ || !cameraHandle_) {
        return false;
    }
    
    if (isControlAvailable(CONTROL_USBTRAFFIC)) {
        return setControlValue(CONTROL_USBTRAFFIC, static_cast<double>(traffic));
    }
#endif
    return true;
}

auto QHYCameraCore::getUSB3Traffic() -> int {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    if (!isConnected_ || !cameraHandle_) {
        return 0;
    }
    
    double traffic = 0.0;
    if (getControlValue(CONTROL_USBTRAFFIC, &traffic)) {
        return static_cast<int>(traffic);
    }
#endif
    return 30;  // Default value
}

auto QHYCameraCore::getCameraType() const -> std::string {
    return cameraType_;
}

auto QHYCameraCore::hasColorCamera() const -> bool {
    return hasColorCamera_;
}

auto QHYCameraCore::hasCooler() const -> bool {
    return hasCooler_;
}

auto QHYCameraCore::hasFilterWheel() const -> bool {
    return hasFilterWheel_;
}

// Private helper methods
auto QHYCameraCore::initializeQHYSDK() -> bool {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    uint32_t result = InitQHYCCDResource();
    if (result != QHYCCD_SUCCESS) {
        LOG_F(ERROR, "Failed to initialize QHY SDK: {}", result);
        return false;
    }
    return true;
#else
    LOG_F(INFO, "QHY SDK stub initialized");
    return true;
#endif
}

auto QHYCameraCore::shutdownQHYSDK() -> bool {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    uint32_t result = ReleaseQHYCCDResource();
    if (result != QHYCCD_SUCCESS) {
        LOG_F(ERROR, "Failed to shutdown QHY SDK: {}", result);
        return false;
    }
    return true;
#else
    LOG_F(INFO, "QHY SDK stub shutdown");
    return true;
#endif
}

auto QHYCameraCore::findCameraByName(const std::string& name) -> std::string {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    uint32_t cameraCount = ScanQHYCCD();
    char cameraId[32];
    
    for (uint32_t i = 0; i < cameraCount; ++i) {
        if (GetQHYCCDId(i, cameraId) == QHYCCD_SUCCESS) {
            if (name.empty() || std::string(cameraId).find(name) != std::string::npos) {
                return std::string(cameraId);
            }
        }
    }
    return "";
#else
    // Stub implementation
    return name + "-SIM12345";
#endif
}

auto QHYCameraCore::loadCameraCapabilities() -> bool {
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    if (!cameraHandle_) {
        return false;
    }
    
    // Detect hardware features
    hasColorCamera_ = isControlAvailable(CONTROL_WBR) && isControlAvailable(CONTROL_WBB);
    hasCooler_ = isControlAvailable(CONTROL_COOLER);
    hasFilterWheel_ = isControlAvailable(CONTROL_CFW);
    hasUSB3_ = isControlAvailable(CONTROL_USBTRAFFIC);
    
    // Get camera type from ID
    cameraType_ = cameraId_;
    
    // Try to get firmware version if available
    firmwareVersion_ = "N/A";
    
    // Generate serial number from camera ID
    serialNumber_ = cameraId_;
    
    return true;
#else
    // Stub implementation
    hasColorCamera_ = deviceName_.find("C") != std::string::npos;
    hasCooler_ = true;
    hasFilterWheel_ = deviceName_.find("CFW") != std::string::npos;
    hasUSB3_ = true;
    cameraType_ = deviceName_;
    firmwareVersion_ = "2.1.0 (Stub)";
    serialNumber_ = "SIM12345";
    return true;
#endif
}

auto QHYCameraCore::detectHardwareFeatures() -> bool {
    return loadCameraCapabilities();
}

auto QHYCameraCore::notifyComponents(CameraState state) -> void {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    for (auto& component : components_) {
        try {
            component->onCameraStateChanged(state);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in component state change notification: {}", e.what());
        }
    }
}

auto QHYCameraCore::notifyParameterChange(const std::string& name, double value) -> void {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    for (auto& component : components_) {
        try {
            component->onParameterChanged(name, value);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in component parameter change notification: {}", e.what());
        }
    }
}

} // namespace lithium::device::qhy::camera
