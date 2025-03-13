#include "efw.hpp"
#include <libasi/EFW_filter.h>
#include <chrono>
#include <concepts>
#include <future>
#include <loguru.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Initialize static members
std::mutex EFWController::g_efwMutex;

// ==================== Error Handling Module ====================
[[nodiscard]] constexpr std::string_view errorCodeToString(
    EFW_ERROR_CODE code) noexcept {
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

// Concept for valid slot positions
template <typename T>
concept SlotPosition = std::integral<T> && requires(T t) {
    { t >= 0 } -> std::convertible_to<bool>;
};

// ==================== Device Management Core Class Implementation
// ====================
EFWController::EFWController()
    : AtomFilterWheel("EFWController"), deviceID(-1), isOpen(false) {}

EFWController::EFWController(int id)
    : AtomFilterWheel("EFWController"), deviceID(id), isOpen(false) {
    if (id < 0) {
        throw std::invalid_argument("Device ID must be non-negative");
    }
}

EFWController::~EFWController() {
    try {
        if (isOpen)
            close();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during destructor: %s", e.what());
    }
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
    if (deviceID < 0) {
        LOG_F(ERROR, "Invalid device ID: %d", deviceID);
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(g_efwMutex);
        EFW_ERROR_CODE err = EFWOpen(deviceID);
        LOG_F(INFO, "Operation: open | Device %d | Status: %s", deviceID,
              errorCodeToString(err).data());

        if (err == EFW_SUCCESS) {
            isOpen = true;
            LOG_F(INFO, "Device %d opened successfully.", deviceID);
            return true;
        }
        LOG_F(ERROR, "Failed to open device %d.", deviceID);
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during open: %s", e.what());
        return false;
    }
}

bool EFWController::setPosition(int slot, int maxRetries) {
    if (!isOpen) {
        LOG_F(WARNING, "Attempted to set position on closed device %d.",
              deviceID);
        return false;
    }

    if (maxRetries <= 0) {
        LOG_F(ERROR, "Invalid maxRetries value: %d", maxRetries);
        return false;
    }

    // Get properties to check slot bounds
    EFW_INFO info;
    {
        std::lock_guard<std::mutex> lock(g_efwMutex);
        if (EFWGetProperty(deviceID, &info) != EFW_SUCCESS) {
            LOG_F(ERROR, "Failed to get properties for bounds checking");
            return false;
        }
    }

    // Verify position is within range
    if (slot < 0 || slot >= info.slotNum) {
        LOG_F(ERROR, "Invalid slot position %d. Valid range: 0-%d",
              static_cast<int>(slot), info.slotNum - 1);
        return false;
    }

    LOG_F(INFO, "Setting position to slot %d on device %d.",
          static_cast<int>(slot), deviceID);

    for (int i = 0; i < maxRetries; ++i) {
        try {
            std::lock_guard<std::mutex> lock(g_efwMutex);
            EFW_ERROR_CODE err =
                EFWSetPosition(deviceID, static_cast<int>(slot));
            LOG_F(
                INFO,
                "Operation: setPosition | Device %d | Attempt %d | Status: %s",
                deviceID, i + 1, errorCodeToString(err).data());

            if (err == EFW_SUCCESS) {
                LOG_F(INFO,
                      "Successfully set position to slot %d on device %d.",
                      static_cast<int>(slot), deviceID);
                return true;
            }

            if (err == EFW_ERROR_REMOVED) {
                LOG_F(WARNING,
                      "Device %d removed during setPosition. Attempting to "
                      "reopen.",
                      deviceID);
                close();
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (open()) {
                    LOG_F(INFO, "Reopened device %d successfully.", deviceID);
                    continue;
                }
            }
            handleError(err);

            // Exponential backoff between attempts
            if (i < maxRetries - 1) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100 * (1 << i)));
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception during setPosition attempt %d: %s", i + 1,
                  e.what());
        }
    }

    LOG_F(ERROR,
          "Failed to set position to slot %d on device %d after %d attempts.",
          static_cast<int>(slot), deviceID, maxRetries);
    return false;
}

