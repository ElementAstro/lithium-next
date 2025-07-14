#include "intelligent_sequence_task.hpp"
#include <chrono>
#include <memory>
#include <thread>
#include <algorithm>
#include <cmath>

#include "../../task.hpp"
#include "deep_sky_sequence_task.hpp"

#include "atom/error/exception.hpp"
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace lithium::task::task {

auto IntelligentSequenceTask::taskName() -> std::string { return "IntelligentSequence"; }

void IntelligentSequenceTask::execute(const json& params) { executeImpl(params); }

void IntelligentSequenceTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing IntelligentSequence task '{}' with params: {}",
          getName(), params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::vector<json> targets = params["targets"];
        double sessionDuration = params.value("session_duration_hours", 8.0);
        double minAltitude = params.value("min_altitude", 30.0);
        bool weatherMonitoring = params.value("weather_monitoring", true);
        double cloudCoverLimit = params.value("cloud_cover_limit", 30.0);
        double windSpeedLimit = params.value("wind_speed_limit", 15.0);
        bool autoMeridianFlip = params.value("auto_meridian_flip", true);
        bool dynamicTargetSelection = params.value("dynamic_target_selection", true);

        LOG_F(INFO, "Starting intelligent sequence for {} targets over {:.1f}h",
              targets.size(), sessionDuration);

        auto sessionEnd = std::chrono::steady_clock::now() +
                         std::chrono::hours(static_cast<int>(sessionDuration));

        int completedTargets = 0;
        while (std::chrono::steady_clock::now() < sessionEnd) {
            // Check weather conditions if monitoring enabled
            if (weatherMonitoring && !checkWeatherConditions()) {
                LOG_F(WARNING, "Weather conditions unfavorable, pausing sequence");
                std::this_thread::sleep_for(std::chrono::minutes(10));
                continue;
            }

            // Select best target based on current conditions
            json bestTarget;
            if (dynamicTargetSelection) {
                bestTarget = selectBestTarget(targets);
                if (bestTarget.empty()) {
                    LOG_F(INFO, "No suitable targets available, waiting 15 minutes");
                    std::this_thread::sleep_for(std::chrono::minutes(15));
                    continue;
                }
            } else {
                // Use sequential target selection
                if (completedTargets < targets.size()) {
                    bestTarget = targets[completedTargets];
                } else {
                    LOG_F(INFO, "All targets completed in sequential mode");
                    break;
                }
            }

            LOG_F(INFO, "Selected target: {}", bestTarget["name"].get<std::string>());

            // Execute imaging sequence for the selected target
            try {
                executeTargetSequence(bestTarget);
                completedTargets++;

                // Mark target as completed for dynamic selection
                if (dynamicTargetSelection) {
                    for (auto& target : targets) {
                        if (target["name"] == bestTarget["name"]) {
                            target["completed"] = true;
                            break;
                        }
                    }
                }

            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to complete target {}: {}",
                      bestTarget["name"].get<std::string>(), e.what());

                if (!dynamicTargetSelection) {
                    completedTargets++; // Skip failed target in sequential mode
                }
            }

            // Check if all targets completed
            if (dynamicTargetSelection) {
                bool allCompleted = std::all_of(targets.begin(), targets.end(),
                    [](const json& target) { return target.value("completed", false); });
                if (allCompleted) {
                    LOG_F(INFO, "All targets completed successfully");
                    break;
                }
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(INFO, "IntelligentSequence task '{}' completed after {} minutes, {} targets processed",
              getName(), duration.count(), completedTargets);

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(ERROR, "IntelligentSequence task '{}' failed after {} minutes: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

json IntelligentSequenceTask::selectBestTarget(const std::vector<json>& targets) {
    json bestTarget;
    double bestPriority = -1.0;

    for (const auto& target : targets) {
        if (target.value("completed", false)) {
            continue; // Skip completed targets
        }

        if (!checkTargetVisibility(target)) {
            continue; // Skip non-visible targets
        }

        double priority = calculateTargetPriority(target);
        if (priority > bestPriority) {
            bestPriority = priority;
            bestTarget = target;
        }
    }

    return bestTarget;
}

bool IntelligentSequenceTask::checkWeatherConditions() {
    // In real implementation, this would check actual weather data
    // For now, simulate with random conditions

    // Simulate cloud cover (0-100%)
    double cloudCover = 20.0; // Placeholder
    // Simulate wind speed (km/h)
    double windSpeed = 8.0; // Placeholder
    // Simulate humidity (%)
    double humidity = 65.0; // Placeholder

    LOG_F(INFO, "Weather check - Clouds: {:.1f}%, Wind: {:.1f}km/h, Humidity: {:.1f}%",
          cloudCover, windSpeed, humidity);

    return cloudCover < 30.0 && windSpeed < 15.0 && humidity < 80.0;
}

bool IntelligentSequenceTask::checkTargetVisibility(const json& target) {
    // In real implementation, this would calculate actual altitude/azimuth
    double targetRA = target["ra"].get<double>();
    double targetDec = target["dec"].get<double>();
    double minAltitude = target.value("min_altitude", 30.0);

    // Simplified visibility check (placeholder)
    // Real implementation would use astronomical calculations
    double currentAltitude = 45.0; // Placeholder

    bool isVisible = currentAltitude >= minAltitude;

    if (!isVisible) {
        LOG_F(INFO, "Target {} not visible - altitude {:.1f}° < {:.1f}°",
              target["name"].get<std::string>(), currentAltitude, minAltitude);
    }

    return isVisible;
}

void IntelligentSequenceTask::executeTargetSequence(const json& target) {
    LOG_F(INFO, "Executing sequence for target: {}", target["name"].get<std::string>());

    // Prepare parameters for deep sky sequence
    json sequenceParams = {
        {"target_name", target["name"]},
        {"total_exposures", target.value("exposures", 20)},
        {"exposure_time", target.value("exposure_time", 300.0)},
        {"filters", target.value("filters", json::array({"L"}))},
        {"dithering", target.value("dithering", true)},
        {"binning", target.value("binning", 1)},
        {"gain", target.value("gain", 100)},
        {"offset", target.value("offset", 10)}
    };

    // Execute the deep sky sequence
    auto deepSkyTask = DeepSkySequenceTask::createEnhancedTask();
    deepSkyTask->execute(sequenceParams);

    LOG_F(INFO, "Target sequence completed for: {}", target["name"].get<std::string>());
}

double IntelligentSequenceTask::calculateTargetPriority(const json& target) {
    double priority = 0.0;

    // Base priority from target configuration
    priority += target.value("priority", 5.0);

    // Higher priority for targets with more remaining exposures
    int totalExposures = target.value("exposures", 20);
    int completedExposures = target.value("completed_exposures", 0);
    double completionRatio = static_cast<double>(completedExposures) / totalExposures;
    priority += (1.0 - completionRatio) * 3.0;

    // Altitude bonus (higher altitude = higher priority)
    double altitude = 45.0; // Placeholder - would be calculated
    priority += (altitude - 30.0) / 60.0 * 2.0; // 0-2 point bonus

    // Meridian proximity penalty (avoid targets near meridian flip)
    double hourAngle = 0.0; // Placeholder - would be calculated
    if (std::abs(hourAngle) < 1.0) {
        priority -= 2.0; // Penalty for being near meridian
    }

    // Weather stability bonus
    if (checkWeatherConditions()) {
        priority += 1.0;
    }

    LOG_F(INFO, "Target {} priority: {:.2f}",
          target["name"].get<std::string>(), priority);

    return priority;
}

void IntelligentSequenceTask::validateIntelligentSequenceParameters(const json& params) {
    if (!params.contains("targets") || !params["targets"].is_array()) {
        THROW_INVALID_ARGUMENT("Missing or invalid targets array");
    }

    if (params["targets"].empty()) {
        THROW_INVALID_ARGUMENT("Targets array cannot be empty");
    }

    for (const auto& target : params["targets"]) {
        if (!target.contains("name") || !target["name"].is_string()) {
            THROW_INVALID_ARGUMENT("Each target must have a name");
        }
        if (!target.contains("ra") || !target["ra"].is_number()) {
            THROW_INVALID_ARGUMENT("Each target must have RA coordinates");
        }
        if (!target.contains("dec") || !target["dec"].is_number()) {
            THROW_INVALID_ARGUMENT("Each target must have Dec coordinates");
        }
    }

    if (params.contains("session_duration_hours")) {
        double duration = params["session_duration_hours"].get<double>();
        if (duration <= 0 || duration > 24) {
            THROW_INVALID_ARGUMENT("Session duration must be between 0 and 24 hours");
        }
    }
}

auto IntelligentSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<IntelligentSequenceTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced IntelligentSequence task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(4);
    task->setTimeout(std::chrono::seconds(28800));  // 8 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void IntelligentSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("targets", "array", true, json::array(),
                            "Array of target objects with coordinates and parameters");
    task.addParamDefinition("session_duration_hours", "double", false, 8.0,
                            "Maximum session duration in hours");
    task.addParamDefinition("min_altitude", "double", false, 30.0,
                            "Minimum target altitude in degrees");
    task.addParamDefinition("weather_monitoring", "bool", false, true,
                            "Enable weather condition monitoring");
    task.addParamDefinition("cloud_cover_limit", "double", false, 30.0,
                            "Maximum acceptable cloud cover percentage");
    task.addParamDefinition("wind_speed_limit", "double", false, 15.0,
                            "Maximum acceptable wind speed in km/h");
    task.addParamDefinition("auto_meridian_flip", "bool", false, true,
                            "Enable automatic meridian flip");
    task.addParamDefinition("dynamic_target_selection", "bool", false, true,
                            "Enable dynamic target selection based on conditions");
}

}  // namespace lithium::task::task
