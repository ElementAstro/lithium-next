/*
 * ascom_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: ASCOM type definitions and constants

*************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_TYPES_HPP
#define LITHIUM_CLIENT_ASCOM_TYPES_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::client::ascom {

using json = nlohmann::json;

/**
 * @brief ASCOM device types
 */
enum class ASCOMDeviceType {
    Unknown = 0,
    Camera,
    CoverCalibrator,
    Dome,
    FilterWheel,
    Focuser,
    ObservingConditions,
    Rotator,
    SafetyMonitor,
    Switch,
    Telescope,
    Video
};

/**
 * @brief Convert ASCOM device type to string
 */
inline auto deviceTypeToString(ASCOMDeviceType type) -> std::string {
    switch (type) {
        case ASCOMDeviceType::Camera: return "camera";
        case ASCOMDeviceType::CoverCalibrator: return "covercalibrator";
        case ASCOMDeviceType::Dome: return "dome";
        case ASCOMDeviceType::FilterWheel: return "filterwheel";
        case ASCOMDeviceType::Focuser: return "focuser";
        case ASCOMDeviceType::ObservingConditions: return "observingconditions";
        case ASCOMDeviceType::Rotator: return "rotator";
        case ASCOMDeviceType::SafetyMonitor: return "safetymonitor";
        case ASCOMDeviceType::Switch: return "switch";
        case ASCOMDeviceType::Telescope: return "telescope";
        case ASCOMDeviceType::Video: return "video";
        default: return "unknown";
    }
}

/**
 * @brief Convert string to ASCOM device type
 */
inline auto stringToDeviceType(const std::string& str) -> ASCOMDeviceType {
    static const std::unordered_map<std::string, ASCOMDeviceType> typeMap = {
        {"camera", ASCOMDeviceType::Camera},
        {"covercalibrator", ASCOMDeviceType::CoverCalibrator},
        {"dome", ASCOMDeviceType::Dome},
        {"filterwheel", ASCOMDeviceType::FilterWheel},
        {"focuser", ASCOMDeviceType::Focuser},
        {"observingconditions", ASCOMDeviceType::ObservingConditions},
        {"rotator", ASCOMDeviceType::Rotator},
        {"safetymonitor", ASCOMDeviceType::SafetyMonitor},
        {"switch", ASCOMDeviceType::Switch},
        {"telescope", ASCOMDeviceType::Telescope},
        {"video", ASCOMDeviceType::Video}
    };
    
    auto it = typeMap.find(str);
    return it != typeMap.end() ? it->second : ASCOMDeviceType::Unknown;
}

/**
 * @brief ASCOM error codes
 */
struct ASCOMErrorCode {
    static constexpr int OK = 0;
    static constexpr int ActionNotImplemented = 0x40C;
    static constexpr int InvalidValue = 0x401;
    static constexpr int ValueNotSet = 0x402;
    static constexpr int NotConnected = 0x407;
    static constexpr int InvalidWhileParked = 0x408;
    static constexpr int InvalidWhileSlaved = 0x409;
    static constexpr int InvalidOperation = 0x40B;
    static constexpr int UnspecifiedError = 0x4FF;
};

/**
 * @brief ASCOM Alpaca API response
 */
struct AlpacaResponse {
    int clientTransactionId{0};
    int serverTransactionId{0};
    int errorNumber{0};
    std::string errorMessage;
    json value;
    
    [[nodiscard]] bool isSuccess() const { return errorNumber == 0; }
    
    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["ClientTransactionID"] = clientTransactionId;
        j["ServerTransactionID"] = serverTransactionId;
        j["ErrorNumber"] = errorNumber;
        j["ErrorMessage"] = errorMessage;
        if (!value.is_null()) {
            j["Value"] = value;
        }
        return j;
    }
    
    static auto fromJson(const json& j) -> AlpacaResponse {
        AlpacaResponse resp;
        resp.clientTransactionId = j.value("ClientTransactionID", 0);
        resp.serverTransactionId = j.value("ServerTransactionID", 0);
        resp.errorNumber = j.value("ErrorNumber", 0);
        resp.errorMessage = j.value("ErrorMessage", "");
        if (j.contains("Value")) {
            resp.value = j["Value"];
        }
        return resp;
    }
};

