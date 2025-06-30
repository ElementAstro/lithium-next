#include "advanced_tasks.hpp"
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace lithium::task::advanced {

void registerAdvancedTasks() {
    LOG_F(INFO, "Registering advanced astrophotography tasks...");
    
    // Tasks are automatically registered via the AUTO_REGISTER_TASK macros
    // in their respective implementation files
    
    LOG_F(INFO, "Advanced tasks registration completed");
}

std::vector<std::string> getAdvancedTaskNames() {
    return {
        "SmartExposure",
        "DeepSkySequence", 
        "PlanetaryImaging",
        "Timelapse",
        "MeridianFlip",
        "IntelligentSequence",
        "AutoCalibration",
        "WeatherMonitor",
        "ObservatoryAutomation",
        "MosaicImaging",
        "FocusOptimization"
    };
}

bool isAdvancedTask(const std::string& taskName) {
    const auto advancedTasks = getAdvancedTaskNames();
    return std::find(advancedTasks.begin(), advancedTasks.end(), taskName) 
           != advancedTasks.end();
}

}  // namespace lithium::task::advanced
