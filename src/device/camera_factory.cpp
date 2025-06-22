/*
 * camera_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Enhanced Camera Factory implementation

*************************************************/

#include "camera_factory.hpp"
#include "indi/camera/indi_camera.hpp"

#ifdef LITHIUM_QHY_CAMERA_ENABLED
#include "qhy/camera/qhy_camera.hpp"
#endif

#ifdef LITHIUM_ASI_CAMERA_ENABLED
#include "asi/camera/asi_camera.hpp"
#endif

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
#include "atik/atik_camera.hpp"
#endif

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
#include "sbig/sbig_camera.hpp"
#endif

#ifdef LITHIUM_FLI_CAMERA_ENABLED
#include "fli/fli_camera.hpp"
#endif

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
#include "playerone/playerone_camera.hpp"
#endif

#ifdef LITHIUM_ASCOM_CAMERA_ENABLED
#include "ascom/camera.hpp"
#endif

#include "template/mock/mock_camera.hpp"

#include <algorithm>
#include <regex>

namespace lithium::device {

CameraFactory& CameraFactory::getInstance() {
    static CameraFactory instance;
    if (!instance.initialized_) {
        instance.initializeDefaultDrivers();
        instance.initialized_ = true;
    }
    return instance;
}

void CameraFactory::registerCameraDriver(CameraDriverType type, CreateCameraFunction createFunc) {
    drivers_[type] = std::move(createFunc);
    LOG_F(INFO, "Registered camera driver: {}", driverTypeToString(type));
}

std::shared_ptr<AtomCamera> CameraFactory::createCamera(CameraDriverType type, const std::string& name) {
    auto it = drivers_.find(type);
    if (it == drivers_.end()) {
        LOG_F(ERROR, "Camera driver type not supported: {}", driverTypeToString(type));
        return nullptr;
    }

    try {
        auto camera = it->second(name);
        if (camera) {
            LOG_F(INFO, "Created {} camera: {}", driverTypeToString(type), name);
        } else {
            LOG_F(ERROR, "Failed to create {} camera: {}", driverTypeToString(type), name);
        }
        return camera;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception creating {} camera '{}': {}", driverTypeToString(type), name, e.what());
        return nullptr;
    }
}

std::shared_ptr<AtomCamera> CameraFactory::createCamera(const std::string& name) {
    LOG_F(INFO, "Auto-detecting camera driver for: {}", name);

    // Try to auto-detect the appropriate driver based on camera name/identifier
    std::vector<CameraDriverType> tryOrder;

    // Heuristics for driver selection based on name patterns
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    if (lowerName.find("qhy") != std::string::npos ||
        lowerName.find("quantum") != std::string::npos) {
        tryOrder = {CameraDriverType::QHY, CameraDriverType::INDI, CameraDriverType::SIMULATOR};
    } else if (lowerName.find("asi") != std::string::npos ||
               lowerName.find("zwo") != std::string::npos) {
        tryOrder = {CameraDriverType::ASI, CameraDriverType::INDI, CameraDriverType::SIMULATOR};
    } else if (lowerName.find("atik") != std::string::npos ||
               lowerName.find("titan") != std::string::npos ||
               lowerName.find("infinity") != std::string::npos) {
        tryOrder = {CameraDriverType::ATIK, CameraDriverType::INDI, CameraDriverType::SIMULATOR};
    } else if (lowerName.find("sbig") != std::string::npos ||
               lowerName.find("st-") != std::string::npos) {
        tryOrder = {CameraDriverType::SBIG, CameraDriverType::INDI, CameraDriverType::SIMULATOR};
    } else if (lowerName.find("fli") != std::string::npos ||
               lowerName.find("microline") != std::string::npos ||
               lowerName.find("proline") != std::string::npos) {
        tryOrder = {CameraDriverType::FLI, CameraDriverType::INDI, CameraDriverType::SIMULATOR};
    } else if (lowerName.find("playerone") != std::string::npos ||
               lowerName.find("player one") != std::string::npos ||
               lowerName.find("poa") != std::string::npos) {
        tryOrder = {CameraDriverType::PLAYERONE, CameraDriverType::INDI, CameraDriverType::SIMULATOR};
    } else if (lowerName.find("ascom") != std::string::npos ||
               lowerName.find(".") != std::string::npos) { // ASCOM ProgID pattern
        tryOrder = {CameraDriverType::ASCOM, CameraDriverType::INDI, CameraDriverType::SIMULATOR};
    } else if (lowerName.find("simulator") != std::string::npos ||
               lowerName.find("sim") != std::string::npos) {
        tryOrder = {CameraDriverType::SIMULATOR};
    } else {
        // Default order: try INDI first (most universal), then others
        tryOrder = {CameraDriverType::INDI, CameraDriverType::QHY, CameraDriverType::ASI, 
                   CameraDriverType::ATIK, CameraDriverType::SBIG, CameraDriverType::FLI,
                   CameraDriverType::PLAYERONE, CameraDriverType::ASCOM, CameraDriverType::SIMULATOR};
    }

    // Try each driver in order
    for (auto type : tryOrder) {
        if (isDriverSupported(type)) {
            auto camera = createCamera(type, name);
            if (camera) {
                LOG_F(INFO, "Successfully created camera with {} driver", driverTypeToString(type));
                return camera;
            }
        }
    }

    LOG_F(ERROR, "Failed to create camera with any available driver: {}", name);
    return nullptr;
}

std::vector<CameraInfo> CameraFactory::scanForCameras() {
    auto now = std::chrono::steady_clock::now();
    
    // Return cached results if still valid
    if (!cached_cameras_.empty() && 
        (now - last_scan_time_) < CACHE_DURATION) {
        LOG_F(DEBUG, "Returning cached camera scan results");
        return cached_cameras_;
    }

    LOG_F(INFO, "Scanning for cameras across all drivers");
    
    std::vector<CameraInfo> allCameras;
    
    // Scan each supported driver type
    for (auto type : getSupportedDriverTypes()) {
        try {
            auto cameras = scanForCameras(type);
            allCameras.insert(allCameras.end(), cameras.begin(), cameras.end());
        } catch (const std::exception& e) {
            LOG_F(WARNING, "Error scanning {} cameras: {}", driverTypeToString(type), e.what());
        }
    }

    // Remove duplicates (same camera detected by multiple drivers)
    std::sort(allCameras.begin(), allCameras.end(), 
              [](const CameraInfo& a, const CameraInfo& b) {
                  return a.name < b.name;
              });
    
    auto it = std::unique(allCameras.begin(), allCameras.end(),
                         [](const CameraInfo& a, const CameraInfo& b) {
                             return a.name == b.name && a.manufacturer == b.manufacturer;
                         });
    allCameras.erase(it, allCameras.end());

    // Cache results
    cached_cameras_ = allCameras;
    last_scan_time_ = now;

    LOG_F(INFO, "Found {} unique cameras", allCameras.size());
    return allCameras;
}

std::vector<CameraInfo> CameraFactory::scanForCameras(CameraDriverType type) {
    LOG_F(DEBUG, "Scanning for {} cameras", driverTypeToString(type));
    
    switch (type) {
        case CameraDriverType::INDI:
            return scanINDICameras();
        case CameraDriverType::QHY:
            return scanQHYCameras();
        case CameraDriverType::ASI:
            return scanASICameras();
        case CameraDriverType::ATIK:
            return scanAtikCameras();
        case CameraDriverType::SBIG:
            return scanSBIGCameras();
        case CameraDriverType::FLI:
            return scanFLICameras();
        case CameraDriverType::PLAYERONE:
            return scanPlayerOneCameras();
        case CameraDriverType::ASCOM:
            return scanASCOMCameras();
        case CameraDriverType::SIMULATOR:
            return scanSimulatorCameras();
        default:
            LOG_F(WARNING, "Unknown camera driver type: {}", static_cast<int>(type));
            return {};
    }
}

std::vector<CameraDriverType> CameraFactory::getSupportedDriverTypes() const {
    std::vector<CameraDriverType> types;
    for (const auto& [type, _] : drivers_) {
        types.push_back(type);
    }
    return types;
}

bool CameraFactory::isDriverSupported(CameraDriverType type) const {
    return drivers_.find(type) != drivers_.end();
}

std::string CameraFactory::driverTypeToString(CameraDriverType type) {
    switch (type) {
        case CameraDriverType::INDI: return "INDI";
        case CameraDriverType::QHY: return "QHY";
        case CameraDriverType::ASI: return "ASI";
        case CameraDriverType::ATIK: return "Atik";
        case CameraDriverType::SBIG: return "SBIG";
        case CameraDriverType::FLI: return "FLI";
        case CameraDriverType::PLAYERONE: return "PlayerOne";
        case CameraDriverType::ASCOM: return "ASCOM";
        case CameraDriverType::SIMULATOR: return "Simulator";
        case CameraDriverType::AUTO_DETECT: return "Auto-Detect";
        default: return "Unknown";
    }
}

CameraDriverType CameraFactory::stringToDriverType(const std::string& typeStr) {
    std::string lower = typeStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "indi") return CameraDriverType::INDI;
    if (lower == "qhy") return CameraDriverType::QHY;
    if (lower == "asi" || lower == "zwo") return CameraDriverType::ASI;
    if (lower == "atik") return CameraDriverType::ATIK;
    if (lower == "sbig") return CameraDriverType::SBIG;
    if (lower == "fli") return CameraDriverType::FLI;
    if (lower == "playerone" || lower == "poa") return CameraDriverType::PLAYERONE;
    if (lower == "ascom") return CameraDriverType::ASCOM;
    if (lower == "simulator" || lower == "sim") return CameraDriverType::SIMULATOR;
    
