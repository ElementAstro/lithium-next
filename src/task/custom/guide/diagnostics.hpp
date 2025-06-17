#ifndef LITHIUM_TASK_GUIDE_DIAGNOSTICS_HPP
#define LITHIUM_TASK_GUIDE_DIAGNOSTICS_HPP

#include "task/task.hpp"
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Comprehensive guide system diagnostics task.
 * Performs deep analysis of guiding performance and issues.
 */
class GuideDiagnosticsTask : public Task {
public:
    GuideDiagnosticsTask();
    
    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performDiagnostics(const json& params);
    void analyzeCalibrationQuality();
    void detectPeriodicError();
    void analyzeGuideStarQuality();
    void checkMountPerformance();
    void generateDiagnosticReport();
    
    struct DiagnosticResults {
        bool calibration_valid;
        double calibration_angle_error;
        bool periodic_error_detected;
        double pe_amplitude;
        double star_snr;
        bool mount_backlash_detected;
        std::vector<std::string> recommendations;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };
    
    DiagnosticResults results_;
};

/**
 * @brief Real-time performance analysis task.
 * Continuously analyzes guiding performance and provides feedback.
 */
class PerformanceAnalysisTask : public Task {
public:
    PerformanceAnalysisTask();
    
    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void analyzePerformance(const json& params);
    void collectGuideData(int duration_seconds);
    void calculateStatistics();
    void identifyTrends();
    void generatePerformanceReport();
    
    struct GuideDataPoint {
        std::chrono::steady_clock::time_point timestamp;
        double ra_error;
        double dec_error;
        double star_brightness;
        bool correction_applied;
    };
    
    std::vector<GuideDataPoint> guide_data_;
    
    struct PerformanceStats {
        double rms_ra;
        double rms_dec;
        double rms_total;
        double max_error;
        double correction_frequency;
        double drift_rate_ra;
        double drift_rate_dec;
    };
    
    PerformanceStats stats_;
};

/**
 * @brief Automated troubleshooting task.
 * Automatically diagnoses and attempts to fix common guiding issues.
 */
class AutoTroubleshootTask : public Task {
public:
    AutoTroubleshootTask();
    
    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performTroubleshooting(const json& params);
    void diagnoseIssue();
    void attemptAutomaticFix();
    void provideTroubleshootingSteps();
    
    enum class IssueType {
        NoIssue,
        PoorCalibration,
        WeakStar,
        MountIssues,
        AtmosphericTurbulence,
        ConfigurationProblem,
        HardwareFailure,
        Unknown
    };
    
    IssueType detected_issue_;
    std::vector<std::string> troubleshooting_steps_;
};

/**
 * @brief Guide log analysis task.
 * Analyzes PHD2 log files for patterns and issues.
 */
class GuideLogAnalysisTask : public Task {
public:
    GuideLogAnalysisTask();
    
    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void analyzeLogFiles(const json& params);
    void parseLogFile(const std::string& log_path);
    void extractGuideData();
    void identifyPatterns();
    void generateLogReport();
    
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        std::string event_type;
        std::string message;
        double ra_error;
        double dec_error;
        double ra_correction;
        double dec_correction;
    };
    
    std::vector<LogEntry> log_entries_;
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_DIAGNOSTICS_HPP
