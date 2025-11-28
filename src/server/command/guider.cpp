#include "guider.hpp"

#include "atom/log/loguru.hpp"
#include "server/models/api.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

static std::shared_ptr<internal::PHD2Controller> g_phd2Controller;

auto connectGuider() -> json {
    LOG_F(INFO, "connectGuider: Connecting to PHD2");
    json response;
    
    try {
        if (!g_phd2Controller) {
            g_phd2Controller = std::make_shared<internal::PHD2Controller>();
        }
        
        if (g_phd2Controller->initialize()) {
            response["status"] = "success";
            response["message"] = "PHD2 connected and started.";
        } else {
            response["status"] = "error";
            response["error"] = {
                {"code", "connection_failed"},
                {"message", "Failed to initialize PHD2."}
            };
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "connectGuider: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    return response;
}

auto disconnectGuider() -> json {
    LOG_F(INFO, "disconnectGuider: Disconnecting from PHD2");
    json response;
    
    if (g_phd2Controller) {
        // Since PHD2Controller::initialize uses pkill, we don't have a clean 'stop' 
        // in the class, but we can just reset our handle.
        // Ideally we should send a quit command if supported.
        g_phd2Controller.reset();
    }
    
    response["status"] = "success";
    response["message"] = "Guider disconnected.";
    return response;
}

auto startGuiding() -> json {
    LOG_F(INFO, "startGuiding: Starting guiding");
    json response;
    
    if (!g_phd2Controller) {
        return lithium::models::api::makeError("device_not_connected", "Guider not connected");
    }
    
    if (g_phd2Controller->startGuiding()) {
        response["status"] = "success";
        response["message"] = "Guiding started.";
    } else {
        response["status"] = "error";
        response["error"] = {
            {"code", "command_failed"},
            {"message", "Failed to start guiding."}
        };
    }
    return response;
}

auto stopGuiding() -> json {
    LOG_F(INFO, "stopGuiding: Stopping guiding");
    json response;
    
    if (!g_phd2Controller) {
        return lithium::models::api::makeError("device_not_connected", "Guider not connected");
    }
    
    if (g_phd2Controller->stopLooping()) { // stopLooping stops guiding/looping
        response["status"] = "success";
        response["message"] = "Guiding stopped.";
    } else {
        response["status"] = "error";
        response["error"] = {
            {"code", "command_failed"},
            {"message", "Failed to stop guiding."}
        };
    }
    return response;
}

auto ditherGuider(double pixels) -> json {
    LOG_F(INFO, "ditherGuider: Dither %f pixels", pixels);
    // TODO: Implement dither command code for PHD2
    return lithium::models::api::makeError("feature_not_supported", "Dither not implemented yet");
}

auto getGuiderStatus() -> json {
    json response;
    response["status"] = "success";
    
    if (!g_phd2Controller) {
        response["data"] = {
            {"connected", false},
            {"state", "DISCONNECTED"}
        };
    } else {
        // Basic status - we assume connected if controller exists
        // TODO: Improve state tracking
        response["data"] = {
            {"connected", true},
            {"state", "CONNECTED"} // Placeholder state
        };
    }
    return response;
}

auto getGuiderStats() -> json {
    return lithium::models::api::makeError("feature_not_supported", "Stats not implemented yet");
}

auto setGuiderSettings(const json& settings) -> json {
    LOG_F(INFO, "setGuiderSettings: Updating settings");
    json response;
    
    if (!g_phd2Controller) {
        return lithium::models::api::makeError("device_not_connected", "Guider not connected");
    }
    
    try {
        bool success = true;
        if (settings.contains("exposureTime")) {
             unsigned int exp = settings["exposureTime"];
             if (!g_phd2Controller->setExposureTime(exp)) success = false;
        }
        
        if (settings.contains("focalLength")) {
             int fl = settings["focalLength"];
             if (!g_phd2Controller->setFocalLength(fl)) success = false;
        }
        
        if (success) {
            response["status"] = "success";
            response["message"] = "Settings updated.";
        } else {
             response["status"] = "error";
             response["error"] = {
                 {"code", "command_failed"},
                 {"message", "Some settings failed to update."}
             };
        }
    } catch (const std::exception& e) {
        return lithium::models::api::makeError("invalid_json", e.what());
    }
    
    return response;
}

} // namespace lithium::middleware
