#include "parameter_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include <unordered_map>

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

#define MOCK_CAMERA

namespace lithium::task::task {

// ==================== Mock Camera Parameter System ====================
#ifdef MOCK_CAMERA
class MockParameterController {
public:
    struct CameraParameters {
        int gain = 100;
        int offset = 10;
        int iso = 800;
        bool isColor = false;
        std::string gainMode = "manual"; // manual, auto
        std::string offsetMode = "manual";
        std::string isoMode = "manual";
    };

    static auto getInstance() -> MockParameterController& {
        static MockParameterController instance;
        return instance;
    }

    auto setGain(int gain) -> bool {
        if (gain < 0 || gain > 1000) return false;
        parameters_.gain = gain;
        spdlog::info("Gain set to: {}", gain);
        return true;
    }

    auto getGain() const -> int {
        return parameters_.gain;
    }

    auto setOffset(int offset) -> bool {
        if (offset < 0 || offset > 255) return false;
        parameters_.offset = offset;
        spdlog::info("Offset set to: {}", offset);
        return true;
    }

    auto getOffset() const -> int {
        return parameters_.offset;
    }

    auto setISO(int iso) -> bool {
        std::vector<int> validISO = {100, 200, 400, 800, 1600, 3200, 6400, 12800};
        if (std::find(validISO.begin(), validISO.end(), iso) == validISO.end()) {
            return false;
        }
        parameters_.iso = iso;
        spdlog::info("ISO set to: {}", iso);
        return true;
    }

    auto getISO() const -> int {
        return parameters_.iso;
    }

    auto isColor() const -> bool {
        return parameters_.isColor;
    }

    auto optimizeParameters(const std::string& target) -> json {
        json results;

        if (target == "snr" || target == "sensitivity") {
            // Optimize for signal-to-noise ratio
            parameters_.gain = 300;
            parameters_.offset = 15;
            parameters_.iso = 1600;
            results["optimized_for"] = "SNR/Sensitivity";
        } else if (target == "speed" || target == "readout") {
            // Optimize for readout speed
            parameters_.gain = 100;
            parameters_.offset = 10;
            parameters_.iso = 800;
            results["optimized_for"] = "Speed/Readout";
        } else if (target == "quality" || target == "precision") {
            // Optimize for image quality
            parameters_.gain = 150;
            parameters_.offset = 12;
            parameters_.iso = 400;
            results["optimized_for"] = "Quality/Precision";
        }

        results["parameters"] = getParameterStatus();
        return results;
    }

    auto saveProfile(const std::string& name) -> bool {
        profiles_[name] = parameters_;
        spdlog::info("Parameter profile '{}' saved", name);
        return true;
    }

    auto loadProfile(const std::string& name) -> bool {
        auto it = profiles_.find(name);
        if (it == profiles_.end()) {
            return false;
        }
        parameters_ = it->second;
        spdlog::info("Parameter profile '{}' loaded", name);
        return true;
    }

    auto getProfileList() const -> std::vector<std::string> {
        std::vector<std::string> names;
        for (const auto& pair : profiles_) {
            names.push_back(pair.first);
        }
        return names;
    }

    auto getParameterStatus() const -> json {
        return json{
            {"gain", {
                {"value", parameters_.gain},
                {"mode", parameters_.gainMode},
                {"range", {{"min", 0}, {"max", 1000}}}
            }},
            {"offset", {
                {"value", parameters_.offset},
                {"mode", parameters_.offsetMode},
                {"range", {{"min", 0}, {"max", 255}}}
            }},
            {"iso", {
                {"value", parameters_.iso},
                {"mode", parameters_.isoMode},
                {"valid_values", json::array({100, 200, 400, 800, 1600, 3200, 6400, 12800})}
            }},
            {"properties", {
                {"is_color", parameters_.isColor},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count()}
            }}
        };
    }

private:
    CameraParameters parameters_;
    std::unordered_map<std::string, CameraParameters> profiles_;
};
#endif

// ==================== GainControlTask Implementation ====================

auto GainControlTask::taskName() -> std::string {
    return "GainControl";
}

