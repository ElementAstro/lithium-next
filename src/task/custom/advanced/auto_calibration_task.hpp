#ifndef LITHIUM_TASK_ADVANCED_AUTO_CALIBRATION_TASK_HPP
#define LITHIUM_TASK_ADVANCED_AUTO_CALIBRATION_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Automated Calibration Task
 *
 * Performs comprehensive calibration sequence including dark frames,
 * bias frames, and flat fields with intelligent automation.
 * Inspired by NINA's calibration automation features.
 */
class AutoCalibrationTask : public Task {
public:
    AutoCalibrationTask()
        : Task("AutoCalibration",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "AutoCalibration"; }

    // Enhanced functionality
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateAutoCalibrationParameters(const json& params);

private:
    void executeImpl(const json& params);
    void captureDarkFrames(const json& params);
    void captureBiasFrames(const json& params);
    void captureFlatFrames(const json& params);
    void organizeCalibratedFrames(const std::string& outputDir);
    bool checkExistingCalibration(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_AUTO_CALIBRATION_TASK_HPP