void EFWController::close() {
    try {
        std::lock_guard<std::mutex> lock(g_efwMutex);
        if (deviceID >= 0) {
            EFW_ERROR_CODE err = EFWClose(deviceID);
            LOG_F(INFO, "Operation: close | Device %d | Status: %s", deviceID,
                  errorCodeToString(err).data());
            if (err == EFW_SUCCESS) {
                isOpen = false;
                LOG_F(INFO, "Device %d closed successfully.", deviceID);
            } else {
                LOG_F(ERROR, "Failed to close device %d.", deviceID);
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during close: %s", e.what());
    }
}

int EFWController::getPositionValue() {
    if (!isOpen) {
        LOG_F(WARNING, "Attempted to get position on closed device %d.",
              deviceID);
        return -1;
    }

    try {
        std::lock_guard<std::mutex> lock(g_efwMutex);
        int pos = -1;
        EFW_ERROR_CODE err = EFWGetPosition(deviceID, &pos);
        LOG_F(INFO, "Operation: getPosition | Device %d | Status: %s", deviceID,
              errorCodeToString(err).data());
        if (err == EFW_SUCCESS) {
            LOG_F(INFO, "Current position of device %d is slot %d.", deviceID,
                  pos);
            return pos;
        } else {
            LOG_F(ERROR, "Failed to get position of device %d.", deviceID);
            return -1;
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during getPositionValue: %s", e.what());
        return -1;
    }
}

bool EFWController::getProperties() const {
    if (deviceID < 0) {
        LOG_F(ERROR, "Invalid device ID: %d", deviceID);
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(propMutex);
        EFW_INFO info;
        EFW_ERROR_CODE err = EFWGetProperty(deviceID, &info);
        if (err != EFW_SUCCESS) {
            LOG_F(ERROR, "Operation: getProperties | Device %d | Status: %s",
                  deviceID, errorCodeToString(err).data());
            return false;
        }

        properties.info = info;

        // Use structured bindings for cleaner code
        bool unidirectional = false;
        EFWGetDirection(deviceID, &unidirectional);
        properties.isUnidirectional = unidirectional;

        // Get firmware version
        unsigned char major = 0, minor = 0, build = 0;
        EFWGetFirmwareVersion(deviceID, &major, &minor, &build);
        properties.firmwareMajor = major;
        properties.firmwareMinor = minor;
        properties.firmwareBuild = build;

        // Get serial number
        EFW_SN sn;
        EFWGetSerialNumber(deviceID, &sn);
        properties.serialNumber = sn;

        // Get hardware error code
        int errorCode = 0;
        EFWGetHWErrorCode(deviceID, &errorCode);
        properties.lastErrorCode = errorCode;

        LOG_F(INFO, "Operation: getProperties | Device %d | Status: Success",
              deviceID);
        LOG_F(INFO,
              "Device %d Properties: Firmware %d.%d.%d, Serial Number %d, "
              "Unidirectional: %s",
              deviceID, properties.firmwareMajor, properties.firmwareMinor,
              properties.firmwareBuild, properties.serialNumber,
              properties.isUnidirectional ? "true" : "false");
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during getProperties: %s", e.what());
        return false;
    }
}

bool EFWController::calibrate() {
    if (!isOpen) {
        LOG_F(WARNING, "Attempted to calibrate closed device %d.", deviceID);
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(g_efwMutex);
        EFW_ERROR_CODE err = EFWCalibrate(deviceID);
        LOG_F(INFO, "Operation: calibrate | Device %d | Status: %s", deviceID,
              errorCodeToString(err).data());
        if (err == EFW_SUCCESS) {
            LOG_F(INFO, "Device %d calibrated successfully.", deviceID);
            return true;
        }
        LOG_F(ERROR, "Failed to calibrate device %d.", deviceID);
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during calibrate: %s", e.what());
        return false;
    }
}

bool EFWController::setUnidirectional(bool enabled) {
    if (!isOpen) {
        LOG_F(WARNING, "Attempted to configure closed device %d.", deviceID);
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(g_efwMutex);
        EFW_ERROR_CODE err = EFWSetDirection(deviceID, enabled);
        LOG_F(INFO,
              "Operation: setUnidirectional | Device %d | Enabled: %s | "
              "Status: %s",
              deviceID, enabled ? "true" : "false",
              errorCodeToString(err).data());
        if (err == EFW_SUCCESS) {
            properties.isUnidirectional = enabled;
            LOG_F(INFO, "Device %d unidirectional set to %s.", deviceID,
                  enabled ? "true" : "false");
            return true;
        }
        LOG_F(ERROR, "Failed to set unidirectional on device %d.", deviceID);
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during setUnidirectional: %s", e.what());
        return false;
    }
}

bool EFWController::setAlias(const EFW_ID& alias) {
    if (!isOpen) {
        LOG_F(WARNING, "Attempted to set alias on closed device %d.", deviceID);
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(g_efwMutex);
        EFW_ERROR_CODE err = EFWSetID(deviceID, alias);
        LOG_F(INFO, "Operation: setAlias | Device %d | Alias: %d | Status: %s",
              deviceID, alias, errorCodeToString(err).data());
        if (err == EFW_SUCCESS) {
            LOG_F(INFO, "Alias for device %d set to %d successfully.", deviceID,
                  alias);
            return true;
        }
        LOG_F(ERROR, "Failed to set alias for device %d.", deviceID);
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during setAlias: %s", e.what());
        return false;
    }
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
    std::lock_guard<std::mutex> lock(propMutex);
    if (slotNames.empty()) {
        LOG_F(WARNING, "No slot names available for device %d.", deviceID);
        return std::nullopt;
    }
    LOG_F(INFO, "Operation: getSlotName | Device %d | Current Slot Name: %s",
          deviceID, slotNames.empty() ? "None" : slotNames[0].c_str());
    return slotNames.empty() ? std::nullopt
                             : std::optional<std::string>(slotNames[0]);
}

bool EFWController::setSlotName(std::string_view name) {
    if (name.empty()) {
        LOG_F(WARNING, "Attempted to set empty slot name for device %d",
              deviceID);
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(propMutex);
        if (slotNames.empty()) {
            slotNames.emplace_back(name);
        } else {
            slotNames[0] = std::string(name);
        }
        LOG_F(INFO, "Operation: setSlotName | Device %d | New Slot Name: %s",
              deviceID, std::string(name).c_str());
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during setSlotName: %s", e.what());
        return false;
    }
}

void EFWController::handleError(EFW_ERROR_CODE err) {
    LOG_F(ERROR, "Device %d error: %s", deviceID,
          errorCodeToString(err).data());
}

bool EFWController::initialize() {
    LOG_F(INFO, "Initializing EFW device");
    // No special initialization needed for EFW devices
    return true;
}

bool EFWController::destroy() {
    LOG_F(INFO, "Destroying EFW device %d", deviceID);
    try {
        if (isOpen) {
            return disconnect();
        }
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during destroy: %s", e.what());
        return false;
    }
}

bool EFWController::connect(const std::string& port, int timeout,
                            int maxRetry) {
    if (maxRetry <= 0) {
        LOG_F(ERROR, "Invalid maxRetry value: %d", maxRetry);
        return false;
    }

    if (timeout <= 0) {
        LOG_F(ERROR, "Invalid timeout value: %d", timeout);
        return false;
    }

    LOG_F(INFO, "Connecting to EFW device %d", deviceID);

    // Use std::async for non-blocking connection attempts with timeout
    for (int attempt = 0; attempt < maxRetry; ++attempt) {
        auto futureResult =
            std::async(std::launch::async, [this]() -> bool { return open(); });

        // Wait for operation to complete or timeout
        auto status = futureResult.wait_for(
            std::chrono::milliseconds(timeout / maxRetry));

        if (status == std::future_status::ready && futureResult.get()) {
            return true;
        }

        // Exponential backoff between attempts
        if (attempt < maxRetry - 1) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(50 * (1 << attempt)));
        }
    }

    LOG_F(ERROR, "Failed to connect to device %d after %d attempts", deviceID,
          maxRetry);
    return false;
}

bool EFWController::disconnect() {
    LOG_F(INFO, "Disconnecting EFW device %d", deviceID);
    try {
        if (isOpen) {
            close();
        }
        return !isOpen;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during disconnect: %s", e.what());
        return false;
    }
}

bool EFWController::isConnected() const { return isOpen; }

std::vector<std::string> EFWController::scan() {
    std::vector<std::string> devices;

    try {
        int count = EFWGetNum();
        LOG_F(INFO, "Found %d EFW devices", count);

        if (count <= 0) {
            return devices;
        }

        devices.reserve(count);

        for (int i = 0; i < count; i++) {
            EFW_INFO info;
            if (EFWGetProperty(i, &info) == EFW_SUCCESS) {
                devices.push_back("EFW-" + std::to_string(info.ID));
                LOG_F(INFO, "Detected device: EFW-%d", info.ID);
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during scan: %s", e.what());
    }

    return devices;
}
