#include "filterwheel/modular_filterwheel.hpp"

#include <spdlog/spdlog.h>

#include "atom/components/component.hpp"
#include "atom/components/module_macro.hpp"
#include "atom/components/registry.hpp"

// Type alias for cleaner code
using ModularFilterWheel = lithium::device::indi::filterwheel::ModularINDIFilterWheel;

ATOM_EMBED_MODULE(filterwheel_indi, [](Component &component) {
    auto logger = spdlog::get("filterwheel");
    if (!logger) {
        logger = spdlog::default_logger();
    }
    logger->info("Registering modular filterwheel_indi module...");
    
    component.doc("INDI FilterWheel - Modular Implementation");
    
    // Device lifecycle
    component.def("initialize", &ModularFilterWheel::initialize, "device",
                  "Initialize a filterwheel device.");
    component.def("destroy", &ModularFilterWheel::destroy, "device",
                  "Destroy a filterwheel device.");
    component.def("connect", &ModularFilterWheel::connect, "device",
                  "Connect to a filterwheel device.");
    component.def("disconnect", &ModularFilterWheel::disconnect, "device",
                  "Disconnect from a filterwheel device.");
    component.def("reconnect", [](ModularFilterWheel* self, int timeout, int maxRetry, const std::string& deviceName) {
        return self->disconnect() && self->connect(deviceName, timeout, maxRetry);
    }, "device", "Reconnect to a filterwheel device.");
    component.def("scan", &ModularFilterWheel::scan, "device",
                  "Scan for filterwheel devices.");
    component.def("is_connected", &ModularFilterWheel::isConnected, "device",
                  "Check if a filterwheel device is connected.");

    // Filter control
    component.def("get_position", &ModularFilterWheel::getPosition, "device",
                  "Get the current filter position.");
    component.def("set_position", &ModularFilterWheel::setPosition, "device",
                  "Set the filter position.");
    component.def("get_filter_count", &ModularFilterWheel::getFilterCount, "device",
                  "Get the maximum filter count.");
    component.def("is_valid_position", &ModularFilterWheel::isValidPosition, "device",
                  "Check if position is valid.");
    component.def("is_moving", &ModularFilterWheel::isMoving, "device",
                  "Check if filterwheel is currently moving.");
    component.def("abort_motion", &ModularFilterWheel::abortMotion, "device",
                  "Abort filterwheel movement.");

    // Filter information
    component.def("get_slot_name", &ModularFilterWheel::getSlotName, "device",
                  "Get the name of a specific filter slot.");
    component.def("set_slot_name", &ModularFilterWheel::setSlotName, "device",
                  "Set the name of a specific filter slot.");
    component.def("get_all_slot_names", &ModularFilterWheel::getAllSlotNames, "device",
                  "Get all filter slot names.");
    component.def("get_current_filter_name", &ModularFilterWheel::getCurrentFilterName, "device",
                  "Get current filter name.");

    // Enhanced filter management
    component.def("get_filter_info", &ModularFilterWheel::getFilterInfo, "device",
                  "Get filter information for a slot.");
    component.def("set_filter_info", &ModularFilterWheel::setFilterInfo, "device",
                  "Set filter information for a slot.");
    component.def("get_all_filter_info", &ModularFilterWheel::getAllFilterInfo, "device",
                  "Get all filter information.");

    // Filter search and selection
    component.def("find_filter_by_name", &ModularFilterWheel::findFilterByName, "device",
                  "Find filter position by name.");
    component.def("select_filter_by_name", &ModularFilterWheel::selectFilterByName, "device",
                  "Select filter by name.");

    // Temperature
    component.def("get_temperature", &ModularFilterWheel::getTemperature, "device",
                  "Get filterwheel temperature.");
    component.def("has_temperature_sensor", &ModularFilterWheel::hasTemperatureSensor, "device",
                  "Check if filterwheel has temperature sensor.");

    // Statistics
    component.def("get_total_moves", &ModularFilterWheel::getTotalMoves, "device",
                  "Get total number of filter moves.");
    component.def("get_last_move_time", &ModularFilterWheel::getLastMoveTime, "device",
                  "Get time of last filter move.");
    component.def("reset_total_moves", &ModularFilterWheel::resetTotalMoves, "device",
                  "Reset filter move statistics.");

    // Factory method
    component.def(
        "create_instance",
        [](const std::string &name) {
            std::shared_ptr<AtomFilterWheel> instance =
                std::make_shared<ModularFilterWheel>(name);
            return instance;
        },
        "device", "Create a new modular filterwheel instance.");
    
    component.defType<ModularFilterWheel>("filterwheel_indi", "device",
                                         "Define a new modular filterwheel instance.");

    logger->info("Registered modular filterwheel_indi module.");
});