void GainControlTask::execute(const json& params) {
    try {
        validateGainParameters(params);

        int gain = params["gain"];
        std::string mode = params.value("mode", "manual");

        spdlog::info("Setting gain: {} (mode: {})", gain, mode);

#ifdef MOCK_CAMERA
        auto& controller = MockParameterController::getInstance();
        if (!controller.setGain(gain)) {
            throw atom::error::RuntimeError("Failed to set gain - value out of range");
        }
#endif

        LOG_F(INFO, "Gain control completed successfully");

    } catch (const std::exception& e) {
        handleParameterError(*this, e);
        throw;
    }
}

auto GainControlTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<GainControlTask>("GainControl",
        [](const json& params) {
            GainControlTask taskInstance("GainControl", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void GainControlTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "gain",
        .type = "integer",
        .required = true,
        .defaultValue = 100,
        .description = "Camera gain value (0-1000)"
    });

    task.addParameter({
        .name = "mode",
        .type = "string",
        .required = false,
        .defaultValue = "manual",
        .description = "Gain control mode (manual, auto)"
    });
}

void GainControlTask::validateGainParameters(const json& params) {
    if (!params.contains("gain")) {
        throw atom::error::InvalidArgument("Missing required parameter: gain");
    }

    int gain = params["gain"];
    if (gain < 0 || gain > 1000) {
        throw atom::error::InvalidArgument("Gain must be between 0 and 1000");
    }

    if (params.contains("mode")) {
        std::string mode = params["mode"];
        if (mode != "manual" && mode != "auto") {
            throw atom::error::InvalidArgument("Mode must be 'manual' or 'auto'");
        }
    }
}

void GainControlTask::handleParameterError(Task& task, const std::exception& e) {
    task.setErrorType(TaskErrorType::InvalidParameter);
    spdlog::error("Parameter control error: {}", e.what());
}

// ==================== OffsetControlTask Implementation ====================

auto OffsetControlTask::taskName() -> std::string {
    return "OffsetControl";
}

void OffsetControlTask::execute(const json& params) {
    try {
        validateOffsetParameters(params);

        int offset = params["offset"];
        spdlog::info("Setting offset: {}", offset);

#ifdef MOCK_CAMERA
        auto& controller = MockParameterController::getInstance();
        if (!controller.setOffset(offset)) {
            throw atom::error::RuntimeError("Failed to set offset - value out of range");
        }
#endif

        LOG_F(INFO, "Offset control completed successfully");

    } catch (const std::exception& e) {
        spdlog::error("OffsetControlTask failed: {}", e.what());
        throw;
    }
}

auto OffsetControlTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<OffsetControlTask>("OffsetControl",
        [](const json& params) {
            OffsetControlTask taskInstance("OffsetControl", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void OffsetControlTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "offset",
        .type = "integer",
        .required = true,
        .defaultValue = 10,
        .description = "Camera offset/pedestal value (0-255)"
    });
}

void OffsetControlTask::validateOffsetParameters(const json& params) {
    if (!params.contains("offset")) {
        throw atom::error::InvalidArgument("Missing required parameter: offset");
    }

    int offset = params["offset"];
    if (offset < 0 || offset > 255) {
        throw atom::error::InvalidArgument("Offset must be between 0 and 255");
    }
}

// ==================== ISOControlTask Implementation ====================

auto ISOControlTask::taskName() -> std::string {
    return "ISOControl";
}

void ISOControlTask::execute(const json& params) {
    try {
        validateISOParameters(params);

        int iso = params["iso"];
        spdlog::info("Setting ISO: {}", iso);

#ifdef MOCK_CAMERA
        auto& controller = MockParameterController::getInstance();
        if (!controller.setISO(iso)) {
            throw atom::error::RuntimeError("Failed to set ISO - invalid value");
        }
#endif

        LOG_F(INFO, "ISO control completed successfully");

    } catch (const std::exception& e) {
        spdlog::error("ISOControlTask failed: {}", e.what());
        throw;
    }
}

auto ISOControlTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<ISOControlTask>("ISOControl",
        [](const json& params) {
            ISOControlTask taskInstance("ISOControl", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void ISOControlTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "iso",
        .type = "integer",
        .required = true,
        .defaultValue = 800,
        .description = "ISO sensitivity value"
    });
}

