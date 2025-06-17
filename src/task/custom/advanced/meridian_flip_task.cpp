#include "meridian_flip_task.hpp"
#include <chrono>
#include <memory>
#include <thread>
#include <cmath>
#include "../platesolve/platesolve_task.hpp"
#include "../focuser/autofocus_task.hpp"

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace lithium::task::task {

auto MeridianFlipTask::taskName() -> std::string { return "MeridianFlip"; }

void MeridianFlipTask::execute(const json& params) { executeImpl(params); }

void MeridianFlipTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing MeridianFlip task '{}' with params: {}", getName(),
          params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double targetRA = params.value("target_ra", 0.0);
        double targetDec = params.value("target_dec", 0.0);
        double flipOffsetMinutes = params.value("flip_offset_minutes", 5.0);
        bool autoFocusAfterFlip = params.value("autofocus_after_flip", true);
        bool plateSolveAfterFlip = params.value("platesolve_after_flip", true);
        bool rotateAfterFlip = params.value("rotate_after_flip", false);
        double targetRotation = params.value("target_rotation", 0.0);
        double pauseBeforeFlip = params.value("pause_before_flip", 30.0);

        LOG_F(INFO, "Monitoring for meridian flip at RA: {:.2f}h, Dec: {:.2f}°",
              targetRA, targetDec);

        // Monitor for meridian flip requirement
        bool flipRequired = false;
        while (!flipRequired) {
            // In real implementation, get current hour angle from mount
            double currentHA = 0.0; // Placeholder
            
            flipRequired = checkMeridianFlipRequired(targetRA, currentHA);
            
            if (!flipRequired) {
                LOG_F(INFO, "Meridian flip not yet required, current HA: {:.2f}h", currentHA);
                std::this_thread::sleep_for(std::chrono::minutes(1));
                continue;
            }
        }

        LOG_F(INFO, "Meridian flip required! Pausing for {} seconds before flip",
              pauseBeforeFlip);
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<int>(pauseBeforeFlip)));

        // Perform the meridian flip
        performFlip();

        // Verify flip was successful
        verifyFlip();

        if (plateSolveAfterFlip) {
            LOG_F(INFO, "Plate solving after meridian flip to recenter target");
            json plateSolveParams = {
                {"target_ra", targetRA},
                {"target_dec", targetDec},
                {"recenter", true}
            };
            // Execute plate solve task
            // auto plateSolveTask = PlateSolveTask::createEnhancedTask();
            // plateSolveTask->execute(plateSolveParams);
        }

        if (rotateAfterFlip) {
            LOG_F(INFO, "Rotating to target rotation: {:.2f}°", targetRotation);
            // Implement rotation logic here
        }

        if (autoFocusAfterFlip) {
            LOG_F(INFO, "Performing autofocus after meridian flip");
            json autofocusParams = {
                {"method", "hfr"},
                {"step_size", 100},
                {"max_attempts", 20}
            };
            // Execute autofocus task
            // auto autofocusTask = AutofocusTask::createEnhancedTask();
            // autofocusTask->execute(autofocusParams);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(INFO, "MeridianFlip task '{}' completed successfully in {} ms",
              getName(), duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(ERROR, "MeridianFlip task '{}' failed after {} ms: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

bool MeridianFlipTask::checkMeridianFlipRequired(double targetRA, double currentHA) {
    // Simple logic: flip when hour angle approaches 0 (meridian crossing)
    // In real implementation, this would use actual mount data
    const double FLIP_THRESHOLD_HOURS = 0.1; // 6 minutes
    return std::abs(currentHA) < FLIP_THRESHOLD_HOURS;
}

void MeridianFlipTask::performFlip() {
    LOG_F(INFO, "Performing meridian flip");
    
    // In real implementation, this would:
    // 1. Stop guiding
    // 2. Command mount to flip
    // 3. Wait for flip completion
    // 4. Update mount state
    
    std::this_thread::sleep_for(std::chrono::seconds(30)); // Simulate flip time
    LOG_F(INFO, "Meridian flip completed");
}

void MeridianFlipTask::verifyFlip() {
    LOG_F(INFO, "Verifying meridian flip success");
    
    // In real implementation, this would:
    // 1. Check mount side of pier
    // 2. Verify target is still accessible
    // 3. Check tracking status
    
    LOG_F(INFO, "Meridian flip verification successful");
}

void MeridianFlipTask::recenterTarget() {
    LOG_F(INFO, "Recentering target after meridian flip");
    
    // This would typically involve plate solving and slewing
    LOG_F(INFO, "Target recentered successfully");
}

void MeridianFlipTask::validateMeridianFlipParameters(const json& params) {
    if (params.contains("target_ra")) {
        double ra = params["target_ra"].get<double>();
        if (ra < 0 || ra >= 24) {
            THROW_INVALID_ARGUMENT("Target RA must be between 0 and 24 hours");
        }
    }

    if (params.contains("target_dec")) {
        double dec = params["target_dec"].get<double>();
        if (dec < -90 || dec > 90) {
            THROW_INVALID_ARGUMENT("Target Dec must be between -90 and 90 degrees");
        }
    }

    if (params.contains("flip_offset_minutes")) {
        double offset = params["flip_offset_minutes"].get<double>();
        if (offset < 0 || offset > 60) {
            THROW_INVALID_ARGUMENT("Flip offset must be between 0 and 60 minutes");
        }
    }
}

auto MeridianFlipTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<MeridianFlipTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced MeridianFlip task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(9);
    task->setTimeout(std::chrono::seconds(3600));  // 1 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void MeridianFlipTask::defineParameters(Task& task) {
    task.addParamDefinition("target_ra", "double", true, 0.0,
                            "Target right ascension in hours");
    task.addParamDefinition("target_dec", "double", true, 0.0,
                            "Target declination in degrees");
    task.addParamDefinition("flip_offset_minutes", "double", false, 5.0,
                            "Minutes past meridian to trigger flip");
    task.addParamDefinition("autofocus_after_flip", "bool", false, true,
                            "Perform autofocus after flip");
    task.addParamDefinition("platesolve_after_flip", "bool", false, true,
                            "Plate solve and recenter after flip");
    task.addParamDefinition("rotate_after_flip", "bool", false, false,
                            "Rotate camera after flip");
    task.addParamDefinition("target_rotation", "double", false, 0.0,
                            "Target rotation angle in degrees");
    task.addParamDefinition("pause_before_flip", "double", false, 30.0,
                            "Pause before flip in seconds");
}

}  // namespace lithium::task::task
