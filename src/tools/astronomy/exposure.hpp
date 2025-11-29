/**
 * @file exposure.hpp
 * @brief Exposure planning types for astronomical imaging.
 *
 * This file defines types for planning and tracking exposure sequences,
 * including filter settings, exposure times, and progress tracking.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_ASTRONOMY_EXPOSURE_HPP
#define LITHIUM_TOOLS_ASTRONOMY_EXPOSURE_HPP

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "atom/type/json.hpp"
#include "constants.hpp"

namespace lithium::tools::astronomy {

using json = nlohmann::json;

// ============================================================================
// Exposure Plan
// ============================================================================

/**
 * @struct ExposurePlan
 * @brief Single exposure plan entry for a filter.
 *
 * Defines the parameters for a series of exposures with a specific filter,
 * including exposure time, count, binning, and camera settings.
 */
struct ExposurePlan {
    std::string filterName;    ///< Filter name (e.g., "L", "R", "Ha")
    double exposureTime{0.0};  ///< Exposure time in seconds
    int count{1};              ///< Number of exposures planned
    int completedCount{0};     ///< Number of completed exposures
    int binning{1};            ///< Binning (1x1, 2x2, etc.)
    int gain{-1};              ///< Camera gain (-1 = use default)
    int offset{-1};            ///< Camera offset (-1 = use default)
    bool ditherEnabled{true};  ///< Enable dithering between exposures
    int ditherEvery{1};        ///< Dither every N exposures

    // ========================================================================
    // Constructors
    // ========================================================================

    ExposurePlan() = default;

    ExposurePlan(const std::string& filter, double expTime, int cnt,
                 int bin = 1, int g = -1, int off = -1)
        : filterName(filter),
          exposureTime(expTime),
          count(cnt),
          binning(bin),
          gain(g),
          offset(off) {}

    // ========================================================================
    // Progress Tracking
    // ========================================================================

    /**
     * @brief Get remaining exposures.
     * @return Number of exposures still to be taken.
     */
    [[nodiscard]] int remaining() const noexcept {
        return std::max(0, count - completedCount);
    }

    /**
     * @brief Get progress percentage.
     * @return Progress as percentage (0-100).
     */
    [[nodiscard]] double progress() const noexcept {
        if (count == 0)
            return 100.0;
        return (static_cast<double>(completedCount) / count) * 100.0;
    }

    /**
     * @brief Check if plan is complete.
     * @return true if all planned exposures are completed.
     */
    [[nodiscard]] bool isComplete() const noexcept {
        return completedCount >= count;
    }

    // ========================================================================
    // Time Calculations
    // ========================================================================

    /**
     * @brief Get total exposure time for this plan.
     * @return Total planned exposure time in seconds.
     */
    [[nodiscard]] double totalExposureTime() const noexcept {
        return exposureTime * count;
    }

    /**
     * @brief Get remaining exposure time.
     * @return Remaining exposure time in seconds.
     */
    [[nodiscard]] double remainingExposureTime() const noexcept {
        return exposureTime * remaining();
    }

    /**
     * @brief Get completed exposure time.
     * @return Completed exposure time in seconds.
     */
    [[nodiscard]] double completedExposureTime() const noexcept {
        return exposureTime * completedCount;
    }

    // ========================================================================
    // Operations
    // ========================================================================

    /**
     * @brief Record a completed exposure.
     * @return true if exposure was recorded, false if already complete.
     */
    bool recordExposure() noexcept {
        if (isComplete())
            return false;
        ++completedCount;
        return true;
    }

    /**
     * @brief Reset progress to zero.
     */
    void reset() noexcept { completedCount = 0; }

    /**
     * @brief Check if dithering should occur after this exposure.
     * @param exposureNumber Current exposure number (1-based).
     * @return true if dithering should occur.
     */
    [[nodiscard]] bool shouldDither(int exposureNumber) const noexcept {
        if (!ditherEnabled)
            return false;
        if (ditherEvery <= 0)
            return false;
        return (exposureNumber % ditherEvery) == 0;
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        return {{"filterName", filterName},
                {"exposureTime", exposureTime},
                {"count", count},
                {"completedCount", completedCount},
                {"binning", binning},
                {"gain", gain},
                {"offset", offset},
                {"ditherEnabled", ditherEnabled},
                {"ditherEvery", ditherEvery}};
    }

    [[nodiscard]] static ExposurePlan fromJson(const json& j) {
        ExposurePlan p;
        p.filterName = j.value("filterName", "");
        p.exposureTime = j.value("exposureTime", 0.0);
        p.count = j.value("count", 1);
        p.completedCount = j.value("completedCount", 0);
        p.binning = j.value("binning", 1);
        p.gain = j.value("gain", -1);
        p.offset = j.value("offset", -1);
        p.ditherEnabled = j.value("ditherEnabled", true);
        p.ditherEvery = j.value("ditherEvery", 1);
        return p;
    }

    // ========================================================================
    // Operators
    // ========================================================================

    bool operator==(const ExposurePlan& other) const noexcept {
        return filterName == other.filterName &&
               std::abs(exposureTime - other.exposureTime) < EPSILON &&
               count == other.count && binning == other.binning &&
               gain == other.gain && offset == other.offset;
    }