void ISOControlTask::validateISOParameters(const json& params) {
    if (!params.contains("iso")) {
        throw atom::error::InvalidArgument("Missing required parameter: iso");
    }

    int iso = params["iso"];
    std::vector<int> validISO = {100, 200, 400, 800, 1600, 3200, 6400, 12800};
    if (std::find(validISO.begin(), validISO.end(), iso) == validISO.end()) {
        throw atom::error::InvalidArgument("Invalid ISO value. Valid values: 100, 200, 400, 800, 1600, 3200, 6400, 12800");
    }
}

// ==================== AutoParameterTask Implementation ====================

auto AutoParameterTask::taskName() -> std::string {
    return "AutoParameter";
}

void AutoParameterTask::execute(const json& params) {
    try {
        validateAutoParameters(params);

        std::string target = params.value("target", "snr");
        spdlog::info("Auto-optimizing parameters for: {}", target);

#ifdef MOCK_CAMERA
        auto& controller = MockParameterController::getInstance();
        auto results = controller.optimizeParameters(target);

        spdlog::info("Optimization results: {}", results.dump(2));
#endif

        LOG_F(INFO, "Auto parameter optimization completed");

    } catch (const std::exception& e) {
        spdlog::error("AutoParameterTask failed: {}", e.what());
        throw;
    }
}

auto AutoParameterTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<AutoParameterTask>("AutoParameter",
        [](const json& params) {
            AutoParameterTask taskInstance("AutoParameter", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void AutoParameterTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "target",
        .type = "string",
        .required = false,
        .defaultValue = "snr",
        .description = "Optimization target (snr, speed, quality)"
    });

    task.addParameter({
        .name = "iterations",
        .type = "integer",
        .required = false,
        .defaultValue = 5,
        .description = "Number of optimization iterations"
    });
}

void AutoParameterTask::validateAutoParameters(const json& params) {
    if (params.contains("target")) {
        std::string target = params["target"];
        std::vector<std::string> validTargets = {"snr", "sensitivity", "speed", "readout", "quality", "precision"};
        if (std::find(validTargets.begin(), validTargets.end(), target) == validTargets.end()) {
            throw atom::error::InvalidArgument("Invalid target. Valid targets: snr, sensitivity, speed, readout, quality, precision");
        }
    }

    if (params.contains("iterations")) {
        int iterations = params["iterations"];
        if (iterations < 1 || iterations > 20) {
            throw atom::error::InvalidArgument("Iterations must be between 1 and 20");
        }
    }
}

// ==================== ParameterProfileTask Implementation ====================

auto ParameterProfileTask::taskName() -> std::string {
    return "ParameterProfile";
}

void ParameterProfileTask::execute(const json& params) {
    try {
        validateProfileParameters(params);

        std::string action = params["action"];

#ifdef MOCK_CAMERA
        auto& controller = MockParameterController::getInstance();

        if (action == "save") {
            std::string name = params["name"];
            if (!controller.saveProfile(name)) {
                throw atom::error::RuntimeError("Failed to save profile");
            }
            spdlog::info("Profile '{}' saved successfully", name);

        } else if (action == "load") {
            std::string name = params["name"];
            if (!controller.loadProfile(name)) {
                throw atom::error::RuntimeError("Failed to load profile - not found");
            }
            spdlog::info("Profile '{}' loaded successfully", name);

        } else if (action == "list") {
            auto profiles = controller.getProfileList();
            spdlog::info("Available profiles: {}", json(profiles).dump());
        }
#endif

        LOG_F(INFO, "Parameter profile operation completed");

    } catch (const std::exception& e) {
        spdlog::error("ParameterProfileTask failed: {}", e.what());
        throw;
    }
}

auto ParameterProfileTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<ParameterProfileTask>("ParameterProfile",
        [](const json& params) {
            ParameterProfileTask taskInstance("ParameterProfile", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void ParameterProfileTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "action",
        .type = "string",
        .required = true,
        .defaultValue = "list",
        .description = "Profile action (save, load, list)"
    });

    task.addParameter({
        .name = "name",
        .type = "string",
        .required = false,
        .defaultValue = "",
        .description = "Profile name (required for save/load)"
    });
}

