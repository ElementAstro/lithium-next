#include "calibration_system.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include "hardware_interface.hpp"
#include "position_manager.hpp"

namespace lithium::device::asi::filterwheel {

lithium::device::asi::filterwheel::CalibrationSystem::CalibrationSystem(
    std::shared_ptr<
        ::lithium::device::asi::filterwheel::components::HardwareInterface>
        hw,
    std::shared_ptr<
        ::lithium::device::asi::filterwheel::components::PositionManager>
        pos_mgr)
    : hardware_(std::move(hw)),
      position_manager_(std::move(pos_mgr)),
      move_timeout_(std::chrono::milliseconds(30000)),
      settle_time_(std::chrono::milliseconds(1000)),
      position_tolerance_(0.1),
      calibration_in_progress_(false),
      current_calibration_step_(0),
      total_calibration_steps_(0) {
    spdlog::info("CalibrationSystem initialized");
}

lithium::device::asi::filterwheel::CalibrationSystem::~CalibrationSystem() {
    spdlog::info("CalibrationSystem destroyed");
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    performFullCalibration() {
    if (calibration_in_progress_) {
        spdlog::error("Calibration already in progress");
        return false;
    }

    if (!hardware_ || !position_manager_) {
        spdlog::error("Hardware interface or position manager not available");
        return false;
    }

    spdlog::info("Starting full calibration");
    resetCalibrationState();

    calibration_in_progress_ = true;
    calibration_status_ = "Starting full calibration";

    auto start_time = std::chrono::steady_clock::now();
    last_calibration_report_.start_time = std::chrono::system_clock::now();

    // Get available positions
    int slot_count = hardware_->getFilterCount();
    if (slot_count <= 0) {
        spdlog::error("Invalid slot count: {}", slot_count);
        calibration_in_progress_ = false;
        return false;
    }

    total_calibration_steps_ = slot_count;
    last_calibration_report_.total_positions_tested = slot_count;

    bool overall_success = true;

    try {
        // Test each position
        for (int pos = 0; pos < slot_count; ++pos) {
            current_calibration_step_ = pos + 1;
            updateProgress(current_calibration_step_, total_calibration_steps_,
                           "Testing position " + std::to_string(pos));

            CalibrationResult result =
                performPositionTest(pos, 3);  // 3 repetitions per position
            last_calibration_report_.position_results.push_back(result);

            if (result.success) {
                last_calibration_report_.successful_positions++;
                position_offsets_[pos] = result.position_accuracy;
            } else {
                last_calibration_report_.failed_positions++;
                overall_success = false;
                spdlog::error("Calibration failed for position {}: {}", pos,
                              result.error_message);
            }
        }

        // Generate final report
        auto end_time = std::chrono::steady_clock::now();
        last_calibration_report_.end_time = std::chrono::system_clock::now();
        last_calibration_report_.total_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                  start_time);
        last_calibration_report_.overall_success = overall_success;

        generateCalibrationSummary(last_calibration_report_);

        if (overall_success) {
            last_calibration_time_ = std::chrono::system_clock::now();
            spdlog::info("Full calibration completed successfully");
            updateProgress(total_calibration_steps_, total_calibration_steps_,
                           "Calibration completed successfully");
        } else {
            spdlog::warn("Full calibration completed with errors");
            updateProgress(total_calibration_steps_, total_calibration_steps_,
                           "Calibration completed with errors");
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception during calibration: {}", e.what());
        last_calibration_report_.general_errors.push_back(
            "Exception: " + std::string(e.what()));
        overall_success = false;
    }

    calibration_in_progress_ = false;
    return overall_success;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    performQuickCalibration() {
    if (!hardware_ || !position_manager_) {
        spdlog::error("Hardware interface or position manager not available");
        return false;
    }

    spdlog::info("Starting quick calibration");

    // Test positions 0, middle, and last
    int slot_count = hardware_->getFilterCount();
    std::vector<int> test_positions = {0};

    if (slot_count > 2) {
        test_positions.push_back(slot_count / 2);
    }
    if (slot_count > 1) {
        test_positions.push_back(slot_count - 1);
    }

    return performCustomCalibration(test_positions);
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    performCustomCalibration(const std::vector<int>& positions) {
    if (calibration_in_progress_) {
        spdlog::error("Calibration already in progress");
        return false;
    }

    if (positions.empty()) {
        spdlog::error("No positions specified for custom calibration");
        return false;
    }

    spdlog::info("Starting custom calibration with {} positions",
                 positions.size());
    resetCalibrationState();

    calibration_in_progress_ = true;
    calibration_status_ = "Starting custom calibration";

    auto start_time = std::chrono::steady_clock::now();
    last_calibration_report_.start_time = std::chrono::system_clock::now();

    total_calibration_steps_ = static_cast<int>(positions.size());
    last_calibration_report_.total_positions_tested = total_calibration_steps_;

    bool overall_success = true;

    try {
        for (size_t i = 0; i < positions.size(); ++i) {
            int pos = positions[i];
            current_calibration_step_ = static_cast<int>(i) + 1;

            if (!isValidPosition(pos)) {
                spdlog::error("Invalid position: {}", pos);
                CalibrationResult result(pos);
                result.error_message = "Invalid position";
                last_calibration_report_.position_results.push_back(result);
                last_calibration_report_.failed_positions++;
                overall_success = false;
                continue;
            }

            updateProgress(current_calibration_step_, total_calibration_steps_,
                           "Testing position " + std::to_string(pos));

            CalibrationResult result =
                performPositionTest(pos, 2);  // 2 repetitions for custom
            last_calibration_report_.position_results.push_back(result);

            if (result.success) {
                last_calibration_report_.successful_positions++;
                position_offsets_[pos] = result.position_accuracy;
            } else {
                last_calibration_report_.failed_positions++;
                overall_success = false;
            }
        }

        // Generate final report
        auto end_time = std::chrono::steady_clock::now();
        last_calibration_report_.end_time = std::chrono::system_clock::now();
        last_calibration_report_.total_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                  start_time);
        last_calibration_report_.overall_success = overall_success;

        generateCalibrationSummary(last_calibration_report_);

        if (overall_success) {
            last_calibration_time_ = std::chrono::system_clock::now();
            spdlog::info("Custom calibration completed successfully");
        } else {
            spdlog::warn("Custom calibration completed with errors");
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception during custom calibration: {}", e.what());
        overall_success = false;
    }

    calibration_in_progress_ = false;
    return overall_success;
}

CalibrationReport
lithium::device::asi::filterwheel::CalibrationSystem::getLastCalibrationReport()
    const {
    return last_calibration_report_;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::performSelfTest(
    const SelfTestConfig& config) {
    if (!hardware_ || !position_manager_) {
        spdlog::error(
            "Hardware interface or position manager not available for "
            "self-test");
        return false;
    }

    spdlog::info("Starting self-test");
    last_self_test_results_.clear();

    std::vector<int> positions_to_test;

    if (config.test_all_positions) {
        int slot_count = hardware_->getFilterCount();
        for (int i = 0; i < slot_count; ++i) {
            positions_to_test.push_back(i);
        }
    } else {
        positions_to_test = config.specific_positions;
    }

    bool overall_success = true;

    for (int pos : positions_to_test) {
        if (!isValidPosition(pos)) {
            spdlog::error("Invalid position in self-test: {}", pos);
            continue;
        }

        for (int rep = 0; rep < config.repetitions_per_position; ++rep) {
            CalibrationResult result = performPositionTest(pos, rep + 1);
            last_self_test_results_.push_back(result);

            if (!result.success) {
                overall_success = false;
            }
        }
    }

    spdlog::info("Self-test completed: {}",
                 overall_success ? "PASSED" : "FAILED");
    return overall_success;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    performQuickSelfTest() {
    SelfTestConfig config;
    config.test_all_positions = false;
    config.specific_positions = {0, 1};  // Test first two positions
    config.repetitions_per_position = 1;

    return performSelfTest(config);
}

bool lithium::device::asi::filterwheel::CalibrationSystem::testPosition(
    int position, int repetitions) {
    if (!isValidPosition(position)) {
        spdlog::error("Invalid position for test: {}", position);
        return false;
    }

    spdlog::info("Testing position {} ({} repetitions)", position, repetitions);

    bool all_success = true;
    for (int rep = 0; rep < repetitions; ++rep) {
        CalibrationResult result = performPositionTest(position, rep + 1);
        if (!result.success) {
            all_success = false;
            spdlog::error("Position {} test {} failed: {}", position, rep + 1,
                          result.error_message);
        }
    }

    return all_success;
}

std::vector<CalibrationResult>
lithium::device::asi::filterwheel::CalibrationSystem::getLastSelfTestResults()
    const {
    return last_self_test_results_;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::testMovementAccuracy(
    int position, double tolerance) {
    if (!isValidPosition(position)) {
        return false;
    }

    if (!moveToPositionAndValidate(position)) {
        return false;
    }

    double accuracy = measurePositionAccuracy(position);
    return accuracy <= tolerance;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::testResponseTime(
    int position, std::chrono::milliseconds max_time) {
    if (!isValidPosition(position)) {
        return false;
    }

    int current_pos = hardware_->getCurrentPosition();
    auto start_time = std::chrono::steady_clock::now();

    if (!position_manager_->setPosition(position)) {
        return false;
    }

    if (!position_manager_->waitForMovement(
            static_cast<int>(max_time.count()))) {
        return false;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return duration <= max_time;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    testMovementReliability(int from_position, int to_position,
                            int repetitions) {
    if (!isValidPosition(from_position) || !isValidPosition(to_position)) {
        return false;
    }

    spdlog::info("Testing movement reliability: {} -> {} ({} repetitions)",
                 from_position, to_position, repetitions);

    int successful_moves = 0;

    for (int rep = 0; rep < repetitions; ++rep) {
        // Move to starting position
        if (!moveToPositionAndValidate(from_position)) {
            spdlog::error("Failed to move to starting position {}",
                          from_position);
            continue;
        }

        // Move to target position
        if (moveToPositionAndValidate(to_position)) {
            successful_moves++;
        }
    }

    double success_rate = static_cast<double>(successful_moves) /
                          static_cast<double>(repetitions);
    spdlog::info("Movement reliability test: {}/{} successful ({:.1f}%%)",
                 successful_moves, repetitions, success_rate * 100.0);

    return success_rate >= 0.9;  // Require 90% success rate
}

bool lithium::device::asi::filterwheel::CalibrationSystem::testFullRotation() {
    if (!hardware_) {
        return false;
    }

    int slot_count = hardware_->getFilterCount();
    if (slot_count <= 1) {
        return true;  // No rotation needed for single slot
    }

    spdlog::info("Testing full rotation through all {} positions", slot_count);

    // Test movement through all positions in sequence
    for (int pos = 0; pos < slot_count; ++pos) {
        if (!moveToPositionAndValidate(pos)) {
            spdlog::error("Full rotation test failed at position {}", pos);
            return false;
        }

        // Small delay between moves
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    spdlog::info("Full rotation test completed successfully");
    return true;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    diagnoseConnectivity() {
    if (!hardware_) {
        spdlog::error("Hardware interface not available");
        return false;
    }

    spdlog::info("Diagnosing connectivity");

    // Test basic connection
    if (!hardware_->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    // Test basic communication
    if (!testBasicCommunication()) {
        spdlog::error("Basic communication test failed");
        return false;
    }

    spdlog::info("Connectivity diagnosis: PASSED");
    return true;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    diagnoseMovementSystem() {
    spdlog::info("Diagnosing movement system");

    bool all_tests_passed = true;

    // Test movement range
    if (!testMovementRange()) {
        spdlog::error("Movement range test failed");
        all_tests_passed = false;
    }

    // Test motor function
    if (!testMotorFunction()) {
        spdlog::error("Motor function test failed");
        all_tests_passed = false;
    }

    // Test position consistency
    if (!testPositionConsistency()) {
        spdlog::error("Position consistency test failed");
        all_tests_passed = false;
    }

    spdlog::info("Movement system diagnosis: {}",
                 all_tests_passed ? "PASSED" : "FAILED");
    return all_tests_passed;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    diagnosePositionSensors() {
    spdlog::info("Diagnosing position sensors");

    if (!hardware_) {
        return false;
    }

    // Test position reading consistency
    int pos1 = hardware_->getCurrentPosition();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int pos2 = hardware_->getCurrentPosition();

    if (pos1 != pos2) {
        spdlog::error("Position sensor reading inconsistent: {} vs {}", pos1,
                      pos2);
        return false;
    }

    // Test position updates during movement
    int initial_pos = pos1;
    int target_pos = (initial_pos + 1) % hardware_->getFilterCount();

    if (position_manager_->setPosition(target_pos)) {
        position_manager_->waitForMovement(
            static_cast<int>(move_timeout_.count()));
        int final_pos = hardware_->getCurrentPosition();

        if (final_pos != target_pos) {
            spdlog::error(
                "Position sensor did not update correctly: expected {}, got {}",
                target_pos, final_pos);
            return false;
        }
    }

    spdlog::info("Position sensor diagnosis: PASSED");
    return true;
}

std::vector<std::string>
lithium::device::asi::filterwheel::CalibrationSystem::runAllDiagnostics() {
    std::vector<std::string> results;

    spdlog::info("Running all diagnostics");

    // Connectivity test
    if (diagnoseConnectivity()) {
        results.push_back("Connectivity: PASSED");
    } else {
        results.push_back("Connectivity: FAILED");
    }

    // Movement system test
    if (diagnoseMovementSystem()) {
        results.push_back("Movement System: PASSED");
    } else {
        results.push_back("Movement System: FAILED");
    }

    // Position sensors test
    if (diagnosePositionSensors()) {
        results.push_back("Position Sensors: PASSED");
    } else {
        results.push_back("Position Sensors: FAILED");
    }

    return results;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::saveCalibrationData(
    const std::string& filepath) {
    std::string path =
        filepath.empty() ? getDefaultCalibrationPath() : filepath;

    try {
        // Create directory if needed
        std::filesystem::path file_path(path);
        std::filesystem::create_directories(file_path.parent_path());

        std::ofstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open calibration file for writing: {}",
                          path);
            return false;
        }

        // Write calibration data
        file << "# ASI Filterwheel Calibration Data\n";
        file << "# Last calibration: "
             << std::chrono::system_clock::to_time_t(last_calibration_time_)
             << "\n\n";

        file << "[calibration]\n";
        file << "last_calibration_time="
             << std::chrono::system_clock::to_time_t(last_calibration_time_)
             << "\n";
        file << "position_tolerance=" << position_tolerance_ << "\n\n";

        file << "[position_offsets]\n";
        for (const auto& [position, offset] : position_offsets_) {
            file << "position_" << position << "=" << offset << "\n";
        }

        spdlog::info("Calibration data saved to: {}", path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save calibration data: {}", e.what());
        return false;
    }
}

bool lithium::device::asi::filterwheel::CalibrationSystem::loadCalibrationData(
    const std::string& filepath) {
    std::string path =
        filepath.empty() ? getDefaultCalibrationPath() : filepath;

    if (!std::filesystem::exists(path)) {
        spdlog::warn("Calibration file not found: {}", path);
        return false;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open calibration file for reading: {}",
                          path);
            return false;
        }

        std::string line;
        std::string current_section;

        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Check for section headers
            if (line[0] == '[' && line.back() == ']') {
                current_section = line.substr(1, line.length() - 2);
                continue;
            }

            // Parse key=value pairs
            size_t pos = line.find('=');
            if (pos == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            if (current_section == "calibration") {
                if (key == "last_calibration_time") {
                    std::time_t time_t_val = std::stol(value);
                    last_calibration_time_ =
                        std::chrono::system_clock::from_time_t(time_t_val);
                } else if (key == "position_tolerance") {
                    position_tolerance_ = std::stod(value);
                }
            } else if (current_section == "position_offsets") {
                if (key.starts_with("position_")) {
                    int position = std::stoi(key.substr(9));
                    double offset = std::stod(value);
                    position_offsets_[position] = offset;
                }
            }
        }

        spdlog::info("Calibration data loaded from: {}", path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load calibration data: {}", e.what());
        return false;
    }
}

bool lithium::device::asi::filterwheel::CalibrationSystem::hasValidCalibration()
    const {
    // Check if we have recent calibration data
    if (last_calibration_time_ == std::chrono::system_clock::time_point{}) {
        return false;
    }

    // Check if calibration is not too old (e.g., 30 days)
    auto now = std::chrono::system_clock::now();
    auto calibration_age = now - last_calibration_time_;
    auto max_age = std::chrono::hours(24 * 30);  // 30 days

    if (calibration_age > max_age) {
        return false;
    }

    // Check if we have position offset data
    return !position_offsets_.empty();
}

std::chrono::system_clock::time_point
lithium::device::asi::filterwheel::CalibrationSystem::getLastCalibrationTime()
    const {
    return last_calibration_time_;
}

void lithium::device::asi::filterwheel::CalibrationSystem::setMoveTimeout(
    std::chrono::milliseconds timeout) {
    move_timeout_ = timeout;
    spdlog::info("Set move timeout to {} ms", timeout.count());
}

void lithium::device::asi::filterwheel::CalibrationSystem::setSettleTime(
    std::chrono::milliseconds settle_time) {
    settle_time_ = settle_time;
    spdlog::info("Set settle time to {} ms", settle_time.count());
}

void lithium::device::asi::filterwheel::CalibrationSystem::setPositionTolerance(
    double tolerance) {
    position_tolerance_ = tolerance;
    spdlog::info("Set position tolerance to {:.3f}", tolerance);
}

void lithium::device::asi::filterwheel::CalibrationSystem::setProgressCallback(
    CalibrationProgressCallback callback) {
    progress_callback_ = std::move(callback);
}

void lithium::device::asi::filterwheel::CalibrationSystem::
    clearProgressCallback() {
    progress_callback_ = nullptr;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    isCalibrationInProgress() const {
    return calibration_in_progress_;
}

double
lithium::device::asi::filterwheel::CalibrationSystem::getCalibrationProgress()
    const {
    if (total_calibration_steps_ == 0) {
        return 0.0;
    }
    return static_cast<double>(current_calibration_step_) /
           static_cast<double>(total_calibration_steps_);
}

std::string
lithium::device::asi::filterwheel::CalibrationSystem::getCalibrationStatus()
    const {
    return calibration_status_;
}

std::string lithium::device::asi::filterwheel::CalibrationSystem::
    generateCalibrationReport() const {
    std::stringstream ss;

    ss << "=== Filterwheel Calibration Report ===\n";
    ss << "Start Time: "
       << std::chrono::system_clock::to_time_t(
              last_calibration_report_.start_time)
       << "\n";
    ss << "End Time: "
       << std::chrono::system_clock::to_time_t(
              last_calibration_report_.end_time)
       << "\n";
    ss << "Duration: "
       << formatDuration(last_calibration_report_.total_duration) << "\n";
    ss << "Overall Result: "
       << (last_calibration_report_.overall_success ? "SUCCESS" : "FAILED")
       << "\n\n";

    ss << "Statistics:\n";
    ss << "- Total Positions Tested: "
       << last_calibration_report_.total_positions_tested << "\n";
    ss << "- Successful: " << last_calibration_report_.successful_positions
       << "\n";
    ss << "- Failed: " << last_calibration_report_.failed_positions << "\n";
    ss << "- Average Move Time: " << std::fixed << std::setprecision(1)
       << last_calibration_report_.average_move_time << " ms\n";
    ss << "- Min Move Time: " << std::fixed << std::setprecision(1)
       << last_calibration_report_.min_move_time << " ms\n";
    ss << "- Max Move Time: " << std::fixed << std::setprecision(1)
       << last_calibration_report_.max_move_time << " ms\n\n";

    ss << "Position Results:\n";
    for (const auto& result : last_calibration_report_.position_results) {
        ss << formatCalibrationResult(result) << "\n";
    }

    if (!last_calibration_report_.general_errors.empty()) {
        ss << "\nGeneral Errors:\n";
        for (const auto& error : last_calibration_report_.general_errors) {
            ss << "- " << error << "\n";
        }
    }

    return ss.str();
}

std::string
lithium::device::asi::filterwheel::CalibrationSystem::generateDiagnosticReport()
    const {
    std::stringstream ss;

    ss << "=== Filterwheel Diagnostic Report ===\n";
    ss << "Generated: "
       << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
       << "\n\n";

    auto results = const_cast<CalibrationSystem*>(this)->runAllDiagnostics();
    for (const auto& result : results) {
        ss << result << "\n";
    }

    return ss.str();
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    validateConfiguration() const {
    if (!hardware_ || !position_manager_) {
        return false;
    }

    if (move_timeout_ < std::chrono::milliseconds(1000)) {
        return false;
    }

    if (position_tolerance_ < 0.0 || position_tolerance_ > 1.0) {
        return false;
    }

    return true;
}

std::vector<std::string>
lithium::device::asi::filterwheel::CalibrationSystem::getConfigurationErrors()
    const {
    std::vector<std::string> errors;

    if (!hardware_) {
        errors.push_back("Hardware interface not available");
    }

    if (!position_manager_) {
        errors.push_back("Position manager not available");
    }

    if (move_timeout_ < std::chrono::milliseconds(1000)) {
        errors.push_back("Move timeout too short (minimum 1000 ms)");
    }

    if (position_tolerance_ < 0.0 || position_tolerance_ > 1.0) {
        errors.push_back("Position tolerance out of range (0.0 to 1.0)");
    }

    return errors;
}

// Private helper methods

CalibrationResult
lithium::device::asi::filterwheel::CalibrationSystem::performPositionTest(
    int position, int repetition) {
    CalibrationResult result(position);
    result.timestamp = std::chrono::system_clock::now();

    spdlog::info("Performing position test: position {}, repetition {}",
                 position, repetition);

    auto start_time = std::chrono::steady_clock::now();

    try {
        if (!moveToPositionAndValidate(position)) {
            result.error_message = "Failed to move to position";
            return result;
        }

        // Measure move time
        auto end_time = std::chrono::steady_clock::now();
        result.move_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                  start_time);

        // Settle time
        std::this_thread::sleep_for(settle_time_);

        // Measure position accuracy
        result.position_accuracy = measurePositionAccuracy(position);

        // Check if within tolerance
        if (result.position_accuracy <= position_tolerance_) {
            result.success = true;
        } else {
            result.error_message = "Position accuracy out of tolerance: " +
                                   std::to_string(result.position_accuracy);
        }

    } catch (const std::exception& e) {
        result.error_message = "Exception: " + std::string(e.what());
    }

    return result;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    moveToPositionAndValidate(int position) {
    if (!position_manager_) {
        return false;
    }

    if (!position_manager_->setPosition(position)) {
        return false;
    }

    if (!position_manager_->waitForMovement(
            static_cast<int>(move_timeout_.count()))) {
        return false;
    }

    // Verify we're at the correct position
    int actual_position = hardware_->getCurrentPosition();
    return actual_position == position;
}

double
lithium::device::asi::filterwheel::CalibrationSystem::measurePositionAccuracy(
    int expected_position) {
    if (!hardware_) {
        return 1.0;  // Max error
    }

    int actual_position = hardware_->getCurrentPosition();
    return std::abs(static_cast<double>(actual_position - expected_position));
}

std::chrono::milliseconds
lithium::device::asi::filterwheel::CalibrationSystem::measureMoveTime(
    int from_position, int to_position) {
    auto start_time = std::chrono::steady_clock::now();

    if (moveToPositionAndValidate(to_position)) {
        auto end_time = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
    }

    return std::chrono::milliseconds::zero();
}

void lithium::device::asi::filterwheel::CalibrationSystem::updateProgress(
    int current, int total, const std::string& status) {
    calibration_status_ = status;

    if (progress_callback_) {
        try {
            progress_callback_(current, total, status);
        } catch (const std::exception& e) {
            spdlog::error("Exception in progress callback: {}", e.what());
        }
    }
}

void lithium::device::asi::filterwheel::CalibrationSystem::
    resetCalibrationState() {
    last_calibration_report_ = CalibrationReport{};
    current_calibration_step_ = 0;
    total_calibration_steps_ = 0;
    calibration_status_ = "Ready";
}

bool lithium::device::asi::filterwheel::CalibrationSystem::isValidPosition(
    int position) const {
    if (!hardware_) {
        return position >= 0 && position < 32;  // Default assumption
    }
    return position >= 0 && position < hardware_->getFilterCount();
}

std::string lithium::device::asi::filterwheel::CalibrationSystem::
    getDefaultCalibrationPath() const {
    std::filesystem::path config_dir;

    const char* home = std::getenv("HOME");
    if (home) {
        config_dir = std::filesystem::path(home) / ".config" / "lithium";
    } else {
        config_dir = std::filesystem::current_path() / "config";
    }

    return (config_dir / "asi_filterwheel_calibration.txt").string();
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    testBasicCommunication() {
    if (!hardware_) {
        return false;
    }

    // Try to get current position - this tests basic communication
    try {
        int position = hardware_->getCurrentPosition();
        return position >= 0;
    } catch (...) {
        return false;
    }
}

bool lithium::device::asi::filterwheel::CalibrationSystem::testMovementRange() {
    if (!hardware_ || !position_manager_) {
        return false;
    }

    int slot_count = hardware_->getFilterCount();

    // Test movement to first and last positions
    return moveToPositionAndValidate(0) &&
           moveToPositionAndValidate(slot_count - 1);
}

bool lithium::device::asi::filterwheel::CalibrationSystem::
    testPositionConsistency() {
    if (!hardware_) {
        return false;
    }

    // Test position reading consistency
    int pos1 = hardware_->getCurrentPosition();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int pos2 = hardware_->getCurrentPosition();

    return pos1 == pos2;
}

bool lithium::device::asi::filterwheel::CalibrationSystem::testMotorFunction() {
    if (!hardware_ || !position_manager_) {
        return false;
    }

    int initial_pos = hardware_->getCurrentPosition();
    int test_pos = (initial_pos + 1) % hardware_->getFilterCount();

    // Test movement in both directions
    bool forward_ok = moveToPositionAndValidate(test_pos);
    bool backward_ok = moveToPositionAndValidate(initial_pos);

    return forward_ok && backward_ok;
}

void lithium::device::asi::filterwheel::CalibrationSystem::
    generateCalibrationSummary(CalibrationReport& report) {
    if (report.position_results.empty()) {
        return;
    }

    // Calculate timing statistics
    double total_time = 0.0;
    double min_time = std::numeric_limits<double>::max();
    double max_time = 0.0;

    for (const auto& result : report.position_results) {
        double time_ms = static_cast<double>(result.move_time.count());
        total_time += time_ms;
        min_time = std::min(min_time, time_ms);
        max_time = std::max(max_time, time_ms);
    }

    report.average_move_time =
        total_time / static_cast<double>(report.position_results.size());
    report.min_move_time = min_time;
    report.max_move_time = max_time;
}

std::string
lithium::device::asi::filterwheel::CalibrationSystem::formatCalibrationResult(
    const CalibrationResult& result) const {
    std::stringstream ss;
    ss << "Position " << result.position << ": ";
    ss << (result.success ? "PASS" : "FAIL");
    ss << " (Move: " << result.move_time.count() << "ms";
    ss << ", Accuracy: " << std::fixed << std::setprecision(3)
       << result.position_accuracy << ")";

    if (!result.success && !result.error_message.empty()) {
        ss << " - " << result.error_message;
    }

    return ss.str();
}

std::string
lithium::device::asi::filterwheel::CalibrationSystem::formatDuration(
    std::chrono::milliseconds duration) const {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto ms = duration - seconds;

    return std::to_string(seconds.count()) + "." +
           std::to_string(ms.count()).substr(0, 3) + "s";
}

}  // namespace lithium::device::asi::filterwheel