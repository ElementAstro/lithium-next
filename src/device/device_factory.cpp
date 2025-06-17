/*
 * device_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "device_factory.hpp"

std::unique_ptr<AtomCamera> DeviceFactory::createCamera(const std::string& name, DeviceBackend backend) {
    switch (backend) {
        case DeviceBackend::MOCK:
            return std::make_unique<MockCamera>(name);
        case DeviceBackend::INDI:
            // TODO: Create INDI camera when available
            break;
        case DeviceBackend::ASCOM:
            // TODO: Create ASCOM camera when available
            break;
        case DeviceBackend::NATIVE:
            // TODO: Create native camera when available
            break;
    }
    
    // Fallback to mock
    return std::make_unique<MockCamera>(name);
}

std::unique_ptr<AtomTelescope> DeviceFactory::createTelescope(const std::string& name, DeviceBackend backend) {
    switch (backend) {
        case DeviceBackend::MOCK:
            return std::make_unique<MockTelescope>(name);
        case DeviceBackend::INDI:
            // TODO: Create INDI telescope when available
            break;
        case DeviceBackend::ASCOM:
            // TODO: Create ASCOM telescope when available
            break;
        case DeviceBackend::NATIVE:
            // TODO: Create native telescope when available
            break;
    }
    
    // Fallback to mock
    return std::make_unique<MockTelescope>(name);
}

std::unique_ptr<AtomFocuser> DeviceFactory::createFocuser(const std::string& name, DeviceBackend backend) {
    switch (backend) {
        case DeviceBackend::MOCK:
            return std::make_unique<MockFocuser>(name);
        case DeviceBackend::INDI:
            // TODO: Create INDI focuser when available
            break;
        case DeviceBackend::ASCOM:
            // TODO: Create ASCOM focuser when available
            break;
        case DeviceBackend::NATIVE:
            // TODO: Create native focuser when available
            break;
    }
    
    // Fallback to mock
    return std::make_unique<MockFocuser>(name);
}

std::unique_ptr<AtomFilterWheel> DeviceFactory::createFilterWheel(const std::string& name, DeviceBackend backend) {
    switch (backend) {
        case DeviceBackend::MOCK:
            return std::make_unique<MockFilterWheel>(name);
        case DeviceBackend::INDI:
            // TODO: Create INDI filter wheel when available
            break;
        case DeviceBackend::ASCOM:
            // TODO: Create ASCOM filter wheel when available
            break;
        case DeviceBackend::NATIVE:
            // TODO: Create native filter wheel when available
            break;
    }
    
    // Fallback to mock
    return std::make_unique<MockFilterWheel>(name);
}

std::unique_ptr<AtomRotator> DeviceFactory::createRotator(const std::string& name, DeviceBackend backend) {
    switch (backend) {
        case DeviceBackend::MOCK:
            return std::make_unique<MockRotator>(name);
        case DeviceBackend::INDI:
            // TODO: Create INDI rotator when available
            break;
        case DeviceBackend::ASCOM:
            // TODO: Create ASCOM rotator when available
            break;
        case DeviceBackend::NATIVE:
            // TODO: Create native rotator when available
            break;
    }
    
    // Fallback to mock
    return std::make_unique<MockRotator>(name);
}

std::unique_ptr<AtomDome> DeviceFactory::createDome(const std::string& name, DeviceBackend backend) {
    switch (backend) {
        case DeviceBackend::MOCK:
            return std::make_unique<MockDome>(name);
        case DeviceBackend::INDI:
            // TODO: Create INDI dome when available
            break;
        case DeviceBackend::ASCOM:
            // TODO: Create ASCOM dome when available
            break;
        case DeviceBackend::NATIVE:
            // TODO: Create native dome when available
            break;
    }
    
    // Fallback to mock
    return std::make_unique<MockDome>(name);
}

std::unique_ptr<AtomDriver> DeviceFactory::createDevice(DeviceType type, const std::string& name, DeviceBackend backend) {
    // Check if we have a custom creator registered
    std::string key = makeRegistryKey(type, backend);
    auto it = device_creators_.find(key);
    if (it != device_creators_.end()) {
        return it->second(name);
    }
    
    // Use built-in creators
    switch (type) {
        case DeviceType::CAMERA:
            return createCamera(name, backend);
        case DeviceType::TELESCOPE:
            return createTelescope(name, backend);
        case DeviceType::FOCUSER:
            return createFocuser(name, backend);
        case DeviceType::FILTERWHEEL:
            return createFilterWheel(name, backend);
        case DeviceType::ROTATOR:
            return createRotator(name, backend);
        case DeviceType::DOME:
            return createDome(name, backend);
        case DeviceType::GUIDER:
            // TODO: Implement guider creation
            break;
        case DeviceType::WEATHER_STATION:
            // TODO: Implement weather station creation
            break;
        case DeviceType::SAFETY_MONITOR:
            // TODO: Implement safety monitor creation
            break;
        case DeviceType::ADAPTIVE_OPTICS:
            // TODO: Implement adaptive optics creation
            break;
        default:
            break;
    }
    
    return nullptr;
}

std::vector<DeviceBackend> DeviceFactory::getAvailableBackends(DeviceType type) const {
    std::vector<DeviceBackend> backends;
    
    // Mock backend is always available
    backends.push_back(DeviceBackend::MOCK);
    
    // Check for INDI availability
    if (isINDIAvailable()) {
        backends.push_back(DeviceBackend::INDI);
    }
    
    // Check for ASCOM availability
    if (isASCOMAvailable()) {
        backends.push_back(DeviceBackend::ASCOM);
    }
    
    // Check for native drivers
    backends.push_back(DeviceBackend::NATIVE);
    
    return backends;
}

bool DeviceFactory::isBackendAvailable(DeviceType type, DeviceBackend backend) const {
    switch (backend) {
        case DeviceBackend::MOCK:
            return true; // Always available
        case DeviceBackend::INDI:
            return isINDIAvailable();
        case DeviceBackend::ASCOM:
            return isASCOMAvailable();
        case DeviceBackend::NATIVE:
            return true; // TODO: Implement proper checking
        default:
            return false;
    }
}

std::vector<DeviceFactory::DeviceInfo> DeviceFactory::discoverDevices(DeviceType type, DeviceBackend backend) const {
    std::vector<DeviceInfo> devices;
    
    if (backend == DeviceBackend::MOCK || backend == DeviceBackend::MOCK) {
        // Add mock devices
        if (type == DeviceType::CAMERA || type == DeviceType::UNKNOWN) {
            devices.push_back({"MockCamera", DeviceType::CAMERA, DeviceBackend::MOCK, "Simulated camera device", "1.0.0"});
        }
        if (type == DeviceType::TELESCOPE || type == DeviceType::UNKNOWN) {
            devices.push_back({"MockTelescope", DeviceType::TELESCOPE, DeviceBackend::MOCK, "Simulated telescope mount", "1.0.0"});
        }
        if (type == DeviceType::FOCUSER || type == DeviceType::UNKNOWN) {
            devices.push_back({"MockFocuser", DeviceType::FOCUSER, DeviceBackend::MOCK, "Simulated focuser device", "1.0.0"});
        }
        if (type == DeviceType::FILTERWHEEL || type == DeviceType::UNKNOWN) {
            devices.push_back({"MockFilterWheel", DeviceType::FILTERWHEEL, DeviceBackend::MOCK, "Simulated filter wheel", "1.0.0"});
        }
        if (type == DeviceType::ROTATOR || type == DeviceType::UNKNOWN) {
            devices.push_back({"MockRotator", DeviceType::ROTATOR, DeviceBackend::MOCK, "Simulated field rotator", "1.0.0"});
        }
        if (type == DeviceType::DOME || type == DeviceType::UNKNOWN) {
            devices.push_back({"MockDome", DeviceType::DOME, DeviceBackend::MOCK, "Simulated observatory dome", "1.0.0"});
        }
    }
    
    // TODO: Add INDI device discovery
    // TODO: Add ASCOM device discovery
    // TODO: Add native device discovery
    
    return devices;
}

void DeviceFactory::registerDeviceCreator(DeviceType type, DeviceBackend backend, DeviceCreator creator) {
    std::string key = makeRegistryKey(type, backend);
    device_creators_[key] = std::move(creator);
}

bool DeviceFactory::isINDIAvailable() const {
    // TODO: Check if INDI libraries are available and indiserver is running
    return false;
}

bool DeviceFactory::isASCOMAvailable() const {
    // TODO: Check if ASCOM platform is available (Windows only)
#ifdef _WIN32
    return false; // TODO: Implement ASCOM detection
#else
    return false;
#endif
}
