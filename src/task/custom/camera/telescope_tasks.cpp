#include "telescope_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include <cmath>

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "../../../utils/logging/spdlog_config.hpp"
#include "atom/type/json.hpp"

#define MOCK_TELESCOPE

namespace lithium::task::task {

// ==================== Mock Telescope System ====================
#ifdef MOCK_TELESCOPE
class MockTelescope {
public:
    struct TelescopeState {
        double ra = 12.0;  // hours
        double dec = 45.0; // degrees
        double targetRA = 12.0;
        double targetDEC = 45.0;
        double azimuth = 180.0;
        double altitude = 45.0;
        bool isTracking = false;
        bool isSlewing = false;
        bool isParked = false;
        bool isConnected = true;
        std::string status = "Idle";
        double slewRate = 2.0;
        double pierSide = 0; // 0=East, 1=West
        std::string trackMode = "Sidereal";
    };

    static auto getInstance() -> MockTelescope& {
        static MockTelescope instance;
        return instance;
    }

    auto slewToTarget(double ra, double dec, bool enableTracking = true) -> bool {
        if (!state_.isConnected) return false;
        
        state_.targetRA = ra;
        state_.targetDEC = dec;
        state_.isSlewing = true;
        state_.status = "Slewing";
        
        spdlog::info("Telescope slewing to RA: {:.2f}h, DEC: {:.2f}°", ra, dec);
        
        // Simulate slew time based on distance
        double deltaRA = std::abs(ra - state_.ra);
        double deltaDEC = std::abs(dec - state_.dec);
        double distance = std::sqrt(deltaRA*deltaRA + deltaDEC*deltaDEC);
        int slewTimeMs = static_cast<int>(distance * 1000 / state_.slewRate);
        
        // Simulate slewing in background
        std::thread([this, ra, dec, enableTracking, slewTimeMs]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(slewTimeMs));
            state_.ra = ra;
            state_.dec = dec;
            state_.isSlewing = false;
            state_.isTracking = enableTracking;
            state_.status = enableTracking ? "Tracking" : "Idle";
            spdlog::info("Telescope slew completed. Now at RA: {:.2f}h, DEC: {:.2f}°", ra, dec);
        }).detach();
        
        return true;
    }

    auto enableTracking(bool enable) -> bool {
        if (state_.isSlewing) return false;
        
        state_.isTracking = enable;
        state_.status = enable ? "Tracking" : "Idle";
        spdlog::info("Telescope tracking: {}", enable ? "ON" : "OFF");
        return true;
    }

    auto park() -> bool {
        if (state_.isSlewing) return false;
        
        state_.isParked = true;
        state_.isTracking = false;
        state_.status = "Parked";
        spdlog::info("Telescope parked");
        return true;
    }

    auto unpark() -> bool {
        state_.isParked = false;
        state_.status = "Idle";
        spdlog::info("Telescope unparked");
        return true;
    }

    auto abortSlew() -> bool {
        if (state_.isSlewing) {
            state_.isSlewing = false;
            state_.status = "Aborted";
            spdlog::info("Telescope slew aborted");
            return true;
        }
        return false;
    }

    auto sync(double ra, double dec) -> bool {
        state_.ra = ra;
        state_.dec = dec;
        spdlog::info("Telescope synced to RA: {:.2f}h, DEC: {:.2f}°", ra, dec);
        return true;
    }

    auto setSlewRate(double rate) -> bool {
        state_.slewRate = std::clamp(rate, 0.5, 5.0);
        spdlog::info("Telescope slew rate set to: {:.1f}", state_.slewRate);
        return true;
    }

    auto checkMeridianFlip() -> bool {
        // Simple pier side simulation
        if (state_.ra > 18.0 || state_.ra < 6.0) {
            return state_.pierSide != 1; // Need flip to West
        } else {
            return state_.pierSide != 0; // Need flip to East
        }
    }

    auto performMeridianFlip() -> bool {
        if (!checkMeridianFlip()) return true;
        
        spdlog::info("Performing meridian flip");
        state_.isSlewing = true;
        state_.status = "Meridian Flip";
        
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            state_.pierSide = (state_.pierSide == 0) ? 1 : 0;
            state_.isSlewing = false;
            state_.status = "Tracking";
            spdlog::info("Meridian flip completed");
        }).detach();
        
        return true;
    }

    auto getTelescopeInfo() const -> json {
        return json{
            {"position", {
                {"ra", state_.ra},
                {"dec", state_.dec},
                {"azimuth", state_.azimuth},
                {"altitude", state_.altitude}
            }},
            {"target", {
                {"ra", state_.targetRA},
                {"dec", state_.targetDEC}
            }},
            {"status", {
                {"tracking", state_.isTracking},
                {"slewing", state_.isSlewing},
                {"parked", state_.isParked},
                {"connected", state_.isConnected},
                {"status_text", state_.status}
            }},
            {"settings", {
                {"slew_rate", state_.slewRate},
                {"pier_side", state_.pierSide},
                {"track_mode", state_.trackMode}
            }}
        };
    }

    auto getState() const -> const TelescopeState& {
        return state_;
    }

