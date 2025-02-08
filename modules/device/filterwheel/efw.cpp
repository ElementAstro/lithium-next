#include "efw.hpp"
#include <libasi/EFW_filter.h>
#include <chrono>
#include <loguru.hpp>
#include <string>
#include <thread>
#include <vector>

// 初始化静态成员
std::mutex EFWController::g_efwMutex;

// ==================== 错误处理模块 ====================
std::string errorCodeToString(EFW_ERROR_CODE code) {
    switch (code) {
        case EFW_SUCCESS:
            return "Success";
        case EFW_ERROR_INVALID_INDEX:
            return "Invalid index";
        case EFW_ERROR_INVALID_ID:
            return "Invalid ID";
        case EFW_ERROR_INVALID_VALUE:
            return "Invalid value";
        case EFW_ERROR_CLOSED:
            return "Device closed";
        case EFW_ERROR_REMOVED:
            return "Device removed";
        case EFW_ERROR_MOVING:
            return "Device moving";
        case EFW_ERROR_GENERAL_ERROR:
            return "General error";
        case EFW_ERROR_NOT_SUPPORTED:
            return "Operation not supported";
        case EFW_ERROR_ERROR_STATE:
            return "Device in error state";
        default:
            return "Unknown error";
    }
}

// ==================== 设备管理核心类 实现 ====================
EFWController::EFWController()
    : AtomFilterWheel("EFWController"), deviceID(-1), isOpen(false) {}

EFWController::EFWController(int id)
    : AtomFilterWheel("EFWController"), deviceID(id), isOpen(false) {}

EFWController::~EFWController() {
    if (isOpen)
        close();
}

EFWController::EFWController(EFWController&& other) noexcept
    : AtomFilterWheel(std::move(other)),
      deviceID(other.deviceID),
      isOpen(other.isOpen),
      properties(other.properties),
      slotNames(std::move(other.slotNames)) {
    other.deviceID = -1;
    other.isOpen = false;
}

EFWController& EFWController::operator=(EFWController&& other) noexcept {
    if (this != &other) {
        AtomFilterWheel::operator=(std::move(other));
        if (isOpen)
            close();
        deviceID = other.deviceID;
        isOpen = other.isOpen;
        properties = other.properties;
        slotNames = std::move(other.slotNames);
        other.deviceID = -1;
        other.isOpen = false;
    }
    return *this;
}

bool EFWController::open() {
    std::lock_guard<std::mutex> lock(g_efwMutex);
    EFW_ERROR_CODE err = EFWOpen(deviceID);
    LOG_F(INFO, "Operation: open | Device {} | Status: {}", deviceID,
          errorCodeToString(err));
    if (err == EFW_SUCCESS) {
        isOpen = true;
        LOG_F(INFO, "Device {} opened successfully.", deviceID);
        return true;
    }
    LOG_F(ERROR, "Failed to open device {}.", deviceID);
    return false;
}

bool EFWController::setPosition(int slot, int maxRetries) {
    if (!isOpen) {
        LOG_F(WARNING, "Attempted to set position on closed device {}.",
              deviceID);
        return false;
    }

    LOG_F(INFO, "Setting position to slot {} on device {}.", slot, deviceID);

    for (int i = 0; i < maxRetries; ++i) {
        std::lock_guard<std::mutex> lock(g_efwMutex);
        EFW_ERROR_CODE err = EFWSetPosition(deviceID, slot);
        LOG_F(INFO,
              "Operation: setPosition | Device {} | Attempt {} | Status: {}",
              deviceID, i + 1, errorCodeToString(err));

        if (err == EFW_SUCCESS) {
            LOG_F(INFO, "Successfully set position to slot {} on device {}.",
                  slot, deviceID);
            return true;
        }
        if (err == EFW_ERROR_REMOVED) {
            LOG_F(WARNING,
                  "Device {} removed during setPosition. Attempting to reopen.",
                  deviceID);
            close();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (open()) {
                LOG_F(INFO, "Reopened device {} successfully.", deviceID);
                continue;
            }
        }
        handleError(err);
    }
    LOG_F(ERROR,
          "Failed to set position to slot {} on device {} after {} attempts.",
          slot, deviceID, maxRetries);
    return false;
}

void EFWController::close() {
    std::lock_guard<std::mutex> lock(g_efwMutex);
    EFW_ERROR_CODE err = EFWClose(deviceID);
    LOG_F(INFO, "Operation: close | Device {} | Status: {}", deviceID,
          errorCodeToString(err));
    if (err == EFW_SUCCESS) {
        isOpen = false;
        LOG_F(INFO, "Device {} closed successfully.", deviceID);
    } else {
        LOG_F(ERROR, "Failed to close device {}.", deviceID);
    }
}

int EFWController::getPositionValue() {
    std::lock_guard<std::mutex> lock(g_efwMutex);
    int pos;
    EFW_ERROR_CODE err = EFWGetPosition(deviceID, &pos);
    LOG_F(INFO, "Operation: getPosition | Device {} | Status: {}", deviceID,
          errorCodeToString(err));
    if (err == EFW_SUCCESS) {
        LOG_F(INFO, "Current position of device {} is slot {}.", deviceID, pos);
        return pos;
    } else {
        LOG_F(ERROR, "Failed to get position of device {}.", deviceID);
        return -1;
    }
}