    return CameraDriverType::AUTO_DETECT;
}

CameraInfo CameraFactory::getCameraInfo(const std::string& name, CameraDriverType type) {
    auto cameras = (type == CameraDriverType::AUTO_DETECT) ? 
                   scanForCameras() : scanForCameras(type);
    
    auto it = std::find_if(cameras.begin(), cameras.end(),
                          [&name](const CameraInfo& info) {
                              return info.name == name;
                          });
    
    return (it != cameras.end()) ? *it : CameraInfo{};
}

void CameraFactory::initializeDefaultDrivers() {
    LOG_F(INFO, "Initializing default camera drivers");

    // INDI Camera Driver (always available)
    registerCameraDriver(CameraDriverType::INDI, 
        [](const std::string& name) -> std::shared_ptr<AtomCamera> {
            return std::make_shared<lithium::device::indi::camera::INDICamera>(name);
        });

#ifdef LITHIUM_QHY_CAMERA_ENABLED
    // QHY Camera Driver
    registerCameraDriver(CameraDriverType::QHY,
        [](const std::string& name) -> std::shared_ptr<AtomCamera> {
            return std::make_shared<lithium::device::qhy::camera::QHYCamera>(name);
        });
    LOG_F(INFO, "QHY camera driver enabled");
#else
    LOG_F(INFO, "QHY camera driver disabled (SDK not found)");
#endif

#ifdef LITHIUM_ASI_CAMERA_ENABLED
    // ASI Camera Driver
    registerCameraDriver(CameraDriverType::ASI,
        [](const std::string& name) -> std::shared_ptr<AtomCamera> {
            return std::make_shared<lithium::device::asi::camera::ASICamera>(name);
        });
    LOG_F(INFO, "ASI camera driver enabled");
#else
    LOG_F(INFO, "ASI camera driver disabled (SDK not found)");
#endif

#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    // Atik Camera Driver
    registerCameraDriver(CameraDriverType::ATIK,
        [](const std::string& name) -> std::shared_ptr<AtomCamera> {
            return std::make_shared<lithium::device::atik::camera::AtikCamera>(name);
        });
    LOG_F(INFO, "Atik camera driver enabled");
#else
    LOG_F(INFO, "Atik camera driver disabled (SDK not found)");
#endif

#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    // SBIG Camera Driver
    registerCameraDriver(CameraDriverType::SBIG,
        [](const std::string& name) -> std::shared_ptr<AtomCamera> {
            return std::make_shared<lithium::device::sbig::camera::SBIGCamera>(name);
        });
    LOG_F(INFO, "SBIG camera driver enabled");
#else
    LOG_F(INFO, "SBIG camera driver disabled (SDK not found)");
#endif

#ifdef LITHIUM_FLI_CAMERA_ENABLED
    // FLI Camera Driver
    registerCameraDriver(CameraDriverType::FLI,
        [](const std::string& name) -> std::shared_ptr<AtomCamera> {
            return std::make_shared<lithium::device::fli::camera::FLICamera>(name);
        });
    LOG_F(INFO, "FLI camera driver enabled");
#else
    LOG_F(INFO, "FLI camera driver disabled (SDK not found)");
#endif

#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    // PlayerOne Camera Driver
    registerCameraDriver(CameraDriverType::PLAYERONE,
        [](const std::string& name) -> std::shared_ptr<AtomCamera> {
            return std::make_shared<lithium::device::playerone::camera::PlayerOneCamera>(name);
        });
    LOG_F(INFO, "PlayerOne camera driver enabled");
#else
    LOG_F(INFO, "PlayerOne camera driver disabled (SDK not found)");
#endif

    // Simulator Camera Driver (always available)
    registerCameraDriver(CameraDriverType::SIMULATOR,
        [](const std::string& name) -> std::shared_ptr<AtomCamera> {
            return std::make_shared<MockCamera>(name);
        });

    LOG_F(INFO, "Camera factory initialization complete");
}