private:
    TelescopeState state_;
};
#endif

// ==================== TelescopeGotoImagingTask Implementation ====================

auto TelescopeGotoImagingTask::taskName() -> std::string {
    return "TelescopeGotoImaging";
}

void TelescopeGotoImagingTask::execute(const json& params) {
    try {
        validateTelescopeParameters(params);
        
        double targetRA = params["target_ra"];
        double targetDEC = params["target_dec"];
        bool enableTracking = params.value("enable_tracking", true);
        bool waitForSlew = params.value("wait_for_slew", true);
        
        spdlog::info("Telescope goto imaging: RA {:.3f}h, DEC {:.3f}°", targetRA, targetDEC);
        
#ifdef MOCK_TELESCOPE
        auto& telescope = MockTelescope::getInstance();
        
        if (!telescope.slewToTarget(targetRA, targetDEC, enableTracking)) {
            throw atom::error::RuntimeError("Failed to start telescope slew");
        }
        
        if (waitForSlew) {
            // Wait for slew to complete
            while (telescope.getState().isSlewing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                spdlog::debug("Waiting for telescope slew to complete...");
            }
            
            // Check if tracking is enabled as requested
            if (enableTracking && !telescope.getState().isTracking) {
                telescope.enableTracking(true);
            }
        }
#endif
        
        LOG_F(INFO, "Telescope goto imaging completed successfully");
        
    } catch (const std::exception& e) {
        handleTelescopeError(*this, e);
        throw;
    }
}

auto TelescopeGotoImagingTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<TelescopeGotoImagingTask>("TelescopeGotoImaging", 
        [](const json& params) {
            TelescopeGotoImagingTask taskInstance("TelescopeGotoImaging", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void TelescopeGotoImagingTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "target_ra",
        .type = "number",
        .required = true,
        .defaultValue = 12.0,
        .description = "Target right ascension in hours (0-24)"
    });
    
    task.addParameter({
        .name = "target_dec",
        .type = "number",
        .required = true,
        .defaultValue = 45.0,
        .description = "Target declination in degrees (-90 to +90)"
    });
    
    task.addParameter({
        .name = "enable_tracking",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Enable tracking after slew"
    });
    
    task.addParameter({
        .name = "wait_for_slew",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Wait for slew completion before finishing task"
    });
}

void TelescopeGotoImagingTask::validateTelescopeParameters(const json& params) {
    if (!params.contains("target_ra")) {
        throw atom::error::InvalidArgument("Missing required parameter: target_ra");
    }
    
    if (!params.contains("target_dec")) {
        throw atom::error::InvalidArgument("Missing required parameter: target_dec");
    }
    
    double ra = params["target_ra"];
    double dec = params["target_dec"];
    
    if (ra < 0.0 || ra >= 24.0) {
        throw atom::error::InvalidArgument("Right ascension must be between 0 and 24 hours");
    }
    
    if (dec < -90.0 || dec > 90.0) {
        throw atom::error::InvalidArgument("Declination must be between -90 and +90 degrees");
    }
}

void TelescopeGotoImagingTask::handleTelescopeError(Task& task, const std::exception& e) {
    task.setErrorType(TaskErrorType::DeviceError);
    spdlog::error("Telescope goto imaging error: {}", e.what());
}

// ==================== TrackingControlTask Implementation ====================

auto TrackingControlTask::taskName() -> std::string {
    return "TrackingControl";
}

