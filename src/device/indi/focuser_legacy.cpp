#include "focuser.hpp"
#include "focuser_main.hpp"

#include <spdlog/spdlog.h>

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "device/template/focuser.hpp"

// Use the modular implementation as INDIFocuser for backward compatibility
using INDIFocuser = lithium::device::indi::focuser::ModularINDIFocuser;

ATOM_MODULE(focuser_indi, [](Component &component) {
    auto logger = spdlog::get("focuser");
    if (!logger) {
        logger = spdlog::default_logger();
    }
    logger->info("Registering modular focuser_indi module...");
    
    component.doc("INDI Focuser - Modular Implementation");
    
    // Device lifecycle
    component.def("initialize", &INDIFocuser::initialize, "device",
                  "Initialize a focuser device.");
    component.def("destroy", &INDIFocuser::destroy, "device",
                  "Destroy a focuser device.");
    component.def("connect", &INDIFocuser::connect, "device",
                  "Connect to a focuser device.");
    component.def("disconnect", &INDIFocuser::disconnect, "device",
                  "Disconnect from a focuser device.");
    component.def("reconnect", &INDIFocuser::reconnect, "device",
                  "Reconnect to a focuser device.");
    component.def("scan", &INDIFocuser::scan, "device",
                  "Scan for focuser devices.");
    component.def("is_connected", &INDIFocuser::isConnected, "device",
                  "Check if a focuser device is connected.");

    // Speed control
    component.def("get_focuser_speed", &INDIFocuser::getSpeed, "device",
                  "Get the focuser speed.");
    component.def("set_focuser_speed", &INDIFocuser::setSpeed, "device",
                  "Set the focuser speed.");
    component.def("get_max_speed", &INDIFocuser::getMaxSpeed, "device",
                  "Get maximum focuser speed.");
    component.def("get_speed_range", &INDIFocuser::getSpeedRange, "device",
                  "Get focuser speed range.");

    // Direction control
    component.def("get_move_direction", &INDIFocuser::getDirection, "device",
                  "Get the focuser move direction.");
    component.def("set_move_direction", &INDIFocuser::setDirection, "device",
                  "Set the focuser move direction.");

    // Position limits
    component.def("get_max_limit", &INDIFocuser::getMaxLimit, "device",
                  "Get the focuser max limit.");
    component.def("set_max_limit", &INDIFocuser::setMaxLimit, "device",
                  "Set the focuser max limit.");
    component.def("get_min_limit", &INDIFocuser::getMinLimit, "device",
                  "Get the focuser min limit.");
    component.def("set_min_limit", &INDIFocuser::setMinLimit, "device",
                  "Set the focuser min limit.");

    // Reverse control
    component.def("is_reversed", &INDIFocuser::isReversed, "device",
                  "Get whether the focuser reverse is enabled.");
    component.def("set_reversed", &INDIFocuser::setReversed, "device",
                  "Set whether the focuser reverse is enabled.");

    // Movement control
    component.def("is_moving", &INDIFocuser::isMoving, "device",
                  "Check if focuser is currently moving.");
    component.def("move_steps", &INDIFocuser::moveSteps, "device",
                  "Move the focuser steps.");
    component.def("move_to_position", &INDIFocuser::moveToPosition, "device",
                  "Move the focuser to absolute position.");
    component.def("get_position", &INDIFocuser::getPosition, "device",
                  "Get the focuser absolute position.");
    component.def("move_for_duration", &INDIFocuser::moveForDuration, "device",
                  "Move the focuser with time.");
    component.def("abort_move", &INDIFocuser::abortMove, "device",
                  "Abort the focuser move.");
    component.def("sync_position", &INDIFocuser::syncPosition, "device",
                  "Sync the focuser position.");
    component.def("move_inward", &INDIFocuser::moveInward, "device",
                  "Move focuser inward by steps.");
    component.def("move_outward", &INDIFocuser::moveOutward, "device",
                  "Move focuser outward by steps.");

    // Backlash compensation
    component.def("get_backlash", &INDIFocuser::getBacklash, "device",
                  "Get backlash compensation steps.");
    component.def("set_backlash", &INDIFocuser::setBacklash, "device",
                  "Set backlash compensation steps.");
    component.def("enable_backlash_compensation", &INDIFocuser::enableBacklashCompensation, "device",
                  "Enable/disable backlash compensation.");
    component.def("is_backlash_compensation_enabled", &INDIFocuser::isBacklashCompensationEnabled, "device",
                  "Check if backlash compensation is enabled.");

    // Temperature monitoring
    component.def("get_external_temperature", &INDIFocuser::getExternalTemperature, "device",
                  "Get the focuser external temperature.");
    component.def("get_chip_temperature", &INDIFocuser::getChipTemperature, "device",
                  "Get the focuser chip temperature.");
    component.def("has_temperature_sensor", &INDIFocuser::hasTemperatureSensor, "device",
                  "Check if focuser has temperature sensor.");

    // Temperature compensation
    component.def("get_temperature_compensation", &INDIFocuser::getTemperatureCompensation, "device",
                  "Get temperature compensation settings.");
    component.def("set_temperature_compensation", &INDIFocuser::setTemperatureCompensation, "device",
                  "Set temperature compensation settings.");
    component.def("enable_temperature_compensation", &INDIFocuser::enableTemperatureCompensation, "device",
                  "Enable/disable temperature compensation.");

    // Auto-focus
    component.def("start_auto_focus", &INDIFocuser::startAutoFocus, "device",
                  "Start auto-focus routine.");
    component.def("stop_auto_focus", &INDIFocuser::stopAutoFocus, "device",
                  "Stop auto-focus routine.");
    component.def("is_auto_focusing", &INDIFocuser::isAutoFocusing, "device",
                  "Check if auto-focus is running.");
    component.def("get_auto_focus_progress", &INDIFocuser::getAutoFocusProgress, "device",
                  "Get auto-focus progress (0.0-1.0).");

    // Preset management
    component.def("save_preset", &INDIFocuser::savePreset, "device",
                  "Save current position to preset slot.");
    component.def("load_preset", &INDIFocuser::loadPreset, "device",
                  "Load position from preset slot.");
    component.def("get_preset", &INDIFocuser::getPreset, "device",
                  "Get position from preset slot.");
    component.def("delete_preset", &INDIFocuser::deletePreset, "device",
                  "Delete preset from slot.");

    // Statistics
    component.def("get_total_steps", &INDIFocuser::getTotalSteps, "device",
                  "Get total steps moved since reset.");
    component.def("reset_total_steps", &INDIFocuser::resetTotalSteps, "device",
                  "Reset total steps counter.");
    component.def("get_last_move_steps", &INDIFocuser::getLastMoveSteps, "device",
                  "Get steps from last move.");
    component.def("get_last_move_duration", &INDIFocuser::getLastMoveDuration, "device",
                  "Get duration of last move in milliseconds.");

    // Factory method
    component.def(
        "create_instance",
        [](const std::string &name) {
            std::shared_ptr<AtomFocuser> instance =
                std::make_shared<INDIFocuser>(name);
            return instance;
        },
        "device", "Create a new modular focuser instance.");
    
    component.defType<INDIFocuser>("focuser_indi", "device",
                                   "Define a new modular focuser instance.");

    logger->info("Registered modular focuser_indi module.");
});