// Scanner implementations
std::vector<CameraInfo> CameraFactory::scanINDICameras() {
    std::vector<CameraInfo> cameras;
    
    try {
        // Create temporary INDI camera instance to scan for devices
        auto indiCamera = std::make_shared<lithium::device::indi::camera::INDICamera>("temp");
        if (indiCamera->initialize()) {
            auto deviceNames = indiCamera->scan();
            
            for (const auto& deviceName : deviceNames) {
                CameraInfo info;
                info.name = deviceName;
                info.manufacturer = "INDI";
                info.model = deviceName;
                info.driver = "INDI";
                info.type = CameraDriverType::INDI;
                info.isAvailable = true;
                info.description = "INDI Camera Device: " + deviceName;
                cameras.push_back(info);
            }
            
            indiCamera->destroy();
        }
    } catch (const std::exception& e) {
        LOG_F(WARNING, "Error scanning INDI cameras: {}", e.what());
    }
    
    return cameras;
}

std::vector<CameraInfo> CameraFactory::scanQHYCameras() {
    std::vector<CameraInfo> cameras;
    
#ifdef LITHIUM_QHY_CAMERA_ENABLED
    try {
        // Create temporary QHY camera instance to scan for devices
        auto qhyCamera = std::make_shared<lithium::device::qhy::camera::QHYCamera>("temp");
        if (qhyCamera->initialize()) {
            auto deviceNames = qhyCamera->scan();
            
            for (const auto& deviceName : deviceNames) {
                CameraInfo info;
                info.name = deviceName;
                info.manufacturer = "QHY";
                info.model = deviceName;
                info.driver = "QHY SDK";
                info.type = CameraDriverType::QHY;
                info.isAvailable = true;
                info.description = "QHY Camera: " + deviceName;
                cameras.push_back(info);
            }
            
            qhyCamera->destroy();
        }
    } catch (const std::exception& e) {
        LOG_F(WARNING, "Error scanning QHY cameras: {}", e.what());
    }
#endif
    
    return cameras;
}