    bool operator!=(const ExposurePlan& other) const noexcept {
        return !(*this == other);
    }
};

// ============================================================================
// Exposure Plan Collection
// ============================================================================

/**
 * @class ExposurePlanCollection
 * @brief Collection of exposure plans with aggregate operations.
 *
 * Manages multiple exposure plans and provides aggregate statistics
 * and operations across all plans.
 */
class ExposurePlanCollection {
public:
    // ========================================================================
    // Constructors
    // ========================================================================

    ExposurePlanCollection() = default;

    explicit ExposurePlanCollection(std::vector<ExposurePlan> plans)
        : plans_(std::move(plans)) {}

    // ========================================================================
    // Plan Management
    // ========================================================================

    /**
     * @brief Add an exposure plan.
     * @param plan The plan to add.
     */
    void addPlan(const ExposurePlan& plan) { plans_.push_back(plan); }

    /**
     * @brief Add an exposure plan (move version).
     * @param plan The plan to add.
     */
    void addPlan(ExposurePlan&& plan) { plans_.push_back(std::move(plan)); }

    /**
     * @brief Remove a plan by filter name.
     * @param filterName Filter name to remove.
     * @return true if plan was found and removed.
     */
    bool removePlan(const std::string& filterName) {
        auto it = std::find_if(plans_.begin(), plans_.end(),
                               [&filterName](const ExposurePlan& p) {
                                   return p.filterName == filterName;
                               });
        if (it != plans_.end()) {
            plans_.erase(it);
            return true;
        }
        return false;
    }

    /**
     * @brief Get plan by filter name.
     * @param filterName Filter name to find.
     * @return Pointer to plan, or nullptr if not found.
     */
    [[nodiscard]] ExposurePlan* getPlan(const std::string& filterName) {
        auto it = std::find_if(plans_.begin(), plans_.end(),
                               [&filterName](const ExposurePlan& p) {
                                   return p.filterName == filterName;
                               });
        return it != plans_.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Get plan by filter name (const version).
     * @param filterName Filter name to find.
     * @return Pointer to plan, or nullptr if not found.
     */
    [[nodiscard]] const ExposurePlan* getPlan(
        const std::string& filterName) const {
        auto it = std::find_if(plans_.begin(), plans_.end(),
                               [&filterName](const ExposurePlan& p) {
                                   return p.filterName == filterName;
                               });
        return it != plans_.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Get all plans.
     * @return Reference to vector of plans.
     */
    [[nodiscard]] const std::vector<ExposurePlan>& plans() const noexcept {
        return plans_;
    }

    /**
     * @brief Get mutable reference to all plans.
     * @return Mutable reference to vector of plans.
     */
    [[nodiscard]] std::vector<ExposurePlan>& plans() noexcept { return plans_; }

    /**
     * @brief Get number of plans.
     * @return Number of exposure plans.
     */
    [[nodiscard]] size_t size() const noexcept { return plans_.size(); }

    /**
     * @brief Check if collection is empty.
     * @return true if no plans.
     */
    [[nodiscard]] bool empty() const noexcept { return plans_.empty(); }

    /**
     * @brief Clear all plans.
     */
    void clear() noexcept { plans_.clear(); }

    // ========================================================================
    // Aggregate Statistics
    // ========================================================================

    /**
     * @brief Get total planned exposure time across all plans.
     * @return Total exposure time in seconds.
     */
    [[nodiscard]] double totalPlannedExposureTime() const noexcept {
        double total = 0.0;
        for (const auto& plan : plans_) {
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
        for (const auto& plan : plans_) {
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
        for (const auto& plan : plans_) {
            totalPlanned += plan.count;
            totalCompleted += plan.completedCount;
        }
        if (totalPlanned == 0)
            return 100.0;
        return (totalCompleted / totalPlanned) * 100.0;
    }

    /**
     * @brief Check if all exposure plans are complete.
     * @return true if all plans are complete.
     */
    [[nodiscard]] bool isComplete() const noexcept {
        if (plans_.empty())
            return true;
        for (const auto& plan : plans_) {
            if (!plan.isComplete())
                return false;
        }
        return true;
    }

    /**
     * @brief Get total number of planned exposures.
     * @return Total exposure count.
     */
    [[nodiscard]] int totalExposureCount() const noexcept {
        int total = 0;
        for (const auto& plan : plans_) {
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
        for (const auto& plan : plans_) {
            total += plan.completedCount;
        }
        return total;
    }

    // ========================================================================
    // Operations
    // ========================================================================

    /**
     * @brief Reset all plans to zero progress.
     */
    void resetAll() noexcept {
        for (auto& plan : plans_) {
            plan.reset();
        }
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        json arr = json::array();
        for (const auto& plan : plans_) {
            arr.push_back(plan.toJson());
        }
        return arr;
    }

    [[nodiscard]] static ExposurePlanCollection fromJson(const json& j) {
        ExposurePlanCollection collection;
        if (j.is_array()) {
            for (const auto& item : j) {
                collection.addPlan(ExposurePlan::fromJson(item));
            }
        }
        return collection;
    }

private:
    std::vector<ExposurePlan> plans_;
};

}  // namespace lithium::tools::astronomy

#endif  // LITHIUM_TOOLS_ASTRONOMY_EXPOSURE_HPP
