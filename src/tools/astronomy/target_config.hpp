/**
 * @file target_config.hpp
 * @brief Complete configuration for astronomical targets.
 *
 * This file defines the TargetConfig structure which combines all
 * configuration aspects for an astronomical observation target.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_ASTRONOMY_TARGET_CONFIG_HPP
#define LITHIUM_TOOLS_ASTRONOMY_TARGET_CONFIG_HPP

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "atom/type/json.hpp"
#include "constraints.hpp"
#include "coordinates.hpp"
#include "exposure.hpp"

namespace lithium::tools::astronomy {

using json = nlohmann::json;

// ============================================================================
// Target Configuration
// ============================================================================

/**
 * @struct TargetConfig
 * @brief Complete configuration for an astronomical target.
 *
 * Combines all aspects of target configuration including identification,
 * coordinates, constraints, exposure plans, and acquisition settings.
 */
struct TargetConfig {
    // ========================================================================
    // Basic Information
    // ========================================================================

    std::string catalogName;        ///< Catalog name (e.g., "M31", "NGC 7000")
    std::string commonName;         ///< Common name (e.g., "Andromeda Galaxy")
    std::string objectType;         ///< Object type (galaxy, nebula, cluster...)

    // ========================================================================
    // Coordinates and Orientation
    // ========================================================================

    Coordinates coordinates;        ///< Target coordinates
    double rotation{0.0};           ///< Camera rotation angle (degrees)

    // ========================================================================
    // Observation Constraints
    // ========================================================================

    AltitudeConstraints altConstraints;  ///< Altitude constraints
    double minMoonSeparation{30.0};      ///< Minimum moon separation (degrees)
    bool avoidMeridianFlip{false};       ///< Avoid meridian flip during exposure

    // ========================================================================
    // Meridian Flip Settings
    // ========================================================================

    double meridianFlipOffset{0.0};      ///< Minutes past meridian before flip
    bool autoMeridianFlip{true};         ///< Allow automatic meridian flip

    // ========================================================================
    // Exposure Plans
    // ========================================================================

    std::vector<ExposurePlan> exposurePlans;  ///< List of exposure plans

    // ========================================================================
    // Timing Constraints
    // ========================================================================

    std::chrono::system_clock::time_point startTime;  ///< Earliest start time
    std::chrono::system_clock::time_point endTime;    ///< Latest end time
    bool useTimeConstraints{false};    ///< Whether to use time constraints

    // ========================================================================
    // Priority
    // ========================================================================

    int priority{5};                   ///< Target priority (1-10, higher=more important)

    // ========================================================================
    // Acquisition Settings
    // ========================================================================

    bool slewRequired{true};           ///< Whether slew is needed
    bool centeringRequired{true};      ///< Whether plate solve centering is needed
    bool guidingRequired{true};        ///< Whether guiding is needed
    bool focusRequired{true};          ///< Whether focus check is needed

    // ========================================================================
    // Focus Settings
    // ========================================================================

    bool autoFocusOnStart{true};          ///< Auto focus when target starts
    bool autoFocusOnFilterChange{false};  ///< Auto focus on filter change
    double focusTempThreshold{1.0};       ///< Temperature change threshold for refocus

    // ========================================================================
    // Constructors
    // ========================================================================

    TargetConfig() = default;

    // ========================================================================
    // Exposure Plan Statistics
    // ========================================================================

    /**
     * @brief Get total planned exposure time across all plans.
     * @return Total exposure time in seconds.
     */
    [[nodiscard]] double totalPlannedExposureTime() const noexcept {
        double total = 0.0;
        for (const auto& plan : exposurePlans) {
            total += plan.totalExposureTime();
        }
        return total;
    }

    /**
     * @brief Get total remaining exposure time.
     * @return Remaining exposure time in seconds.
     */
    [[nodiscard]] double totalRemainingExposureTime() const noexcept {
        double total = 0.0;
        for (const auto& plan : exposurePlans) {
            total += plan.remainingExposureTime();
        }
        return total;
    }

    /**
     * @brief Get overall progress percentage.
     * @return Progress as percentage (0-100).
     */
    [[nodiscard]] double overallProgress() const noexcept {
        double totalPlanned = 0.0;
        double totalCompleted = 0.0;
        for (const auto& plan : exposurePlans) {
            totalPlanned += plan.count;
            totalCompleted += plan.completedCount;
        }
        if (totalPlanned == 0) return 100.0;
        return (totalCompleted / totalPlanned) * 100.0;
    }

    /**
     * @brief Check if all exposure plans are complete.
     * @return true if all plans are complete.
     */
    [[nodiscard]] bool isComplete() const noexcept {
        if (exposurePlans.empty()) return true;
        for (const auto& plan : exposurePlans) {
            if (!plan.isComplete()) return false;
        }
        return true;
    }

    /**
     * @brief Get total number of planned exposures.
     * @return Total exposure count.
     */
    [[nodiscard]] int totalExposureCount() const noexcept {
        int total = 0;
        for (const auto& plan : exposurePlans) {
            total += plan.count;
        }
        return total;
    }

    /**
     * @brief Get total number of completed exposures.
     * @return Completed exposure count.
     */
    [[nodiscard]] int totalCompletedCount() const noexcept {
        int total = 0;
        for (const auto& plan : exposurePlans) {
            total += plan.completedCount;
        }
        return total;
    }