std::vector<CameraInfo> CameraFactory::scanASICameras() {
    std::vector<CameraInfo> cameras;
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    try {
        // Create temporary ASI camera instance to scan for devices
        auto asiCamera = std::make_shared<lithium::device::asi::camera::ASICamera>("temp");
        if (asiCamera->initialize()) {
            auto deviceNames = asiCamera->scan();
            
            for (const auto& deviceName : deviceNames) {
                CameraInfo info;
                info.name = deviceName;
                info.manufacturer = "ZWO";
                info.model = "ASI Camera";
                info.driver = "ASI SDK";
                info.type = CameraDriverType::ASI;
                info.isAvailable = true;
                info.description = "ZWO ASI Camera ID: " + deviceName;
                cameras.push_back(info);
            }
            
            asiCamera->destroy();
        }
    } catch (const std::exception& e) {
        LOG_F(WARNING, "Error scanning ASI cameras: {}", e.what());
    }
#endif
    
    return cameras;
}

std::vector<CameraInfo> CameraFactory::scanAtikCameras() {
    std::vector<CameraInfo> cameras;
    
#ifdef LITHIUM_ATIK_CAMERA_ENABLED
    try {
        // Create temporary Atik camera instance to scan for devices
        auto atikCamera = std::make_shared<lithium::device::atik::camera::AtikCamera>("temp");
        if (atikCamera->initialize()) {
            auto deviceNames = atikCamera->scan();
            
            for (const auto& deviceName : deviceNames) {
                CameraInfo info;
                info.name = deviceName;
                info.manufacturer = "Atik";
                info.model = deviceName;
                info.driver = "Atik SDK";
                info.type = CameraDriverType::ATIK;
                info.isAvailable = true;
                info.description = "Atik Camera: " + deviceName;
                cameras.push_back(info);
            }
            
            atikCamera->destroy();
        }
    } catch (const std::exception& e) {
        LOG_F(WARNING, "Error scanning Atik cameras: {}", e.what());
    }
#endif
    
    return cameras;
}

