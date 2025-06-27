#include "focuser.hpp"
#include "focuser/modular_focuser.hpp"

#include <spdlog/spdlog.h>

#include "atom/components/component.hpp"

// Type alias for cleaner code
using ModularFocuser = lithium::device::indi::focuser::ModularINDIFocuser;

ATOM_MODULE(focuser_indi, [](Component &component) {
    auto logger = spdlog::get("focuser");
    if (!logger) {
        logger = spdlog::default_logger();
    }
    logger->info("Registering modular focuser_indi module...");
    
    component.doc("INDI Focuser - Modular Implementation");
    
    // Device lifecycle
    component.def("initialize", &ModularFocuser::initialize, "device",
                  "Initialize a focuser device.");
    component.def("destroy", &ModularFocuser::destroy, "device",
                  "Destroy a focuser device.");
    component.def("connect", &ModularFocuser::connect, "device",
                  "Connect to a focuser device.");
    component.def("disconnect", &ModularFocuser::disconnect, "device",
                  "Disconnect from a focuser device.");
    component.def("reconnect", [](ModularFocuser* self, int timeout, int maxRetry, const std::string& deviceName) {
        return self->disconnect() && self->connect(deviceName, timeout, maxRetry);
    }, "device", "Reconnect to a focuser device.");
    component.def("scan", &ModularFocuser::scan, "device",
                  "Scan for focuser devices.");
    component.def("is_connected", &ModularFocuser::isConnected, "device",
                  "Check if a focuser device is connected.");

    // Speed control
    component.def("get_focuser_speed", &ModularFocuser::getSpeed, "device",
                  "Get the focuser speed.");
    component.def("set_focuser_speed", &ModularFocuser::setSpeed, "device",
                  "Set the focuser speed.");
    component.def("get_max_speed", &ModularFocuser::getMaxSpeed, "device",
                  "Get maximum focuser speed.");
    component.def("get_speed_range", &ModularFocuser::getSpeedRange, "device",
                  "Get focuser speed range.");

    // Direction control
    component.def("get_move_direction", &ModularFocuser::getDirection, "device",
                  "Get the focuser move direction.");
    component.def("set_move_direction", &ModularFocuser::setDirection, "device",
                  "Set the focuser move direction.");

    // Position limits
    component.def("get_max_limit", &ModularFocuser::getMaxLimit, "device",
                  "Get the focuser max limit.");
    component.def("set_max_limit", &ModularFocuser::setMaxLimit, "device",
                  "Set the focuser max limit.");
    component.def("get_min_limit", &ModularFocuser::getMinLimit, "device",
                  "Get the focuser min limit.");
    component.def("set_min_limit", &ModularFocuser::setMinLimit, "device",
                  "Set the focuser min limit.");

    // Reverse control
    component.def("is_reversed", &ModularFocuser::isReversed, "device",
                  "Get whether the focuser reverse is enabled.");
    component.def("set_reversed", &ModularFocuser::setReversed, "device",
                  "Set whether the focuser reverse is enabled.");

    // Movement control
    component.def("is_moving", &ModularFocuser::isMoving, "device",
                  "Check if focuser is currently moving.");
    component.def("move_steps", &ModularFocuser::moveSteps, "device",
                  "Move the focuser steps.");
    component.def("move_to_position", &ModularFocuser::moveToPosition, "device",
                  "Move the focuser to absolute position.");
    component.def("get_position", &ModularFocuser::getPosition, "device",
                  "Get the focuser absolute position.");
    component.def("move_for_duration", &ModularFocuser::moveForDuration, "device",
                  "Move the focuser with time.");
    component.def("abort_move", &ModularFocuser::abortMove, "device",
                  "Abort the focuser move.");
    component.def("sync_position", &ModularFocuser::syncPosition, "device",
                  "Sync the focuser position.");
    component.def("move_inward", &ModularFocuser::moveInward, "device",
                  "Move focuser inward by steps.");
    component.def("move_outward", &ModularFocuser::moveOutward, "device",
                  "Move focuser outward by steps.");

    // Backlash compensation
    component.def("get_backlash", &ModularFocuser::getBacklash, "device",
                  "Get backlash compensation steps.");
    component.def("set_backlash", &ModularFocuser::setBacklash, "device",
                  "Set backlash compensation steps.");
    component.def("enable_backlash_compensation", &ModularFocuser::enableBacklashCompensation, "device",
                  "Enable/disable backlash compensation.");
    component.def("is_backlash_compensation_enabled", &ModularFocuser::isBacklashCompensationEnabled, "device",
                  "Check if backlash compensation is enabled.");

    // Temperature monitoring
    component.def("get_external_temperature", &ModularFocuser::getExternalTemperature, "device",
                  "Get the focuser external temperature.");
    component.def("get_chip_temperature", &ModularFocuser::getChipTemperature, "device",
                  "Get the focuser chip temperature.");
    component.def("has_temperature_sensor", &ModularFocuser::hasTemperatureSensor, "device",
                  "Check if focuser has temperature sensor.");

    // Temperature compensation
    component.def("get_temperature_compensation", &ModularFocuser::getTemperatureCompensation, "device",
                  "Get temperature compensation settings.");
    component.def("set_temperature_compensation", &ModularFocuser::setTemperatureCompensation, "device",
                  "Set temperature compensation settings.");
    component.def("enable_temperature_compensation", &ModularFocuser::enableTemperatureCompensation, "device",
                  "Enable/disable temperature compensation.");

    // Auto-focus
    component.def("start_auto_focus", &ModularFocuser::startAutoFocus, "device",
                  "Start auto-focus routine.");
    component.def("stop_auto_focus", &ModularFocuser::stopAutoFocus, "device",
                  "Stop auto-focus routine.");
    component.def("is_auto_focusing", &ModularFocuser::isAutoFocusing, "device",
                  "Check if auto-focus is running.");
    component.def("get_auto_focus_progress", &ModularFocuser::getAutoFocusProgress, "device",
                  "Get auto-focus progress (0.0-1.0).");

    // Preset management
    component.def("save_preset", &ModularFocuser::savePreset, "device",
                  "Save current position to preset slot.");
    component.def("load_preset", &ModularFocuser::loadPreset, "device",
                  "Load position from preset slot.");
    component.def("get_preset", &ModularFocuser::getPreset, "device",
                  "Get position from preset slot.");
    component.def("delete_preset", &ModularFocuser::deletePreset, "device",
                  "Delete preset from slot.");

    // Statistics
    component.def("get_total_steps", &ModularFocuser::getTotalSteps, "device",
                  "Get total steps moved since reset.");
    component.def("reset_total_steps", &ModularFocuser::resetTotalSteps, "device",
                  "Reset total steps counter.");
    component.def("get_last_move_steps", &ModularFocuser::getLastMoveSteps, "device",
                  "Get steps from last move.");
    component.def("get_last_move_duration", &ModularFocuser::getLastMoveDuration, "device",
                  "Get duration of last move in milliseconds.");

    // Factory method
    component.def(
        "create_instance",
        [](const std::string &name) {
            std::shared_ptr<AtomFocuser> instance =
                std::make_shared<ModularFocuser>(name);
            return instance;
        },
        "device", "Create a new modular focuser instance.");
    
    component.defType<ModularFocuser>("focuser_indi", "device",
                                   "Define a new modular focuser instance.");

    logger->info("Registered modular focuser_indi module.");
});
