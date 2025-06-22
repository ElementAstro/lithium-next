/*
 * asi_camera_core.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Core ASI camera functionality implementation

*************************************************/

#include "asi_camera_core.hpp"
#include "atom/log/loguru.hpp"

#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>

namespace lithium::device::asi::camera {

ASICameraCore::ASICameraCore(const std::string& deviceName)
    : deviceName_(deviceName)
    , name_(deviceName)
    , cameraId_(-1)
    , cameraInfo_(nullptr) {
    LOG_F(INFO, "Created ASI camera core instance: {}", deviceName);
}

ASICameraCore::~ASICameraCore() {
    if (isConnected_) {
        disconnect();
    }
    if (isInitialized_) {
        destroy();
    }
    LOG_F(INFO, "Destroyed ASI camera core instance: {}", name_);
}

auto ASICameraCore::initialize() -> bool {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    
    if (isInitialized_) {
        LOG_F(WARNING, "ASI camera core already initialized");
        return true;
    }

    if (!initializeASISDK()) {
        LOG_F(ERROR, "Failed to initialize ASI SDK");
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
    LOG_F(INFO, "ASI camera core initialized successfully");
    return true;
}

auto ASICameraCore::destroy() -> bool {
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

    shutdownASISDK();
    isInitialized_ = false;
    LOG_F(INFO, "ASI camera core destroyed successfully");
    return true;
}

auto ASICameraCore::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    if (isConnected_) {
        LOG_F(WARNING, "ASI camera already connected");
        return true;
    }

    if (!isInitialized_) {
        LOG_F(ERROR, "ASI camera core not initialized");
        return false;
    }

    // Try to connect with retries
    for (int retry = 0; retry < maxRetry; ++retry) {
        LOG_F(INFO, "Attempting to connect to ASI camera: {} (attempt {}/{})", 
              deviceName, retry + 1, maxRetry);

        cameraId_ = findCameraByName(deviceName.empty() ? deviceName_ : deviceName);
        if (cameraId_ < 0) {
            LOG_F(ERROR, "ASI camera not found: {}", deviceName);
            if (retry < maxRetry - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            continue;
        }

        if (!loadCameraInfo(cameraId_)) {
            LOG_F(ERROR, "Failed to load camera information");
            continue;
        }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
        ASI_ERROR_CODE result = ASIOpenCamera(cameraId_);
        if (result != ASI_SUCCESS) {
            LOG_F(ERROR, "Failed to open ASI camera: {}", result);
            continue;
        }

        result = ASIInitCamera(cameraId_);
        if (result != ASI_SUCCESS) {
            LOG_F(ERROR, "Failed to initialize ASI camera: {}", result);
            ASICloseCamera(cameraId_);
            continue;
        }
#endif

        isConnected_ = true;
        updateCameraState(CameraState::IDLE);
        LOG_F(INFO, "Connected to ASI camera successfully: {}", getCameraModel());
        return true;
    }

    LOG_F(ERROR, "Failed to connect to ASI camera after {} attempts", maxRetry);
    return false;
}

auto ASICameraCore::disconnect() -> bool {
    if (!isConnected_) {
        return true;
    }

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    ASICloseCamera(cameraId_);
#endif

    isConnected_ = false;
    updateCameraState(CameraState::IDLE);
    LOG_F(INFO, "Disconnected from ASI camera");
    return true;
}

auto ASICameraCore::isConnected() const -> bool {
    return isConnected_;
}

auto ASICameraCore::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    int cameraCount = ASIGetNumOfConnectedCameras();
    ASI_CAMERA_INFO info;
    
    for (int i = 0; i < cameraCount; ++i) {
        if (ASIGetCameraProperty(&info, i) == ASI_SUCCESS) {
            devices.emplace_back(info.Name);
        }
    }
#else
    // Stub implementation
    devices.emplace_back("ASI294MC Pro Simulator");
    devices.emplace_back("ASI2600MM Pro Simulator");
    devices.emplace_back("ASI183MC Pro Simulator");
#endif

    LOG_F(INFO, "Found {} ASI cameras", devices.size());
    return devices;
}

auto ASICameraCore::getCameraId() const -> int {
    return cameraId_;
}

auto ASICameraCore::getDeviceName() const -> const std::string& {
    return deviceName_;
}

auto ASICameraCore::getCameraInfo() const -> const ASI_CAMERA_INFO* {
    return cameraInfo_;
}

auto ASICameraCore::registerComponent(std::shared_ptr<ComponentBase> component) -> void {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    components_.push_back(component);
    LOG_F(INFO, "Registered component: {}", component->getComponentName());
}

auto ASICameraCore::unregisterComponent(ComponentBase* component) -> void {
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

auto ASICameraCore::updateCameraState(CameraState state) -> void {
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

auto ASICameraCore::getCameraState() const -> CameraState {
    return currentState_;
}

auto ASICameraCore::getCurrentFrame() -> std::shared_ptr<AtomCameraFrame> {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_;
}

auto ASICameraCore::setCurrentFrame(std::shared_ptr<AtomCameraFrame> frame) -> void {
    std::lock_guard<std::mutex> lock(frameMutex_);
    currentFrame_ = frame;
}

auto ASICameraCore::setControlValue(ASI_CONTROL_TYPE controlType, long value, ASI_BOOL isAuto) -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (!isConnected_) {
        return false;
    }
    
    ASI_ERROR_CODE result = ASISetControlValue(cameraId_, controlType, value, isAuto);
    if (result == ASI_SUCCESS) {
        LOG_F(INFO, "Set ASI control {} to {} (auto: {})", controlType, value, isAuto);
        return true;
    } else {
        LOG_F(ERROR, "Failed to set ASI control {}: {}", controlType, result);
        return false;
    }
#else
    LOG_F(INFO, "Set ASI control {} to {} (auto: {}) [STUB]", controlType, value, isAuto);
    return true;
#endif
}

auto ASICameraCore::getControlValue(ASI_CONTROL_TYPE controlType, long* value, ASI_BOOL* isAuto) -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (!isConnected_ || !value) {
        return false;
    }
    
    ASI_ERROR_CODE result = ASIGetControlValue(cameraId_, controlType, value, isAuto);
    if (result == ASI_SUCCESS) {
        return true;
    } else {
        LOG_F(ERROR, "Failed to get ASI control {}: {}", controlType, result);
        return false;
    }
#else
    if (value) *value = 100;  // Stub value
    if (isAuto) *isAuto = ASI_FALSE;
    return true;
#endif
}

auto ASICameraCore::getControlCaps(ASI_CONTROL_TYPE controlType, ASI_CONTROL_CAPS* caps) -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (!isConnected_ || !caps) {
        return false;
    }
    
