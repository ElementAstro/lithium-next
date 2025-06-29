#pragma once
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <unordered_map>

#include "hardware_interface.hpp"
#include "position_manager.hpp"

namespace lithium::device::asi::filterwheel {

/**
 * @brief Results from a calibration test
 */
struct CalibrationResult {
    bool success;
    int position;
    std::chrono::milliseconds move_time;
    double position_accuracy;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp;

    CalibrationResult(int pos = 0)
        : success(false), position(pos), move_time(0), position_accuracy(0.0)
        , timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Complete calibration report
 */
struct CalibrationReport {
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::milliseconds total_duration;
    int total_positions_tested;
    int successful_positions;
    int failed_positions;
    std::vector<CalibrationResult> position_results;
    std::vector<std::string> general_errors;
    bool overall_success;
    double average_move_time;
    double max_move_time;
    double min_move_time;

    CalibrationReport()
        : total_duration(0), total_positions_tested(0), successful_positions(0)
        , failed_positions(0), overall_success(false), average_move_time(0.0)
        , max_move_time(0.0), min_move_time(0.0) {}
};

/**
 * @brief Self-test configuration
 */
struct SelfTestConfig {
    bool test_all_positions;
    std::vector<int> specific_positions;
    int repetitions_per_position;
    int move_timeout_ms;
    int settle_time_ms;
    bool test_movement_accuracy;
    bool test_response_time;

    SelfTestConfig()
        : test_all_positions(true), repetitions_per_position(3)
        , move_timeout_ms(30000), settle_time_ms(1000)
        , test_movement_accuracy(true), test_response_time(true) {}
};

/**
 * @brief Callback for calibration progress updates
 */
using CalibrationProgressCallback = std::function<void(int current_position, int total_positions,
                                                      const std::string& status)>;

/**
 * @brief Manages calibration, self-testing, and diagnostic functions for the filterwheel
 */
class CalibrationSystem {
public:
    CalibrationSystem(std::shared_ptr<components::HardwareInterface> hw,
                     std::shared_ptr<components::PositionManager> pos_mgr);
    ~CalibrationSystem();

    // Full calibration
    bool performFullCalibration();
    bool performQuickCalibration();
    bool performCustomCalibration(const std::vector<int>& positions);
    CalibrationReport getLastCalibrationReport() const;

    // Self-testing
    bool performSelfTest(const SelfTestConfig& config = SelfTestConfig{});
    bool performQuickSelfTest();
    bool testPosition(int position, int repetitions = 1);
    std::vector<CalibrationResult> getLastSelfTestResults() const;

    // Individual tests
    bool testMovementAccuracy(int position, double tolerance = 0.1);
    bool testResponseTime(int position, std::chrono::milliseconds max_time = std::chrono::milliseconds(10000));
    bool testMovementReliability(int from_position, int to_position, int repetitions = 5);
    bool testFullRotation();

    // Diagnostic functions
    bool diagnoseConnectivity();
    bool diagnoseMovementSystem();
    bool diagnosePositionSensors();
    std::vector<std::string> runAllDiagnostics();

    // Calibration management
    bool saveCalibrationData(const std::string& filepath = "");
    bool loadCalibrationData(const std::string& filepath = "");
    bool hasValidCalibration() const;
    std::chrono::system_clock::time_point getLastCalibrationTime() const;

    // Configuration
    void setMoveTimeout(std::chrono::milliseconds timeout);
    void setSettleTime(std::chrono::milliseconds settle_time);
    void setPositionTolerance(double tolerance);
    void setProgressCallback(CalibrationProgressCallback callback);
    void clearProgressCallback();

    // Status and reporting
    bool isCalibrationInProgress() const;
    double getCalibrationProgress() const; // 0.0 to 1.0
    std::string getCalibrationStatus() const;
    std::string generateCalibrationReport() const;
    std::string generateDiagnosticReport() const;

    // Validation
    bool validateConfiguration() const;
    std::vector<std::string> getConfigurationErrors() const;

private:
    std::shared_ptr<components::HardwareInterface> hardware_;
    std::shared_ptr<components::PositionManager> position_manager_;

    // Configuration
    std::chrono::milliseconds move_timeout_;
    std::chrono::milliseconds settle_time_;
    double position_tolerance_;

    // Calibration state
    bool calibration_in_progress_;
    int current_calibration_step_;
    int total_calibration_steps_;
    std::string calibration_status_;
    CalibrationReport last_calibration_report_;
    std::vector<CalibrationResult> last_self_test_results_;

    // Callback
    CalibrationProgressCallback progress_callback_;

    // Calibration data
    std::unordered_map<int, double> position_offsets_;
    std::chrono::system_clock::time_point last_calibration_time_;

    // Helper methods
    CalibrationResult performPositionTest(int position, int repetition = 1);
    bool moveToPositionAndValidate(int position);
    double measurePositionAccuracy(int expected_position);
    std::chrono::milliseconds measureMoveTime(int from_position, int to_position);
    void updateProgress(int current, int total, const std::string& status);
    void resetCalibrationState();
    bool isValidPosition(int position) const;
    std::string getDefaultCalibrationPath() const;

    // Diagnostic helpers
    bool testBasicCommunication();
    bool testMovementRange();
    bool testPositionConsistency();
    bool testMotorFunction();

    // Report generation
    void generateCalibrationSummary(CalibrationReport& report);
    std::string formatCalibrationResult(const CalibrationResult& result) const;
    std::string formatDuration(std::chrono::milliseconds duration) const;
};

} // namespace lithium::device::asi::filterwheel