void ParameterProfileTask::validateProfileParameters(const json& params) {
    if (!params.contains("action")) {
        throw atom::error::InvalidArgument("Missing required parameter: action");
    }

    std::string action = params["action"];
    std::vector<std::string> validActions = {"save", "load", "list"};
    if (std::find(validActions.begin(), validActions.end(), action) == validActions.end()) {
        throw atom::error::InvalidArgument("Invalid action. Valid actions: save, load, list");
    }

    if ((action == "save" || action == "load") && !params.contains("name")) {
        throw atom::error::InvalidArgument("Profile name is required for save/load actions");
    }
}

// ==================== ParameterStatusTask Implementation ====================

auto ParameterStatusTask::taskName() -> std::string {
    return "ParameterStatus";
}

void ParameterStatusTask::execute(const json& params) {
    try {
        spdlog::info("Retrieving parameter status");

#ifdef MOCK_CAMERA
        auto& controller = MockParameterController::getInstance();
        auto status = controller.getParameterStatus();

        spdlog::info("Current parameter status: {}", status.dump(2));
#endif

        LOG_F(INFO, "Parameter status retrieved successfully");

    } catch (const std::exception& e) {
        spdlog::error("ParameterStatusTask failed: {}", e.what());
        throw;
    }
}

auto ParameterStatusTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<ParameterStatusTask>("ParameterStatus",
        [](const json& params) {
            ParameterStatusTask taskInstance("ParameterStatus", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void ParameterStatusTask::defineParameters(Task& task) {
    // No parameters needed for status retrieval
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register GainControlTask
AUTO_REGISTER_TASK(
    GainControlTask, "GainControl",
    (TaskInfo{
        .name = "GainControl",
        .description = "Controls camera gain settings for sensitivity adjustment",
        .category = "Parameter",
        .requiredParameters = {"gain"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"gain", json{{"type", "integer"},
                                     {"minimum", 0},
                                     {"maximum", 1000}}},
                       {"mode", json{{"type", "string"},
                                     {"enum", json::array({"manual", "auto"})}}}}},
                 {"required", json::array({"gain"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register OffsetControlTask
AUTO_REGISTER_TASK(
    OffsetControlTask, "OffsetControl",
    (TaskInfo{
        .name = "OffsetControl",
        .description = "Controls camera offset/pedestal settings",
        .category = "Parameter",
        .requiredParameters = {"offset"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"offset", json{{"type", "integer"},
                                       {"minimum", 0},
                                       {"maximum", 255}}}}},
                 {"required", json::array({"offset"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register ISOControlTask
AUTO_REGISTER_TASK(
    ISOControlTask, "ISOControl",
    (TaskInfo{
        .name = "ISOControl",
        .description = "Controls ISO sensitivity settings for DSLR-type cameras",
        .category = "Parameter",
        .requiredParameters = {"iso"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"iso", json{{"type", "integer"},
                                    {"enum", json::array({100, 200, 400, 800, 1600, 3200, 6400, 12800})}}}}},
                 {"required", json::array({"iso"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register AutoParameterTask
AUTO_REGISTER_TASK(
    AutoParameterTask, "AutoParameter",
    (TaskInfo{
        .name = "AutoParameter",
        .description = "Automatically optimizes camera parameters for different scenarios",
        .category = "Parameter",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"target", json{{"type", "string"},
                                       {"enum", json::array({"snr", "sensitivity", "speed", "readout", "quality", "precision"})}}},
                       {"iterations", json{{"type", "integer"},
                                           {"minimum", 1},
                                           {"maximum", 20}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register ParameterProfileTask
AUTO_REGISTER_TASK(
    ParameterProfileTask, "ParameterProfile",
    (TaskInfo{
        .name = "ParameterProfile",
        .description = "Manages parameter profiles for different imaging scenarios",
        .category = "Parameter",
        .requiredParameters = {"action"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"action", json{{"type", "string"},
                                       {"enum", json::array({"save", "load", "list"})}}},
                       {"name", json{{"type", "string"}}}}},
                 {"required", json::array({"action"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register ParameterStatusTask
AUTO_REGISTER_TASK(
    ParameterStatusTask, "ParameterStatus",
    (TaskInfo{
        .name = "ParameterStatus",
        .description = "Retrieves current camera parameter values and status",
        .category = "Parameter",
        .requiredParameters = {},
        .parameterSchema = json{{"type", "object"}, {"properties", json{}}},
        .version = "1.0.0",
        .dependencies = {}}));

}  // namespace