/**
 * @brief ASCOM device description from discovery
 */
struct ASCOMDeviceDescription {
    std::string deviceName;
    ASCOMDeviceType deviceType;
    int deviceNumber{0};
    std::string uniqueId;
    
    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["DeviceName"] = deviceName;
        j["DeviceType"] = deviceTypeToString(deviceType);
        j["DeviceNumber"] = deviceNumber;
        j["UniqueID"] = uniqueId;
        return j;
    }
    
    static auto fromJson(const json& j) -> ASCOMDeviceDescription {
        ASCOMDeviceDescription desc;
        desc.deviceName = j.value("DeviceName", "");
        desc.deviceType = stringToDeviceType(j.value("DeviceType", ""));
        desc.deviceNumber = j.value("DeviceNumber", 0);
        desc.uniqueId = j.value("UniqueID", "");
        return desc;
    }
};

/**
 * @brief ASCOM Alpaca server info
 */
struct AlpacaServerInfo {
    std::string serverName;
    std::string manufacturer;
    std::string manufacturerVersion;
    std::string location;
    std::vector<ASCOMDeviceDescription> devices;
    
    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["ServerName"] = serverName;
        j["Manufacturer"] = manufacturer;
        j["ManufacturerVersion"] = manufacturerVersion;
        j["Location"] = location;
        
        json devArray = json::array();
        for (const auto& dev : devices) {
            devArray.push_back(dev.toJson());
        }
        j["Devices"] = devArray;
        return j;
    }
    
    static auto fromJson(const json& j) -> AlpacaServerInfo {
        AlpacaServerInfo info;
        info.serverName = j.value("ServerName", "");
        info.manufacturer = j.value("Manufacturer", "");
        info.manufacturerVersion = j.value("ManufacturerVersion", "");
        info.location = j.value("Location", "");
        
        if (j.contains("Devices") && j["Devices"].is_array()) {
            for (const auto& dev : j["Devices"]) {
                info.devices.push_back(ASCOMDeviceDescription::fromJson(dev));
            }
        }
        return info;
    }
};

/**
 * @brief ASCOM image array types
 */
enum class ImageArrayType {
    Unknown = 0,
    Int16 = 2,
    Int32 = 3,
    Double = 5
};

/**
 * @brief ASCOM sensor types
 */
enum class SensorType {
    Monochrome = 0,
    Color = 1,
    RGGB = 2,
    CMYG = 3,
    CMYG2 = 4,
    LRGB = 5
};

/**
 * @brief ASCOM camera states
 */
enum class CameraState {
    Idle = 0,
    Waiting = 1,
    Exposing = 2,
    Reading = 3,
    Download = 4,
    Error = 5
};

/**
 * @brief ASCOM guide directions
 */
enum class GuideDirection {
    North = 0,
    South = 1,
    East = 2,
    West = 3
};

/**
 * @brief ASCOM telescope tracking rates
 */
enum class DriveRate {
    Sidereal = 0,
    Lunar = 1,
    Solar = 2,
    King = 3
};

/**
 * @brief ASCOM telescope alignment modes
 */
enum class AlignmentMode {
    AltAz = 0,
    Polar = 1,
    GermanPolar = 2
};

/**
 * @brief ASCOM pier side
 */
enum class PierSide {
    Unknown = -1,
    East = 0,
    West = 1
};

/**
 * @brief ASCOM shutter states
 */
enum class ShutterState {
    Open = 0,
    Closed = 1,
    Opening = 2,
    Closing = 3,
    Error = 4
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_TYPES_HPP
