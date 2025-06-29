#ifndef LITHIUM_TASK_GUIDE_WORKFLOWS_HPP
#define LITHIUM_TASK_GUIDE_WORKFLOWS_HPP

#include "client/phd2/types.h"
#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Complete guide setup workflow task.
 * Performs a complete setup sequence: connect -> find star -> calibrate ->
 * start guiding.
 */
class CompleteGuideSetupTask : public Task {
public:
    CompleteGuideSetupTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performCompleteSetup(const json& params);
    bool waitForState(phd2::AppStateType target_state,
                      int timeout_seconds = 30);
};

/**
 * @brief Guided session workflow task.
 * Manages a complete guided imaging session with automatic recovery.
 */
class GuidedSessionTask : public Task {
public:
    GuidedSessionTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void runGuidedSession(const json& params);
    void performRecovery();
    bool monitorGuiding(int check_interval_seconds);
};

/**
 * @brief Meridian flip workflow task.
 * Handles complete meridian flip sequence: stop -> flip -> recalibrate ->
 * resume.
 */
class MeridianFlipWorkflowTask : public Task {
public:
    MeridianFlipWorkflowTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performMeridianFlip(const json& params);
    void savePreFlipState();
    void restorePostFlipState();

    json pre_flip_state_;
};

/**
 * @brief Adaptive dithering workflow task.
 * Intelligent dithering based on current conditions and history.
 */
class AdaptiveDitheringTask : public Task {
public:
    AdaptiveDitheringTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performAdaptiveDithering(const json& params);
    double calculateOptimalDitherAmount();
    void updateDitherHistory(double amount, bool success);

    std::vector<std::pair<double, bool>> dither_history_;
};

/**
 * @brief Guide performance monitoring task.
 * Continuously monitors and reports guide performance metrics.
 */
class GuidePerformanceMonitorTask : public Task {
public:
    GuidePerformanceMonitorTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void monitorPerformance(const json& params);
    void collectMetrics();
    void analyzePerformance();
    void generateReport();

    struct PerformanceMetrics {
        double rms_ra;
        double rms_dec;
        double total_rms;
        int correction_count;
        double max_error;
        std::chrono::steady_clock::time_point start_time;
    };

    PerformanceMetrics current_metrics_;
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_WORKFLOWS_HPP