    ASI_ERROR_CODE result = ASIGetControlCaps(cameraId_, controlType, caps);
    return result == ASI_SUCCESS;
#else
    if (caps) {
        strcpy(caps->Name, "Stub Control");
        caps->MaxValue = 1000;
        caps->MinValue = 0;
        caps->DefaultValue = 100;
        caps->IsAutoSupported = ASI_TRUE;
        caps->IsWritable = ASI_TRUE;
        caps->ControlType = controlType;
    }
    return true;
#endif
}

auto ASICameraCore::setParameter(const std::string& name, double value) -> void {
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

auto ASICameraCore::getParameter(const std::string& name) -> double {
    std::lock_guard<std::mutex> lock(parametersMutex_);
    auto it = parameters_.find(name);
    return (it != parameters_.end()) ? it->second : 0.0;
}

auto ASICameraCore::hasParameter(const std::string& name) const -> bool {
    std::lock_guard<std::mutex> lock(parametersMutex_);
    return parameters_.find(name) != parameters_.end();
}

auto ASICameraCore::setStateChangeCallback(std::function<void(CameraState)> callback) -> void {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    stateChangeCallback_ = callback;
}

auto ASICameraCore::setParameterChangeCallback(std::function<void(const std::string&, double)> callback) -> void {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    parameterChangeCallback_ = callback;
}

auto ASICameraCore::getSDKVersion() const -> std::string {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    return ASIGetSDKVersion();
#else
    return "ASI SDK 1.32 (Stub)";
#endif
}

auto ASICameraCore::getFirmwareVersion() const -> std::string {
    if (!cameraInfo_) {
        return "Unknown";
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    // ASI SDK doesn't provide direct firmware version access
    return "N/A";
#else
    return "2.1.0 (Stub)";
#endif
}

auto ASICameraCore::getCameraModel() const -> std::string {
    if (!cameraInfo_) {
        return "Unknown";
    }
    return std::string(cameraInfo_->Name);
}

auto ASICameraCore::getSerialNumber() const -> std::string {
    if (!cameraInfo_) {
        return "Unknown";
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    return std::to_string(cameraInfo_->CameraID);
#else
    return "SIM" + std::to_string(cameraInfo_->CameraID);
#endif
}

// Private helper methods
auto ASICameraCore::initializeASISDK() -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    // ASI SDK initializes automatically
    return true;
#else
    LOG_F(INFO, "ASI SDK stub initialized");
    return true;
#endif
}

auto ASICameraCore::shutdownASISDK() -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    // ASI SDK doesn't require explicit shutdown
    return true;
#else
    LOG_F(INFO, "ASI SDK stub shutdown");
    return true;
#endif
}

auto ASICameraCore::findCameraByName(const std::string& name) -> int {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    int cameraCount = ASIGetNumOfConnectedCameras();
    ASI_CAMERA_INFO info;
    
    for (int i = 0; i < cameraCount; ++i) {
        if (ASIGetCameraProperty(&info, i) == ASI_SUCCESS) {
            if (name.empty() || std::string(info.Name) == name) {
                return i;
            }
        }
    }
    return -1;
#else
    // Stub implementation
    static ASI_CAMERA_INFO stubInfo;
    strcpy(stubInfo.Name, name.c_str());
    stubInfo.CameraID = 0;
    stubInfo.MaxWidth = 6248;
    stubInfo.MaxHeight = 4176;
    stubInfo.IsColorCam = 1;
    stubInfo.PixelSize = 4.63;
    cameraInfo_ = &stubInfo;
    return 0;
#endif
}

auto ASICameraCore::loadCameraInfo(int cameraId) -> bool {
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    static ASI_CAMERA_INFO info;
    ASI_ERROR_CODE result = ASIGetCameraProperty(&info, cameraId);
    if (result == ASI_SUCCESS) {
        cameraInfo_ = &info;
        return true;
    }
    return false;
#else
    // Stub implementation already handled in findCameraByName
    return cameraInfo_ != nullptr;
#endif
}

auto ASICameraCore::notifyComponents(CameraState state) -> void {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    for (auto& component : components_) {
        try {
            component->onCameraStateChanged(state);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in component state change notification: {}", e.what());
        }
    }
}

auto ASICameraCore::notifyParameterChange(const std::string& name, double value) -> void {
    std::lock_guard<std::mutex> lock(componentsMutex_);
    for (auto& component : components_) {
        try {
            component->onParameterChanged(name, value);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in component parameter change notification: {}", e.what());
        }
    }
}

} // namespace lithium::device::asi::camera