void TrackingControlTask::execute(const json& params) {
    try {
        validateTrackingParameters(params);
        
        bool enable = params["enable"];
        std::string trackMode = params.value("track_mode", "sidereal");
        
        spdlog::info("Setting telescope tracking: {} (mode: {})", enable ? "ON" : "OFF", trackMode);
        
#ifdef MOCK_TELESCOPE
        auto& telescope = MockTelescope::getInstance();
        
        if (!telescope.enableTracking(enable)) {
            throw atom::error::RuntimeError("Failed to set tracking mode");
        }
#endif
        
        LOG_F(INFO, "Tracking control completed successfully");
        
    } catch (const std::exception& e) {
        spdlog::error("TrackingControlTask failed: {}", e.what());
        throw;
    }
}

auto TrackingControlTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<TrackingControlTask>("TrackingControl", 
        [](const json& params) {
            TrackingControlTask taskInstance("TrackingControl", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void TrackingControlTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "enable",
        .type = "boolean",
        .required = true,
        .defaultValue = true,
        .description = "Enable or disable telescope tracking"
    });
    
    task.addParameter({
        .name = "track_mode",
        .type = "string",
        .required = false,
        .defaultValue = "sidereal",
        .description = "Tracking mode (sidereal, solar, lunar)"
    });
}

void TrackingControlTask::validateTrackingParameters(const json& params) {
    if (!params.contains("enable")) {
        throw atom::error::InvalidArgument("Missing required parameter: enable");
    }
    
    if (params.contains("track_mode")) {
        std::string mode = params["track_mode"];
        std::vector<std::string> validModes = {"sidereal", "solar", "lunar", "custom"};
        if (std::find(validModes.begin(), validModes.end(), mode) == validModes.end()) {
            throw atom::error::InvalidArgument("Invalid tracking mode");
        }
    }
}

// ==================== MeridianFlipTask Implementation ====================

auto MeridianFlipTask::taskName() -> std::string {
    return "MeridianFlip";
}