bool EFWController::getProperties() const {
    std::lock_guard<std::mutex> lock(propMutex);
    EFW_INFO info;
    EFW_ERROR_CODE err = EFWGetProperty(deviceID, &info);
    if (err != EFW_SUCCESS) {
        LOG_F(ERROR, "Operation: getProperties | Device {} | Status: {}",
              deviceID, errorCodeToString(err));
        return false;
    }

    properties.info = info;
    bool unidirectional;
    EFWGetDirection(deviceID, &unidirectional);
    properties.isUnidirectional = unidirectional;

    unsigned char major, minor, build;
    EFWGetFirmwareVersion(deviceID, &major, &minor, &build);
    properties.firmwareMajor = major;
    properties.firmwareMinor = minor;
    properties.firmwareBuild = build;

    EFW_SN sn;
    EFWGetSerialNumber(deviceID, &sn);
    properties.serialNumber = sn;

    int errorCode;
    EFWGetHWErrorCode(deviceID, &errorCode);
    properties.lastErrorCode = errorCode;

    LOG_F(INFO, "Operation: getProperties | Device {} | Status: Success",
          deviceID);
    LOG_F(INFO,
          "Device {} Properties: Firmware {}.{}.{}, Serial Number {}, "
          "Unidirectional: {}",
          deviceID, properties.firmwareMajor, properties.firmwareMinor,
          properties.firmwareBuild, properties.serialNumber,
          properties.isUnidirectional ? "true" : "false");
    return true;
}

bool EFWController::calibrate() {
    std::lock_guard<std::mutex> lock(g_efwMutex);
    EFW_ERROR_CODE err = EFWCalibrate(deviceID);
    LOG_F(INFO, "Operation: calibrate | Device {} | Status: {}", deviceID,
          errorCodeToString(err));
    if (err == EFW_SUCCESS) {
        LOG_F(INFO, "Device {} calibrated successfully.", deviceID);
        return true;
    }
    LOG_F(ERROR, "Failed to calibrate device {}.", deviceID);
    return false;
}

bool EFWController::setUnidirectional(bool enabled) {
    std::lock_guard<std::mutex> lock(g_efwMutex);
    EFW_ERROR_CODE err = EFWSetDirection(deviceID, enabled);
    LOG_F(INFO,
          "Operation: setUnidirectional | Device {} | Enabled: {} | Status: {}",
          deviceID, enabled ? "true" : "false", errorCodeToString(err));
    if (err == EFW_SUCCESS) {
        properties.isUnidirectional = enabled;
        LOG_F(INFO, "Device {} unidirectional set to {}.", deviceID,
              enabled ? "true" : "false");
        return true;
    }
    LOG_F(ERROR, "Failed to set unidirectional on device {}.", deviceID);
    return false;
}

bool EFWController::setAlias(const EFW_ID& alias) {
    std::lock_guard<std::mutex> lock(g_efwMutex);
    EFW_ERROR_CODE err = EFWSetID(deviceID, alias);
    LOG_F(INFO, "Operation: setAlias | Device {} | Alias: {} | Status: {}",
          deviceID, alias, errorCodeToString(err));
    if (err == EFW_SUCCESS) {
        LOG_F(INFO, "Alias for device {} set to {} successfully.", deviceID,
              alias);
        return true;
    }
    LOG_F(ERROR, "Failed to set alias for device {}.", deviceID);
    return false;
}

const DeviceProperties& EFWController::getDeviceProperties() const {
    return properties;
}

std::optional<std::tuple<double, double, double>> EFWController::getPosition() {
    int pos = getPositionValue();
    if (pos != -1) {
        return std::make_tuple(static_cast<double>(pos), 0.0, 0.0);
    }
    return std::nullopt;
}

bool EFWController::setPosition(int position) {
    return setPosition(position, 3);
}

std::optional<std::string> EFWController::getSlotName() {
    if (slotNames.empty()) {
        LOG_F(WARNING, "No slot names available for device {}.", deviceID);
        return std::nullopt;
    }
    LOG_F(INFO, "Operation: getSlotName | Device {} | Current Slot Name: {}",
          deviceID, slotNames.empty() ? "None" : slotNames[0]);
    return slotNames.empty() ? std::nullopt
                             : std::optional<std::string>(slotNames[0]);
}

bool EFWController::setSlotName(std::string_view name) {
    if (slotNames.empty()) {
        slotNames.emplace_back(name);
    } else {
        slotNames[0] = std::string(name);
    }
    LOG_F(INFO, "Operation: setSlotName | Device {} | New Slot Name: {}",
          deviceID, std::string(name));
    return true;
}

void EFWController::handleError(EFW_ERROR_CODE err) {
    LOG_F(ERROR, "Device {} error: {}", deviceID, errorCodeToString(err));
}

bool EFWController::initialize() {
    LOG_F(INFO, "Initializing EFW device");
    // EFW设备不需要特殊的初始化
    return true;
}

bool EFWController::destroy() {
    LOG_F(INFO, "Destroying EFW device {}", deviceID);
    if (isOpen) {
        return disconnect();
    }
    return true;
}

bool EFWController::connect(const std::string& port, int timeout,
                            int maxRetry) {
    LOG_F(INFO, "Connecting to EFW device {}", deviceID);
    for (int attempt = 0; attempt < maxRetry; ++attempt) {
        if (open()) {
            return true;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(timeout / maxRetry));
    }
    return false;
}

bool EFWController::disconnect() {
    LOG_F(INFO, "Disconnecting EFW device {}", deviceID);
    if (isOpen) {
        close();
    }
    return true;
}

bool EFWController::isConnected() const { return isOpen; }

std::vector<std::string> EFWController::scan() {
    std::vector<std::string> devices;
    int count = EFWGetNum();
    LOG_F(INFO, "Found {} EFW devices", count);

    for (int i = 0; i < count; i++) {
        EFW_INFO info;
        if (EFWGetProperty(i, &info) == EFW_SUCCESS) {
            devices.push_back("EFW-" + std::to_string(info.ID));
        }
    }

    return devices;
}