std::vector<CameraInfo> CameraFactory::scanSBIGCameras() {
    std::vector<CameraInfo> cameras;
    
#ifdef LITHIUM_SBIG_CAMERA_ENABLED
    try {
        // Create temporary SBIG camera instance to scan for devices
        auto sbigCamera = std::make_shared<lithium::device::sbig::camera::SBIGCamera>("temp");
        if (sbigCamera->initialize()) {
            auto deviceNames = sbigCamera->scan();
            
            for (const auto& deviceName : deviceNames) {
                CameraInfo info;
                info.name = deviceName;
                info.manufacturer = "SBIG";
                info.model = deviceName;
                info.driver = "SBIG Universal Driver";
                info.type = CameraDriverType::SBIG;
                info.isAvailable = true;
                info.description = "SBIG Camera: " + deviceName;
                cameras.push_back(info);
            }
            
            sbigCamera->destroy();
        }
    } catch (const std::exception& e) {
        LOG_F(WARNING, "Error scanning SBIG cameras: {}", e.what());
    }
#endif
    
    return cameras;
}

std::vector<CameraInfo> CameraFactory::scanFLICameras() {
    std::vector<CameraInfo> cameras;
    
#ifdef LITHIUM_FLI_CAMERA_ENABLED
    try {
        // Create temporary FLI camera instance to scan for devices
        auto fliCamera = std::make_shared<lithium::device::fli::camera::FLICamera>("temp");
        if (fliCamera->initialize()) {
            auto deviceNames = fliCamera->scan();
            
            for (const auto& deviceName : deviceNames) {
                CameraInfo info;
                info.name = deviceName;
                info.manufacturer = "FLI";
                info.model = deviceName;
                info.driver = "FLI SDK";
                info.type = CameraDriverType::FLI;
                info.isAvailable = true;
                info.description = "FLI Camera: " + deviceName;
                cameras.push_back(info);
            }
            
            fliCamera->destroy();
        }
    } catch (const std::exception& e) {
        LOG_F(WARNING, "Error scanning FLI cameras: {}", e.what());
    }
#endif
    
    return cameras;
}

std::vector<CameraInfo> CameraFactory::scanPlayerOneCameras() {
    std::vector<CameraInfo> cameras;
    
#ifdef LITHIUM_PLAYERONE_CAMERA_ENABLED
    try {
        // Create temporary PlayerOne camera instance to scan for devices
        auto poaCamera = std::make_shared<lithium::device::playerone::camera::PlayerOneCamera>("temp");
        if (poaCamera->initialize()) {
            auto deviceNames = poaCamera->scan();
            
            for (const auto& deviceName : deviceNames) {
                CameraInfo info;
                info.name = deviceName;
                info.manufacturer = "PlayerOne";
                info.model = deviceName;
                info.driver = "PlayerOne SDK";
                info.type = CameraDriverType::PLAYERONE;
                info.isAvailable = true;
                info.description = "PlayerOne Camera: " + deviceName;
                cameras.push_back(info);
            }
            
            poaCamera->destroy();
        }
    } catch (const std::exception& e) {
        LOG_F(WARNING, "Error scanning PlayerOne cameras: {}", e.what());
    }
#endif
    
    return cameras;
}

std::vector<CameraInfo> CameraFactory::scanSimulatorCameras() {
    std::vector<CameraInfo> cameras;
    
    // Always provide simulator cameras
    std::vector<std::string> simCameras = {
        "CCD Simulator",
        "Guide Camera Simulator", 
        "Planetary Camera Simulator"
    };
    
    for (const auto& simName : simCameras) {
        CameraInfo info;
        info.name = simName;
        info.manufacturer = "Lithium";
        info.model = "Mock Camera";
        info.driver = "Simulator";
        info.type = CameraDriverType::SIMULATOR;
        info.isAvailable = true;
        info.description = "Simulated camera for testing: " + simName;
        cameras.push_back(info);
    }
    
    return cameras;
}

} // namespace lithium::device
