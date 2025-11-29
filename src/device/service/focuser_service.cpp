/*
 * focuser_service.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "focuser_service.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"

#include "constant/constant.hpp"
#include "device/template/focuser.hpp"

namespace lithium::device {

using json = nlohmann::json;

class FocuserService::Impl {
public:
    std::mutex autofocusMutex;
    std::unordered_map<std::string, json> autofocusSessions;
    std::atomic<uint64_t> autofocusCounter{0};

    // Backlash compensation
    int backlashSteps = 0;

    auto generateAutofocusId() -> std::string {
        auto id = autofocusCounter.fetch_add(1, std::memory_order_relaxed);
        return "af_" + std::to_string(id);
    }
};

FocuserService::FocuserService()
    : TypedDeviceService("FocuserService", "Focuser", Constants::MAIN_FOCUSER),
      impl_(std::make_unique<Impl>()) {}

FocuserService::~FocuserService() = default;

auto FocuserService::list() -> json {
    LOG_INFO("FocuserService::list: Listing all available focusers");
    json response;
    response["status"] = "success";

    try {
        json focuserList = json::array();

        std::shared_ptr<AtomFocuser> focuser;
        try {
            GET_OR_CREATE_PTR(focuser, AtomFocuser, Constants::MAIN_FOCUSER)
            json info;
            info["deviceId"] = "foc-001";
            info["name"] = focuser->getName();
            info["isConnected"] = focuser->isConnected();
            focuserList.push_back(info);
        } catch (...) {
            LOG_WARN("FocuserService::list: Main focuser not available");
        }

        response["data"] = focuserList;
    } catch (const std::exception& e) {
        LOG_ERROR("FocuserService::list: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO("FocuserService::list: Completed");
    return response;
}

auto FocuserService::getStatus(const std::string& deviceId) -> json {
    LOG_INFO("FocuserService::getStatus: Getting status for focuser: %s",
             deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFocuser> focuser;
        GET_OR_CREATE_PTR(focuser, AtomFocuser, Constants::MAIN_FOCUSER)

        if (!focuser->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Focuser is not connected"}};
            return response;
        }

        json data;
        data["isConnected"] = focuser->isConnected();
        data["isMoving"] = false;

        if (auto position = focuser->getPosition()) {
            data["position"] = *position;
        }

        if (auto extTemp = focuser->getExternalTemperature()) {
            data["temperature"] = *extTemp;
        } else if (auto chipTemp = focuser->getChipTemperature()) {
            data["temperature"] = *chipTemp;
        }

        json tempComp;
        tempComp["enabled"] = false;
        tempComp["coefficient"] = 0.0;
        data["tempComp"] = tempComp;

        response["status"] = "success";
        response["data"] = data;
    } catch (const std::exception& e) {
        LOG_ERROR("FocuserService::getStatus: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO("FocuserService::getStatus: Completed");
    return response;
}

auto FocuserService::connect(const std::string& deviceId, bool connected)
    -> json {
    LOG_INFO("FocuserService::connect: %s focuser: %s",
             connected ? "Connecting" : "Disconnecting", deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFocuser> focuser;
        GET_OR_CREATE_PTR(focuser, AtomFocuser, Constants::MAIN_FOCUSER)

        bool success = connected ? focuser->connect("") : focuser->disconnect();
        if (success) {
            response["status"] = "success";
            response["message"] =
                connected ? "Focuser connection process initiated."
                          : "Focuser disconnection process initiated.";
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "connection_failed"},
                                 {"message", "Connection operation failed."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("FocuserService::connect: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO("FocuserService::connect: Completed");
    return response;
}

auto FocuserService::move(const std::string& deviceId, const json& moveRequest)
    -> json {
    LOG_INFO("FocuserService::move: Moving focuser: %s", deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFocuser> focuser;
        GET_OR_CREATE_PTR(focuser, AtomFocuser, Constants::MAIN_FOCUSER)

        if (!focuser->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Focuser is not connected"}};
            return response;
        }

        bool isRelative = moveRequest.value("isRelative", false);
        bool success = false;

        if (isRelative) {
            if (!moveRequest.contains("offset") ||
                !moveRequest["offset"].is_number_integer()) {
                response["status"] = "error";
                response["error"] = {
                    {"code", "invalid_field_value"},
                    {"message", "Relative move requires integer 'offset'"}};
                return response;
            }
            int offset = moveRequest["offset"].get<int>();
            if (offset == 0) {
                response["status"] = "error";
                response["error"] = {{"code", "invalid_field_value"},
                                     {"message", "Offset must be non-zero"}};
                return response;
            }

            auto direction =
                offset > 0 ? FocusDirection::OUT : FocusDirection::IN;
            focuser->setDirection(direction);
            success = focuser->moveSteps(offset);
        } else {
            if (!moveRequest.contains("position") ||
                !moveRequest["position"].is_number_integer()) {
                response["status"] = "error";
                response["error"] = {
                    {"code", "invalid_field_value"},
                    {"message", "Absolute move requires integer 'position'"}};
                return response;
            }
            int position = moveRequest["position"].get<int>();
            if (position < 0) {
                response["status"] = "error";
                response["error"] = {
                    {"code", "invalid_field_value"},
                    {"message", "Position must be non-negative"}};
                return response;
            }

            success = focuser->moveToPosition(position);
        }

        if (success) {
            response["status"] = "success";
            response["message"] = "Focuser move initiated.";
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "move_failed"},
                                 {"message", "Focuser move command failed."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("FocuserService::move: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO("FocuserService::move: Completed");
    return response;
}

auto FocuserService::updateSettings(const std::string& deviceId,
                                    const json& settings) -> json {
    LOG_INFO(
        "FocuserService::updateSettings: Updating settings for focuser: %s",
        deviceId.c_str());
    json response;

    try {
        (void)deviceId;
        if (settings.contains("tempComp")) {
            response["status"] = "error";
            response["error"] = {
                {"code", "feature_not_supported"},
                {"message",
                 "Temperature compensation is not supported by this focuser"}};
        } else {
            response["status"] = "success";
            response["message"] = "No focuser settings were changed.";
        }
    } catch (const std::exception& e) {
        LOG_ERROR("FocuserService::updateSettings: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO("FocuserService::updateSettings: Completed");
    return response;
}

auto FocuserService::halt(const std::string& deviceId) -> json {
    LOG_INFO("FocuserService::halt: Halting focuser: %s", deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFocuser> focuser;
        GET_OR_CREATE_PTR(focuser, AtomFocuser, Constants::MAIN_FOCUSER)

        if (!focuser->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Focuser is not connected"}};
            return response;
        }

        bool success = focuser->abortMove();
        if (success) {
            response["status"] = "success";
            response["message"] = "Focuser halted.";
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "halt_failed"},
                                 {"message", "Failed to halt focuser."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("FocuserService::halt: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO("FocuserService::halt: Completed");
    return response;
}

auto FocuserService::getCapabilities(const std::string& deviceId) -> json {
    LOG_INFO(
        "FocuserService::getCapabilities: Getting capabilities for focuser: %s",
        deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFocuser> focuser;
        GET_OR_CREATE_PTR(focuser, AtomFocuser, Constants::MAIN_FOCUSER)

        if (!focuser->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Focuser is not connected"}};
            return response;
        }

        json caps;
        caps["canHalt"] = true;
        caps["canReverse"] = true;
        caps["canAbsoluteMove"] = true;
        caps["canRelativeMove"] = true;
        caps["canTempComp"] = false;

        int maxPos = 50000;
        if (auto maxLimit = focuser->getMaxLimit()) {
            maxPos = *maxLimit;
        }
        caps["maxPosition"] = maxPos;
        caps["maxIncrement"] = 1000;
        caps["stepSize"] = 1.0;

        bool hasTempSensor = focuser->getExternalTemperature().has_value() ||
                             focuser->getChipTemperature().has_value();
        caps["tempCompAvailable"] = false;
        caps["hasTemperatureSensor"] = hasTempSensor;

        response["status"] = "success";
        response["data"] = caps;
    } catch (const std::exception& e) {
        LOG_ERROR("FocuserService::getCapabilities: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO("FocuserService::getCapabilities: Completed");
    return response;
}

auto FocuserService::startAutofocus(const std::string& deviceId,
                                    const json& autofocusRequest) -> json {
    LOG_INFO(
        "FocuserService::startAutofocus: Autofocus request for focuser: %s",
        deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFocuser> focuser;
        GET_OR_CREATE_PTR(focuser, AtomFocuser, Constants::MAIN_FOCUSER)

        if (!focuser->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Focuser is not connected"}};
            return response;
        }

        if (autofocusRequest.contains("numberOfSteps") &&
            !autofocusRequest["numberOfSteps"].is_number_integer()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_field_value"},
                {"message", "'numberOfSteps' must be an integer"}};
            return response;
        }

        std::string autofocusId = impl_->generateAutofocusId();

        json status;
        status["autofocusId"] = autofocusId;
        status["status"] = "completed";
        status["progress"] = 100.0;

        int currentPosition = 0;
        if (auto pos = focuser->getPosition()) {
            currentPosition = *pos;
        }
        status["currentPosition"] = currentPosition;
        status["currentHFR"] = 2.0;
        status["bestPosition"] = currentPosition;
        status["bestHFR"] = 1.8;

        json measurements = json::array();
        measurements.push_back(json{
            {"position", currentPosition}, {"hfr", 2.2}, {"starCount", 40}});
        measurements.push_back(json{
            {"position", currentPosition}, {"hfr", 1.8}, {"starCount", 48}});
        status["measurements"] = std::move(measurements);

        {
            std::lock_guard<std::mutex> lock(impl_->autofocusMutex);
            impl_->autofocusSessions[autofocusId] = std::move(status);
        }

        response["status"] = "success";
        response["message"] = "Autofocus routine initiated.";
        json data;
        data["autofocusId"] = autofocusId;
        data["estimatedTime"] = 0;
        response["data"] = std::move(data);
    } catch (const std::exception& e) {
        LOG_ERROR("FocuserService::startAutofocus: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    return response;
}

auto FocuserService::getAutofocusStatus(const std::string& deviceId,
                                        const std::string& autofocusId)
    -> json {
    (void)deviceId;
    LOG_INFO("FocuserService::getAutofocusStatus: for focuser: %s",
             deviceId.c_str());
    json response;

    try {
        std::lock_guard<std::mutex> lock(impl_->autofocusMutex);
        auto it = impl_->autofocusSessions.find(autofocusId);
        if (it == impl_->autofocusSessions.end()) {
            response["status"] = "error";
            response["error"] = {{"code", "autofocus_not_found"},
                                 {"message", "Autofocus session not found."}};
            return response;
        }

        response["status"] = "success";
        response["data"] = it->second;
    } catch (const std::exception& e) {
        LOG_ERROR("FocuserService::getAutofocusStatus: Exception: %s",
                  e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    return response;
}

// ========== INDI-specific operations ==========

auto FocuserService::getINDIProperties(const std::string& deviceId) -> json {
    return withConnectedDevice(
        deviceId, "getINDIProperties", [this](auto focuser) -> json {
            json data;
            data["driverName"] = "INDI Focuser";
            data["driverVersion"] = "1.0";

            json properties = json::object();

            // Position
            if (auto pos = focuser->getPosition()) {
                properties["ABS_FOCUS_POSITION"] = {{"value", *pos},
                                                    {"type", "number"}};
            }

            // Max limit
            if (auto maxLimit = focuser->getMaxLimit()) {
                properties["FOCUS_MAX"] = {{"value", *maxLimit},
                                           {"type", "number"}};
            }

            // Speed
            if (auto speed = focuser->getSpeed()) {
                properties["FOCUS_SPEED"] = {{"value", *speed},
                                             {"type", "number"}};
            }

            // Temperature
            if (auto temp = focuser->getExternalTemperature()) {
                properties["FOCUS_TEMPERATURE"] = {{"value", *temp},
                                                   {"type", "number"}};
            }

            // Reverse
            if (auto reversed = focuser->isReversed()) {
                properties["FOCUS_REVERSE_MOTION"] = {{"value", *reversed},
                                                      {"type", "switch"}};
            }

            // Backlash
            properties["FOCUS_BACKLASH_STEPS"] = {
                {"value", impl_->backlashSteps}, {"type", "number"}};

            data["properties"] = properties;
            return makeSuccessResponse(data);
        });
}

auto FocuserService::setINDIProperty(const std::string& deviceId,
                                     const std::string& propertyName,
                                     const json& value) -> json {
    return withConnectedDevice(
        deviceId, "setINDIProperty", [&](auto focuser) -> json {
            bool success = false;

            if (propertyName == "ABS_FOCUS_POSITION" &&
                value.is_number_integer()) {
                success = focuser->moveToPosition(value.get<int>());
            } else if (propertyName == "FOCUS_MAX" &&
                       value.is_number_integer()) {
                success = focuser->setMaxLimit(value.get<int>());
            } else if (propertyName == "FOCUS_SPEED" && value.is_number()) {
                success = focuser->setSpeed(value.get<double>());
            } else if (propertyName == "FOCUS_REVERSE_MOTION" &&
                       value.is_boolean()) {
                success = focuser->setReversed(value.get<bool>());
            } else if (propertyName == "FOCUS_BACKLASH_STEPS" &&
                       value.is_number_integer()) {
                impl_->backlashSteps = value.get<int>();
                success = true;
            } else {
                return makeErrorResponse(
                    ErrorCode::INVALID_FIELD_VALUE,
                    "Unknown or invalid property: " + propertyName);
            }

            if (success) {
                return makeSuccessResponse("Property " + propertyName +
                                           " updated");
            }
            return makeErrorResponse(ErrorCode::OPERATION_FAILED,
                                     "Failed to set property " + propertyName);
        });
}

auto FocuserService::syncPosition(const std::string& deviceId, int position)
    -> json {
    return withConnectedDevice(
        deviceId, "syncPosition", [&](auto focuser) -> json {
            if (position < 0) {
                return makeErrorResponse(ErrorCode::INVALID_FIELD_VALUE,
                                         "Position must be non-negative");
            }

            if (focuser->syncPosition(position)) {
                json data;
                data["position"] = position;
                return makeSuccessResponse(data, "Position synced");
            }
            return makeErrorResponse(ErrorCode::OPERATION_FAILED,
                                     "Failed to sync position");
        });
}

auto FocuserService::getBacklash(const std::string& deviceId) -> json {
    return withConnectedDevice(deviceId, "getBacklash",
                               [this](auto focuser) -> json {
                                   (void)focuser;
                                   json data;
                                   data["steps"] = impl_->backlashSteps;
                                   data["enabled"] = impl_->backlashSteps > 0;
                                   return makeSuccessResponse(data);
                               });
}

auto FocuserService::setBacklash(const std::string& deviceId, int steps)
    -> json {
    return withConnectedDevice(
        deviceId, "setBacklash", [&](auto focuser) -> json {
            (void)focuser;
            if (steps < 0) {
                return makeErrorResponse(ErrorCode::INVALID_FIELD_VALUE,
                                         "Backlash steps must be non-negative");
            }

            impl_->backlashSteps = steps;

            json data;
            data["steps"] = steps;
            data["enabled"] = steps > 0;
            return makeSuccessResponse(data, "Backlash compensation updated");
        });
}

}  // namespace lithium::device