    // ========================================================================
    // Exposure Plan Management
    // ========================================================================

    /**
     * @brief Add an exposure plan.
     * @param plan The plan to add.
     */
    void addExposurePlan(const ExposurePlan& plan) {
        exposurePlans.push_back(plan);
    }

    /**
     * @brief Remove an exposure plan by filter name.
     * @param filterName Filter name to remove.
     * @return true if plan was found and removed.
     */
    bool removeExposurePlan(const std::string& filterName) {
        auto it = std::find_if(exposurePlans.begin(), exposurePlans.end(),
            [&filterName](const ExposurePlan& p) {
                return p.filterName == filterName;
            });
        if (it != exposurePlans.end()) {
            exposurePlans.erase(it);
            return true;
        }
        return false;
    }

    /**
     * @brief Get exposure plan by filter name.
     * @param filterName Filter name to find.
     * @return Pointer to plan, or nullptr if not found.
     */
    [[nodiscard]] ExposurePlan* getExposurePlan(const std::string& filterName) {
        auto it = std::find_if(exposurePlans.begin(), exposurePlans.end(),
            [&filterName](const ExposurePlan& p) {
                return p.filterName == filterName;
            });
        return it != exposurePlans.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Reset all exposure plan progress.
     */
    void resetExposurePlans() noexcept {
        for (auto& plan : exposurePlans) {
            plan.reset();
        }
    }

    // ========================================================================
    // Validation
    // ========================================================================

    /**
     * @brief Check if configuration is valid.
     * @return true if all required fields are valid.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return coordinates.isValid() &&
               altConstraints.areConstraintsValid() &&
               priority >= 1 && priority <= 10;
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        json plansJson = json::array();
        for (const auto& plan : exposurePlans) {
            plansJson.push_back(plan.toJson());
        }

        return {
            {"catalogName", catalogName},
            {"commonName", commonName},
            {"objectType", objectType},
            {"coordinates", coordinates.toJson()},
            {"rotation", rotation},
            {"altConstraints", altConstraints.toJson()},
            {"minMoonSeparation", minMoonSeparation},
            {"avoidMeridianFlip", avoidMeridianFlip},
            {"meridianFlipOffset", meridianFlipOffset},
            {"autoMeridianFlip", autoMeridianFlip},
            {"exposurePlans", plansJson},
            {"startTime", std::chrono::system_clock::to_time_t(startTime)},
            {"endTime", std::chrono::system_clock::to_time_t(endTime)},
            {"useTimeConstraints", useTimeConstraints},
            {"priority", priority},
            {"slewRequired", slewRequired},
            {"centeringRequired", centeringRequired},
            {"guidingRequired", guidingRequired},
            {"focusRequired", focusRequired},
            {"autoFocusOnStart", autoFocusOnStart},
            {"autoFocusOnFilterChange", autoFocusOnFilterChange},
            {"focusTempThreshold", focusTempThreshold}
        };
    }

    [[nodiscard]] static TargetConfig fromJson(const json& j) {
        TargetConfig cfg;
        cfg.catalogName = j.value("catalogName", "");
        cfg.commonName = j.value("commonName", "");
        cfg.objectType = j.value("objectType", "");

        if (j.contains("coordinates")) {
            cfg.coordinates = Coordinates::fromJson(j["coordinates"]);
        }

        cfg.rotation = j.value("rotation", 0.0);

        if (j.contains("altConstraints")) {
            cfg.altConstraints = AltitudeConstraints::fromJson(j["altConstraints"]);
        }

        cfg.minMoonSeparation = j.value("minMoonSeparation", 30.0);
        cfg.avoidMeridianFlip = j.value("avoidMeridianFlip", false);
        cfg.meridianFlipOffset = j.value("meridianFlipOffset", 0.0);
        cfg.autoMeridianFlip = j.value("autoMeridianFlip", true);

        if (j.contains("exposurePlans") && j["exposurePlans"].is_array()) {
            for (const auto& p : j["exposurePlans"]) {
                cfg.exposurePlans.push_back(ExposurePlan::fromJson(p));
            }
        }

        cfg.startTime = std::chrono::system_clock::from_time_t(
            j.value("startTime", static_cast<std::time_t>(0)));
        cfg.endTime = std::chrono::system_clock::from_time_t(
            j.value("endTime", static_cast<std::time_t>(0)));
        cfg.useTimeConstraints = j.value("useTimeConstraints", false);
        cfg.priority = j.value("priority", 5);
        cfg.slewRequired = j.value("slewRequired", true);
        cfg.centeringRequired = j.value("centeringRequired", true);
        cfg.guidingRequired = j.value("guidingRequired", true);
        cfg.focusRequired = j.value("focusRequired", true);
        cfg.autoFocusOnStart = j.value("autoFocusOnStart", true);
        cfg.autoFocusOnFilterChange = j.value("autoFocusOnFilterChange", false);
        cfg.focusTempThreshold = j.value("focusTempThreshold", 1.0);

        return cfg;
    }
};

}  // namespace lithium::tools::astronomy

#endif  // LITHIUM_TOOLS_ASTRONOMY_TARGET_CONFIG_HPP