void MeridianFlipTask::execute(const json& params) {
    try {
        validateMeridianFlipParameters(params);
        
        bool autoCheck = params.value("auto_check", true);
        bool forceFlip = params.value("force_flip", false);
        double timeLimit = params.value("time_limit", 300.0);
        
        spdlog::info("Meridian flip check: auto={}, force={}", autoCheck, forceFlip);
        
#ifdef MOCK_TELESCOPE
        auto& telescope = MockTelescope::getInstance();
        
        bool needsFlip = forceFlip || (autoCheck && telescope.checkMeridianFlip());
        
        if (needsFlip) {
            spdlog::info("Meridian flip required, executing...");
            
            if (!telescope.performMeridianFlip()) {
                throw atom::error::RuntimeError("Failed to perform meridian flip");
            }
            
            // Wait for flip completion with timeout
            auto startTime = std::chrono::steady_clock::now();
            while (telescope.getState().isSlewing) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                
                if (elapsed > timeLimit) {
                    throw atom::error::RuntimeError("Meridian flip timeout");
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            
            spdlog::info("Meridian flip completed successfully");
        } else {
            spdlog::info("No meridian flip required");
        }
#endif
        
        LOG_F(INFO, "Meridian flip task completed");
        
    } catch (const std::exception& e) {
        spdlog::error("MeridianFlipTask failed: {}", e.what());
        throw;
    }
}

auto MeridianFlipTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<MeridianFlipTask>("MeridianFlip", 
        [](const json& params) {
            MeridianFlipTask taskInstance("MeridianFlip", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void MeridianFlipTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "auto_check",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Automatically check if meridian flip is needed"
    });
    
    task.addParameter({
        .name = "force_flip",
        .type = "boolean",
        .required = false,
        .defaultValue = false,
        .description = "Force meridian flip regardless of position"
    });
    
    task.addParameter({
        .name = "time_limit",
        .type = "number",
        .required = false,
        .defaultValue = 300.0,
        .description = "Maximum time to wait for flip completion (seconds)"
    });
}

void MeridianFlipTask::validateMeridianFlipParameters(const json& params) {
    if (params.contains("time_limit")) {
        double timeLimit = params["time_limit"];
        if (timeLimit < 30.0 || timeLimit > 1800.0) {
            throw atom::error::InvalidArgument("Time limit must be between 30 and 1800 seconds");
        }
    }
}

// ==================== TelescopeParkTask Implementation ====================

auto TelescopeParkTask::taskName() -> std::string {
    return "TelescopePark";
}

void TelescopeParkTask::execute(const json& params) {
    try {
        bool park = params.value("park", true);
        bool stopTracking = params.value("stop_tracking", true);
        
        spdlog::info("Telescope park operation: {}", park ? "PARK" : "UNPARK");
        
#ifdef MOCK_TELESCOPE
        auto& telescope = MockTelescope::getInstance();
        
        if (park) {
            if (stopTracking) {
                telescope.enableTracking(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            
            if (!telescope.park()) {
                throw atom::error::RuntimeError("Failed to park telescope");
            }
        } else {
            if (!telescope.unpark()) {
                throw atom::error::RuntimeError("Failed to unpark telescope");
            }
        }
#endif
        
        LOG_F(INFO, "Telescope park operation completed");
        
    } catch (const std::exception& e) {
        spdlog::error("TelescopeParkTask failed: {}", e.what());
        throw;
    }
}

auto TelescopeParkTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<TelescopeParkTask>("TelescopePark", 
        [](const json& params) {
            TelescopeParkTask taskInstance("TelescopePark", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void TelescopeParkTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "park",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Park (true) or unpark (false) telescope"
    });
    
    task.addParameter({
        .name = "stop_tracking",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Stop tracking before parking"
    });
}

// ==================== PointingModelTask Implementation ====================

auto PointingModelTask::taskName() -> std::string {
    return "PointingModel";
}

void PointingModelTask::execute(const json& params) {
    try {
        int pointCount = params.value("point_count", 20);
        bool autoSelect = params.value("auto_select", true);
        double exposureTime = params.value("exposure_time", 3.0);
        
        spdlog::info("Building pointing model with {} points", pointCount);
        
        // This would integrate with plate solving and star catalogues
        // For now, simulate the process
        
        for (int i = 0; i < pointCount; ++i) {
            // Select target point (would use star catalogue)
            double ra = 2.0 + (i * 20.0 / pointCount);  // Spread across sky
            double dec = -60.0 + (i * 120.0 / pointCount);
            
            spdlog::info("Pointing model point {}/{}: RA {:.2f}h, DEC {:.2f}°", 
                        i+1, pointCount, ra, dec);
            
#ifdef MOCK_TELESCOPE
            auto& telescope = MockTelescope::getInstance();
            
            // Slew to target
            telescope.slewToTarget(ra, dec, false);
            while (telescope.getState().isSlewing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Simulate exposure and plate solving
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(exposureTime * 1000)));
            
            // Simulate sync (in real implementation, use plate solve result)
            telescope.sync(ra + 0.001, dec + 0.001);  // Small error correction
#endif
        }
        
        spdlog::info("Pointing model completed with {} points", pointCount);
        LOG_F(INFO, "Pointing model task completed");
        
    } catch (const std::exception& e) {
        spdlog::error("PointingModelTask failed: {}", e.what());
        throw;
    }
}

auto PointingModelTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<PointingModelTask>("PointingModel", 
        [](const json& params) {
            PointingModelTask taskInstance("PointingModel", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void PointingModelTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "point_count",
        .type = "integer",
        .required = false,
        .defaultValue = 20,
        .description = "Number of points to measure"
    });
    
    task.addParameter({
        .name = "auto_select",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Automatically select pointing stars"
    });
    
    task.addParameter({
        .name = "exposure_time",
        .type = "number",
        .required = false,
        .defaultValue = 3.0,
        .description = "Exposure time for each pointing measurement"
    });
}

void PointingModelTask::validatePointingModelParameters(const json& params) {
    if (params.contains("point_count")) {
        int count = params["point_count"];
        if (count < 5 || count > 100) {
            throw atom::error::InvalidArgument("Point count must be between 5 and 100");
        }
    }
    
    if (params.contains("exposure_time")) {
        double exposure = params["exposure_time"];
        if (exposure < 0.1 || exposure > 60.0) {
            throw atom::error::InvalidArgument("Exposure time must be between 0.1 and 60 seconds");
        }
    }
}

// ==================== SlewSpeedOptimizationTask Implementation ====================

auto SlewSpeedOptimizationTask::taskName() -> std::string {
    return "SlewSpeedOptimization";
}

void SlewSpeedOptimizationTask::execute(const json& params) {
    try {
        std::string optimizationTarget = params.value("target", "accuracy");
        bool adaptiveSpeed = params.value("adaptive_speed", true);
        
        spdlog::info("Optimizing slew speed for: {}", optimizationTarget);
        
#ifdef MOCK_TELESCOPE
        auto& telescope = MockTelescope::getInstance();
        
        double optimalSpeed = 2.0;  // Default
        
        if (optimizationTarget == "speed") {
            optimalSpeed = 4.0;  // Fast slews
        } else if (optimizationTarget == "accuracy") {
            optimalSpeed = 1.5;  // Slow, accurate slews
        } else if (optimizationTarget == "balanced") {
            optimalSpeed = 2.5;  // Balanced approach
        }
        
        telescope.setSlewRate(optimalSpeed);
        
        spdlog::info("Slew speed optimized to: {:.1f}", optimalSpeed);
#endif
        
        LOG_F(INFO, "Slew speed optimization completed");
        
    } catch (const std::exception& e) {
        spdlog::error("SlewSpeedOptimizationTask failed: {}", e.what());
        throw;
    }
}

auto SlewSpeedOptimizationTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<SlewSpeedOptimizationTask>("SlewSpeedOptimization", 
        [](const json& params) {
            SlewSpeedOptimizationTask taskInstance("SlewSpeedOptimization", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void SlewSpeedOptimizationTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "target",
        .type = "string",
        .required = false,
        .defaultValue = "accuracy",
        .description = "Optimization target (speed, accuracy, balanced)"
    });
    
    task.addParameter({
        .name = "adaptive_speed",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Use adaptive speed based on slew distance"
    });
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register TelescopeGotoImagingTask
AUTO_REGISTER_TASK(
    TelescopeGotoImagingTask, "TelescopeGotoImaging",
    (TaskInfo{
        .name = "TelescopeGotoImaging",
        .description = "Slews telescope to target coordinates and sets up for imaging",
        .category = "Telescope",
        .requiredParameters = {"target_ra", "target_dec"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"target_ra", json{{"type", "number"},
                                          {"minimum", 0},
                                          {"maximum", 24}}},
                       {"target_dec", json{{"type", "number"},
                                           {"minimum", -90},
                                           {"maximum", 90}}},
                       {"enable_tracking", json{{"type", "boolean"}}},
                       {"wait_for_slew", json{{"type", "boolean"}}}}},
                 {"required", json::array({"target_ra", "target_dec"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register TrackingControlTask
AUTO_REGISTER_TASK(
    TrackingControlTask, "TrackingControl",
    (TaskInfo{
        .name = "TrackingControl",
        .description = "Controls telescope tracking during imaging sessions",
        .category = "Telescope",
        .requiredParameters = {"enable"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"enable", json{{"type", "boolean"}}},
                       {"track_mode", json{{"type", "string"},
                                           {"enum", json::array({"sidereal", "solar", "lunar", "custom"})}}}}},
                 {"required", json::array({"enable"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register MeridianFlipTask
AUTO_REGISTER_TASK(
    MeridianFlipTask, "MeridianFlip",
    (TaskInfo{
        .name = "MeridianFlip",
        .description = "Handles meridian flip operations for continuous imaging",
        .category = "Telescope",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"auto_check", json{{"type", "boolean"}}},
                       {"force_flip", json{{"type", "boolean"}}},
                       {"time_limit", json{{"type", "number"},
                                           {"minimum", 30},
                                           {"maximum", 1800}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register TelescopeParkTask
AUTO_REGISTER_TASK(
    TelescopeParkTask, "TelescopePark",
    (TaskInfo{
        .name = "TelescopePark",
        .description = "Parks or unparks telescope safely",
        .category = "Telescope",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"park", json{{"type", "boolean"}}},
                       {"stop_tracking", json{{"type", "boolean"}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register PointingModelTask
AUTO_REGISTER_TASK(
    PointingModelTask, "PointingModel",
    (TaskInfo{
        .name = "PointingModel",
        .description = "Builds pointing model for improved telescope accuracy",
        .category = "Telescope",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"point_count", json{{"type", "integer"},
                                             {"minimum", 5},
                                             {"maximum", 100}}},
                       {"auto_select", json{{"type", "boolean"}}},
                       {"exposure_time", json{{"type", "number"},
                                              {"minimum", 0.1},
                                              {"maximum", 60}}}}}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

// Register SlewSpeedOptimizationTask
AUTO_REGISTER_TASK(
    SlewSpeedOptimizationTask, "SlewSpeedOptimization",
    (TaskInfo{
        .name = "SlewSpeedOptimization",
        .description = "Optimizes telescope slew speeds for different scenarios",
        .category = "Telescope",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"target", json{{"type", "string"},
                                       {"enum", json::array({"speed", "accuracy", "balanced"})}}},
                       {"adaptive_speed", json{{"type", "boolean"}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

}  // namespace
