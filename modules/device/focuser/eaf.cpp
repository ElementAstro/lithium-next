#include "eaf.hpp"
#include <libasi/EAF_focuser.h>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "atom/async/timer.hpp"
#include "atom/log/spdlog_logger.hpp"

// 全局互斥锁保证线程安全
std::mutex g_eafMutex;

// ==================== 错误处理模块 ====================
std::string ErrorCodeToString(EAF_ERROR_CODE code) {
    switch (code) {
        case EAF_SUCCESS:
            return "Success";
        case EAF_ERROR_INVALID_INDEX:
            return "Invalid index";
        case EAF_ERROR_INVALID_ID:
            return "Invalid ID";
        case EAF_ERROR_INVALID_VALUE:
            return "Invalid value";
        case EAF_ERROR_REMOVED:
            return "Device removed";
        case EAF_ERROR_MOVING:
            return "Device moving";
        case EAF_ERROR_ERROR_STATE:
            return "Device in error state";
        case EAF_ERROR_GENERAL_ERROR:
            return "General error";
        case EAF_ERROR_NOT_SUPPORTED:
            return "Operation not supported";
        case EAF_ERROR_CLOSED:
            return "Device closed";
        default:
            return "Unknown error";
    }
}

// EAFController 实现
EAFController::EAFController(const std::string& name)
    : AtomFocuser(name),
      m_deviceID(-1),
      m_isOpen(false),
      m_isConnected(false),  // 添加这一行
      m_speed(1.0),
      m_maxLimit(0) {
    LOG_F(INFO, "EAFController created with name: {}", name.c_str());
}

EAFController::~EAFController() {
    if (m_isOpen)
        Close();
    LOG_F(INFO, "EAFController destroyed");
}

EAFController::EAFController(EAFController&& other) noexcept
    : AtomFocuser(std::move(other)),
      m_deviceID(other.m_deviceID),
      m_isOpen(other.m_isOpen),
      m_speed(other.m_speed),
      m_maxLimit(other.m_maxLimit),
      m_properties(std::move(other.m_properties)) {
    other.m_deviceID = -1;
    other.m_isOpen = false;
    LOG_F(INFO, "EAFController moved");
}

EAFController& EAFController::operator=(EAFController&& other) noexcept {
    if (this != &other) {
        if (m_isOpen)
            Close();
        AtomFocuser::operator=(std::move(other));
        m_deviceID = other.m_deviceID;
        m_isOpen = other.m_isOpen;
        m_speed = other.m_speed;
        m_maxLimit = other.m_maxLimit;
        m_properties = std::move(other.m_properties);
        other.m_deviceID = -1;
        other.m_isOpen = false;
        LOG_F(INFO, "EAFController move-assigned");
    }
    return *this;
}

bool EAFController::Open() {
    std::lock_guard<std::mutex> lock(g_eafMutex);
    EAF_ERROR_CODE err = EAFOpen(m_deviceID);
    if (err == EAF_SUCCESS) {
        m_isOpen = true;
        UpdateProperties();
        LOG_F(INFO, "EAFController opened with device ID: {}", m_deviceID);
        return true;
    }
    LOG_F(ERROR, "Failed to open EAFController with device ID: {}, error: {}",
          m_deviceID, ErrorCodeToString(err).c_str());
    return false;
}

void EAFController::Close() {
    std::lock_guard<std::mutex> lock(g_eafMutex);
    if (m_isOpen) {
        EAFClose(m_deviceID);
        m_isOpen = false;
        LOG_F(INFO, "EAFController closed with device ID: {}", m_deviceID);
    }
}

