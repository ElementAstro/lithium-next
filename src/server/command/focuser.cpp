
#include "focuser.hpp"

#include <memory>

#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"

#include "device/template/focuser.hpp"

#include "constant/constant.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

// List all available focusers
auto listFocusers() -> json {
    LOG_F(INFO, "listFocusers: Listing all available focusers");
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
            LOG_F(WARNING, "listFocusers: Main focuser not available");
        }

        response["data"] = focuserList;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "listFocusers: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }

    LOG_F(INFO, "listFocusers: Completed");
    return response;
}

// Get focuser status
auto getFocuserStatus(const std::string &deviceId) -> json {
    LOG_F(INFO, "getFocuserStatus: Getting status for focuser: %s",
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
        // AtomFocuser interface does not expose motion state; report false.
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
    } catch (const std::exception &e) {
        LOG_F(ERROR, "getFocuserStatus: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }

    LOG_F(INFO, "getFocuserStatus: Completed");
    return response;
}

// Connect / disconnect focuser
auto connectFocuser(const std::string &deviceId, bool connected) -> json {
    LOG_F(INFO, "connectFocuser: %s focuser: %s",
          connected ? "Connecting" : "Disconnecting", deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFocuser> focuser;
        GET_OR_CREATE_PTR(focuser, AtomFocuser, Constants::MAIN_FOCUSER)

        bool success = connected ? focuser->connect("") : focuser->disconnect();
        if (success) {
            response["status"] = "success";
            response["message"] = connected
                                       ? "Focuser connection process initiated."
                                       : "Focuser disconnection process initiated.";
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "connection_failed"},
                                   {"message", "Connection operation failed."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "connectFocuser: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }

    LOG_F(INFO, "connectFocuser: Completed");
    return response;
}

// Move focuser (absolute or relative)
auto moveFocuser(const std::string &deviceId, const json &moveRequest) -> json {
    LOG_F(INFO, "moveFocuser: Moving focuser: %s", deviceId.c_str());
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

            auto direction = offset > 0 ? FocusDirection::OUT : FocusDirection::IN;
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
                response["error"] = {{"code", "invalid_field_value"},
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
    } catch (const std::exception &e) {
        LOG_F(ERROR, "moveFocuser: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }

    LOG_F(INFO, "moveFocuser: Completed");
    return response;
}

// Update focuser settings (currently temp compensation only; not implemented)
auto updateFocuserSettings(const std::string &deviceId,
                           const json &settings) -> json {
    LOG_F(INFO, "updateFocuserSettings: Updating settings for focuser: %s",
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
    } catch (const std::exception &e) {
        LOG_F(ERROR, "updateFocuserSettings: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }

    LOG_F(INFO, "updateFocuserSettings: Completed");
    return response;
}

// Halt focuser movement
auto haltFocuser(const std::string &deviceId) -> json {
    LOG_F(INFO, "haltFocuser: Halting focuser: %s", deviceId.c_str());
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
    } catch (const std::exception &e) {
        LOG_F(ERROR, "haltFocuser: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }

    LOG_F(INFO, "haltFocuser: Completed");
    return response;
}

// Get focuser capabilities
auto getFocuserCapabilities(const std::string &deviceId) -> json {
    LOG_F(INFO, "getFocuserCapabilities: Getting capabilities for focuser: %s",
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

        bool hasTempSensor =
            focuser->getExternalTemperature().has_value() ||
            focuser->getChipTemperature().has_value();
        caps["tempCompAvailable"] = false;
        caps["hasTemperatureSensor"] = hasTempSensor;

        response["status"] = "success";
        response["data"] = caps;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "getFocuserCapabilities: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }

    LOG_F(INFO, "getFocuserCapabilities: Completed");
    return response;
}

// Autofocus is currently not implemented; return explicit error
auto startAutofocus(const std::string &deviceId,
                    const json &autofocusRequest) -> json {
    (void)deviceId;
    (void)autofocusRequest;
    LOG_F(INFO, "startAutofocus: Autofocus request received for focuser: %s",
          deviceId.c_str());
    json response;
    response["status"] = "error";
    response["error"] = {{"code", "feature_not_supported"},
                           {"message", "Autofocus is not implemented yet."}};
    return response;
}

auto getAutofocusStatus(const std::string &deviceId,
                        const std::string &autofocusId) -> json {
    (void)deviceId;
    (void)autofocusId;
    LOG_F(INFO,
          "getAutofocusStatus: Autofocus status requested for focuser: %s",
          deviceId.c_str());
    json response;
    response["status"] = "error";
    response["error"] = {{"code", "feature_not_supported"},
                           {"message", "Autofocus status is not available."}};
    return response;
}

}  // namespace lithium::middleware