bool EAFController::Move(int position, bool waitComplete) {
    if (!m_isOpen) {
        LOG_F(ERROR, "Attempted to move EAFController while it is not open");
        return false;
    }

    std::lock_guard<std::mutex> lock(g_eafMutex);
    EAF_ERROR_CODE err = EAFMove(m_deviceID, position);
    if (err != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to move EAFController to position {}, error: {}",
              position, ErrorCodeToString(err).c_str());
        return false;
    }

    if (waitComplete) {
        bool moving = true;
        bool handControl;
        while (moving) {
            err = EAFIsMoving(m_deviceID, &moving, &handControl);
            if (err != EAF_SUCCESS) {
                LOG_F(ERROR,
                      "Error while checking if EAFController is moving, error: "
                      "{}",
                      ErrorCodeToString(err).c_str());
                return false;
            }
            if (handControl) {
                LOG_F(ERROR, "EAFController is in hand control mode");
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    LOG_F(INFO, "EAFController moved to position {}", position);
    return true;
}

bool EAFController::Stop() {
    std::lock_guard<std::mutex> lock(g_eafMutex);
    bool result = EAFStop(m_deviceID) == EAF_SUCCESS;
    LOG_F(INFO, "EAFController stop command issued, result: {}",
          result ? "success" : "failure");
    return result;
}

std::optional<int> EAFController::getPosition() {
    std::lock_guard<std::mutex> lock(g_eafMutex);
    int pos;
    if (EAFGetPosition(m_deviceID, &pos) == EAF_SUCCESS) {
        LOG_F(INFO, "EAFController current position: {}", pos);
        return pos;
    }
    LOG_F(ERROR, "Failed to get EAFController position");
    return std::nullopt;
}

std::optional<double> EAFController::getSpeed() {
    LOG_F(INFO, "EAFController current speed: {}", m_speed);
    return m_speed;
}

bool EAFController::setSpeed(double speed) {
    if (speed < 0.0 || speed > 1.0) {
        LOG_F(ERROR, "Invalid speed value: {}", speed);
        return false;
    }
    m_speed = speed;
    LOG_F(INFO, "EAFController speed set to: {}", speed);
    return true;
}

std::optional<FocusDirection> EAFController::getDirection() {
    bool reversed;
    if (EAFGetReverse(m_deviceID, &reversed) != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to get EAFController direction");
        return std::nullopt;
    }
    LOG_F(INFO, "EAFController direction: {}", reversed ? "IN" : "OUT");
    return reversed ? FocusDirection::IN : FocusDirection::OUT;
}

bool EAFController::setDirection(FocusDirection direction) {
    bool result = EAFSetReverse(m_deviceID, direction == FocusDirection::IN) ==
                  EAF_SUCCESS;
    LOG_F(INFO, "EAFController direction set to: {}, result: {}",
          direction == FocusDirection::IN ? "IN" : "OUT",
          result ? "success" : "failure");
    return result;
}

std::optional<int> EAFController::getMaxLimit() {
    LOG_F(INFO, "EAFController max limit: {}", m_maxLimit);
    return m_maxLimit;
}

bool EAFController::setMaxLimit(int maxLimit) {
    m_maxLimit = maxLimit;
    LOG_F(INFO, "EAFController max limit set to: {}", maxLimit);
    return true;
}

std::optional<bool> EAFController::isReversed() {
    bool reversed;
    if (EAFGetReverse(m_deviceID, &reversed) != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to get EAFController reversed state");
        return std::nullopt;
    }
    LOG_F(INFO, "EAFController reversed state: {}",
          reversed ? "true" : "false");
    return reversed;
}

bool EAFController::setReversed(bool reversed) {
    bool result = EAFSetReverse(m_deviceID, reversed) == EAF_SUCCESS;
    LOG_F(INFO, "EAFController reversed state set to: {}, result: {}",
          reversed ? "true" : "false", result ? "success" : "failure");
    return result;
}

bool EAFController::moveSteps(int steps) {
    int currentPos = getPosition().value_or(0);
    bool result = moveToPosition(currentPos + steps);
    LOG_F(INFO, "EAFController moved steps: {}, result: {}", steps,
          result ? "success" : "failure");
    return result;
}

bool EAFController::moveToPosition(int position) {
    std::lock_guard<std::mutex> lock(g_eafMutex);
    bool result = EAFMove(m_deviceID, position) == EAF_SUCCESS;
    LOG_F(INFO, "EAFController moved to position: {}, result: {}", position,
          result ? "success" : "failure");
    return result;
}

bool EAFController::moveForDuration(int durationMs) {
    if (!m_isOpen) {
        LOG_F(ERROR,
              "Attempted to move EAFController for duration while it is not "
              "open");
        return false;
    }

    std::lock_guard<std::mutex> lock(g_eafMutex);
    int initialPosition;
    if (EAFGetPosition(m_deviceID, &initialPosition) != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to get initial position for moveForDuration");
        return false;
    }

    atom::async::Timer timer;
    int steps = 10;  // 每次移动10步
    int elapsed = 0;

    auto moveStep = [&]() {
        if (elapsed >= durationMs) {
            timer.stop();
            return;
        }
        EAFMove(m_deviceID, initialPosition + steps);
        steps += 10;
        elapsed += 100;  // 假设每次移动间隔100ms
    };

    timer.setInterval(moveStep, 100, -1, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
    timer.stop();

    bool result = EAFStop(m_deviceID) == EAF_SUCCESS;
    LOG_F(INFO, "EAFController moved for duration: {} ms, result: {}",
          durationMs, result ? "success" : "failure");
    return result;
}

bool EAFController::abortMove() {
    bool result = Stop();
    LOG_F(INFO, "EAFController abort move, result: {}",
          result ? "success" : "failure");
    return result;
}

bool EAFController::syncPosition(int position) {
    std::lock_guard<std::mutex> lock(g_eafMutex);
    EAF_ERROR_CODE err = EAFMove(m_deviceID, position);
    if (err != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to sync position to {}, error: {}", position,
              ErrorCodeToString(err));
        return false;
    }

    bool moving = true;
    bool handControl;
    while (moving) {
        err = EAFIsMoving(m_deviceID, &moving, &handControl);
        if (err != EAF_SUCCESS) {
            LOG_F(ERROR,
                  "Error while checking if EAFController is moving during "
                  "sync, error: {}",
                  ErrorCodeToString(err).c_str());
            return false;
        }
        if (handControl) {
            LOG_F(ERROR, "EAFController is in hand control mode during sync");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_F(INFO, "EAFController synced to position: {}", position);
    return true;
}

std::optional<double> EAFController::getExternalTemperature() {
    float temp;
    if (EAFGetTemp(m_deviceID, &temp) == EAF_SUCCESS) {
        LOG_F(INFO, "EAFController external temperature: {}", temp);
        return static_cast<double>(temp);
    }
    LOG_F(ERROR, "Failed to get EAFController external temperature");
    return std::nullopt;
}

std::optional<double> EAFController::getChipTemperature() {
    float temp;
    if (EAFGetTemp(m_deviceID, &temp) == EAF_SUCCESS) {
        LOG_F(INFO, "EAFController chip temperature: {}", temp);
        return static_cast<double>(temp);
    }
    LOG_F(ERROR, "Failed to get EAFController chip temperature");
    return std::nullopt;
}

bool EAFController::UpdateProperties() const {
    std::lock_guard<std::mutex> lock(m_propMutex);

    if (EAFGetProperty(m_deviceID, &m_properties.info) != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to update EAFController properties");
        return false;
    }

    EAFGetReverse(m_deviceID, &m_properties.isReversed);
    EAFGetBacklash(m_deviceID, &m_properties.backlash);
    EAFGetTemp(m_deviceID, &m_properties.temperature);
    EAFGetBeep(m_deviceID, &m_properties.beepEnabled);

    EAFGetFirmwareVersion(m_deviceID, &m_properties.firmwareMajor,
                          &m_properties.firmwareMinor,
                          &m_properties.firmwareBuild);

    EAFGetSerialNumber(m_deviceID, &m_properties.serialNumber);

    LOG_F(INFO, "EAFController properties updated");
    return true;
}

const DeviceProperties& EAFController::GetDeviceProperties() const {
    return m_properties;
}

bool EAFController::disconnect() {
    if (!m_isConnected) {
        LOG_F(WARNING, "EAFController already disconnected");
        return true;
    }

    Close();
    m_isConnected = false;
    LOG_F(INFO, "EAFController disconnected");
    return true;
}

bool EAFController::isConnected() const { return m_isConnected; }

std::vector<std::string> EAFController::scan() {
    std::vector<std::string> ports;
    std::lock_guard<std::mutex> lock(g_eafMutex);

    int count = EAFGetNum();
    for (int i = 0; i < count; ++i) {
        int id;
        if (EAFGetID(i, &id) == EAF_SUCCESS) {
            ports.push_back("EAF_" + std::to_string(id));
            LOG_F(INFO, "Found EAF device with ID: {}", id);
        }
    }

    return ports;
}
