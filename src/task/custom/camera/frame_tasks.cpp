#include "frame_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include <random>

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

#define MOCK_CAMERA

namespace lithium::task::task {

// ==================== Mock Camera Frame System ====================
#ifdef MOCK_CAMERA
class MockFrameController {
public:
    struct FrameSettings {
        int width = 1920;
        int height = 1080;
        int maxWidth = 6000;
        int maxHeight = 4000;
        int startX = 0;
        int startY = 0;
        int binX = 1;
        int binY = 1;
        std::string frameType = "FITS";
        std::string uploadMode = "LOCAL";
        double pixelSize = 3.76; // microns
        bool isColor = false;
    };

    static auto getInstance() -> MockFrameController& {
        static MockFrameController instance;
        return instance;
    }

    auto setResolution(int x, int y, int width, int height) -> bool {
        if (x < 0 || y < 0 || width <= 0 || height <= 0) return false;
        if (x + width > settings_.maxWidth || y + height > settings_.maxHeight) return false;

        settings_.startX = x;
        settings_.startY = y;
        settings_.width = width;
        settings_.height = height;

        spdlog::info("Resolution set: {}x{} at ({}, {})", width, height, x, y);
        return true;
    }

    auto setBinning(int horizontal, int vertical) -> bool {
        if (horizontal < 1 || vertical < 1 || horizontal > 4 || vertical > 4) return false;

        settings_.binX = horizontal;
        settings_.binY = vertical;

        spdlog::info("Binning set: {}x{}", horizontal, vertical);
        return true;
    }

    auto setFrameType(const std::string& type) -> bool {
        std::vector<std::string> validTypes = {"FITS", "NATIVE", "XISF", "JPG", "PNG", "TIFF"};
        if (std::find(validTypes.begin(), validTypes.end(), type) == validTypes.end()) {
            return false;
        }

        settings_.frameType = type;
        spdlog::info("Frame type set: {}", type);
        return true;
    }

    auto setUploadMode(const std::string& mode) -> bool {
        std::vector<std::string> validModes = {"CLIENT", "LOCAL", "BOTH", "CLOUD"};
        if (std::find(validModes.begin(), validModes.end(), mode) == validModes.end()) {
            return false;
        }

        settings_.uploadMode = mode;
        spdlog::info("Upload mode set: {}", mode);
        return true;
    }

    auto getFrameInfo() const -> json {
        return json{
            {"resolution", {
                {"width", settings_.width},
                {"height", settings_.height},
                {"max_width", settings_.maxWidth},
                {"max_height", settings_.maxHeight},
                {"start_x", settings_.startX},
                {"start_y", settings_.startY}
            }},
            {"binning", {
                {"horizontal", settings_.binX},
                {"vertical", settings_.binY}
            }},
            {"pixel", {
                {"size", settings_.pixelSize},
                {"size_x", settings_.pixelSize},
                {"size_y", settings_.pixelSize},
                {"depth", 16.0}
            }},
            {"format", {
                {"type", settings_.frameType},
                {"upload_mode", settings_.uploadMode}
            }},
            {"properties", {
                {"is_color", settings_.isColor},
                {"binned_width", settings_.width / settings_.binX},
                {"binned_height", settings_.height / settings_.binY}
            }}
        };
    }

    auto generateFrameStats() const -> json {
        // Generate realistic mock statistics
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);

        int effectiveWidth = settings_.width / settings_.binX;
        int effectiveHeight = settings_.height / settings_.binY;
        int totalPixels = effectiveWidth * effectiveHeight;

        double mean = 1500.0 + dis(gen) * 500.0;
        double stddev = 50.0 + dis(gen) * 20.0;
        double min_val = mean - 3 * stddev;
        double max_val = mean + 3 * stddev;

        return json{
            {"statistics", {
                {"mean", mean},
                {"stddev", stddev},
                {"min", min_val},
                {"max", max_val},
                {"median", mean + (dis(gen) - 0.5) * 10.0}
            }},
            {"dimensions", {
                {"effective_width", effectiveWidth},
                {"effective_height", effectiveHeight},
                {"total_pixels", totalPixels},
                {"binning_factor", settings_.binX * settings_.binY}
            }},
            {"quality", {
                {"snr", 20.0 + dis(gen) * 10.0},
                {"fwhm", 2.5 + dis(gen) * 1.0},
                {"saturation_percentage", dis(gen) * 5.0}
            }}
        };
    }

    auto getSettings() const -> const FrameSettings& {
        return settings_;
    }

private:
    FrameSettings settings_;
};
#endif

// ==================== FrameConfigTask Implementation ====================

auto FrameConfigTask::taskName() -> std::string {
    return "FrameConfig";
}

void FrameConfigTask::execute(const json& params) {
    try {
        validateFrameParameters(params);

        spdlog::info("Configuring frame settings: {}", params.dump());

#ifdef MOCK_CAMERA
        auto& controller = MockFrameController::getInstance();

        // Set resolution if provided
        if (params.contains("width") && params.contains("height")) {
            int width = params["width"];
            int height = params["height"];
            int x = params.value("x", 0);
            int y = params.value("y", 0);

            if (!controller.setResolution(x, y, width, height)) {
                throw atom::error::RuntimeError("Failed to set resolution");
            }
        }

        // Set binning if provided
        if (params.contains("binning")) {
            auto binning = params["binning"];
            int binX = binning.value("x", 1);
            int binY = binning.value("y", 1);

            if (!controller.setBinning(binX, binY)) {
                throw atom::error::RuntimeError("Failed to set binning");
            }
        }

        // Set frame type if provided
        if (params.contains("frame_type")) {
            std::string frameType = params["frame_type"];
            if (!controller.setFrameType(frameType)) {
                throw atom::error::RuntimeError("Failed to set frame type");
            }
        }

        // Set upload mode if provided
        if (params.contains("upload_mode")) {
            std::string uploadMode = params["upload_mode"];
            if (!controller.setUploadMode(uploadMode)) {
                throw atom::error::RuntimeError("Failed to set upload mode");
            }
        }
#endif

        LOG_F(INFO, "Frame configuration completed successfully");

    } catch (const std::exception& e) {
        handleFrameError(*this, e);
        throw;
    }
}

auto FrameConfigTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<FrameConfigTask>("FrameConfig",
        [](const json& params) {
            FrameConfigTask taskInstance("FrameConfig", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void FrameConfigTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "width",
        .type = "integer",
        .required = false,
        .defaultValue = 1920,
        .description = "Frame width in pixels"
    });

    task.addParameter({
        .name = "height",
        .type = "integer",
        .required = false,
        .defaultValue = 1080,
        .description = "Frame height in pixels"
    });

    task.addParameter({
        .name = "x",
        .type = "integer",
        .required = false,
        .defaultValue = 0,
        .description = "Frame start X coordinate"
    });

    task.addParameter({
        .name = "y",
        .type = "integer",
        .required = false,
        .defaultValue = 0,
        .description = "Frame start Y coordinate"
    });

    task.addParameter({
        .name = "binning",
        .type = "object",
        .required = false,
        .defaultValue = json{{"x", 1}, {"y", 1}},
        .description = "Binning configuration"
    });

    task.addParameter({
        .name = "frame_type",
        .type = "string",
        .required = false,
        .defaultValue = "FITS",
        .description = "Frame file format"
    });

    task.addParameter({
        .name = "upload_mode",
        .type = "string",
        .required = false,
        .defaultValue = "LOCAL",
        .description = "Upload destination mode"
    });
}

void FrameConfigTask::validateFrameParameters(const json& params) {
    if (params.contains("width")) {
        int width = params["width"];
        if (width <= 0 || width > 10000) {
            throw atom::error::InvalidArgument("Width must be between 1 and 10000 pixels");
        }
    }

    if (params.contains("height")) {
        int height = params["height"];
        if (height <= 0 || height > 10000) {
            throw atom::error::InvalidArgument("Height must be between 1 and 10000 pixels");
        }
    }

    if (params.contains("frame_type")) {
        std::string frameType = params["frame_type"];
        std::vector<std::string> validTypes = {"FITS", "NATIVE", "XISF", "JPG", "PNG", "TIFF"};
        if (std::find(validTypes.begin(), validTypes.end(), frameType) == validTypes.end()) {
            throw atom::error::InvalidArgument("Invalid frame type");
        }
    }
}

void FrameConfigTask::handleFrameError(Task& task, const std::exception& e) {
    task.setErrorType(TaskErrorType::InvalidParameter);
    spdlog::error("Frame configuration error: {}", e.what());
}

// ==================== ROIConfigTask Implementation ====================

auto ROIConfigTask::taskName() -> std::string {
    return "ROIConfig";
}

void ROIConfigTask::execute(const json& params) {
    try {
        validateROIParameters(params);

        int x = params["x"];
        int y = params["y"];
        int width = params["width"];
        int height = params["height"];

        spdlog::info("Setting ROI: {}x{} at ({}, {})", width, height, x, y);

#ifdef MOCK_CAMERA
        auto& controller = MockFrameController::getInstance();
        if (!controller.setResolution(x, y, width, height)) {
            throw atom::error::RuntimeError("Failed to set ROI");
        }
#endif

        LOG_F(INFO, "ROI configuration completed");

    } catch (const std::exception& e) {
        spdlog::error("ROIConfigTask failed: {}", e.what());
        throw;
    }
}

auto ROIConfigTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<ROIConfigTask>("ROIConfig",
        [](const json& params) {
            ROIConfigTask taskInstance("ROIConfig", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void ROIConfigTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "x",
        .type = "integer",
        .required = true,
        .defaultValue = 0,
        .description = "ROI start X coordinate"
    });

    task.addParameter({
        .name = "y",
        .type = "integer",
        .required = true,
        .defaultValue = 0,
        .description = "ROI start Y coordinate"
    });

    task.addParameter({
        .name = "width",
        .type = "integer",
        .required = true,
        .defaultValue = 1920,
        .description = "ROI width in pixels"
    });

    task.addParameter({
        .name = "height",
        .type = "integer",
        .required = true,
        .defaultValue = 1080,
        .description = "ROI height in pixels"
    });
}

void ROIConfigTask::validateROIParameters(const json& params) {
    std::vector<std::string> required = {"x", "y", "width", "height"};
    for (const auto& param : required) {
        if (!params.contains(param)) {
            throw atom::error::InvalidArgument("Missing required parameter: " + param);
        }
    }

    int x = params["x"];
    int y = params["y"];
    int width = params["width"];
    int height = params["height"];

    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        throw atom::error::InvalidArgument("Invalid ROI dimensions");
    }

    if (x + width > 6000 || y + height > 4000) {
        throw atom::error::InvalidArgument("ROI exceeds maximum sensor dimensions");
    }
}

// ==================== BinningConfigTask Implementation ====================

auto BinningConfigTask::taskName() -> std::string {
    return "BinningConfig";
}

void BinningConfigTask::execute(const json& params) {
    try {
        validateBinningParameters(params);

        int binX = params.value("horizontal", 1);
        int binY = params.value("vertical", 1);

        spdlog::info("Setting binning: {}x{}", binX, binY);

#ifdef MOCK_CAMERA
        auto& controller = MockFrameController::getInstance();
        if (!controller.setBinning(binX, binY)) {
            throw atom::error::RuntimeError("Failed to set binning");
        }
#endif

        LOG_F(INFO, "Binning configuration completed");

    } catch (const std::exception& e) {
        spdlog::error("BinningConfigTask failed: {}", e.what());
        throw;
    }
}

auto BinningConfigTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<BinningConfigTask>("BinningConfig",
        [](const json& params) {
            BinningConfigTask taskInstance("BinningConfig", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void BinningConfigTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "horizontal",
        .type = "integer",
        .required = false,
        .defaultValue = 1,
        .description = "Horizontal binning factor"
    });

    task.addParameter({
        .name = "vertical",
        .type = "integer",
        .required = false,
        .defaultValue = 1,
        .description = "Vertical binning factor"
    });
}

void BinningConfigTask::validateBinningParameters(const json& params) {
    if (params.contains("horizontal")) {
        int binX = params["horizontal"];
        if (binX < 1 || binX > 4) {
            throw atom::error::InvalidArgument("Horizontal binning must be between 1 and 4");
        }
    }

    if (params.contains("vertical")) {
        int binY = params["vertical"];
        if (binY < 1 || binY > 4) {
            throw atom::error::InvalidArgument("Vertical binning must be between 1 and 4");
        }
    }
}

// ==================== FrameInfoTask Implementation ====================

auto FrameInfoTask::taskName() -> std::string {
    return "FrameInfo";
}

void FrameInfoTask::execute(const json& params) {
    try {
        spdlog::info("Retrieving frame information");

#ifdef MOCK_CAMERA
        auto& controller = MockFrameController::getInstance();
        auto frameInfo = controller.getFrameInfo();

        spdlog::info("Current frame info: {}", frameInfo.dump(2));
#endif

        LOG_F(INFO, "Frame information retrieved successfully");

    } catch (const std::exception& e) {
        spdlog::error("FrameInfoTask failed: {}", e.what());
        throw;
    }
}

auto FrameInfoTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<FrameInfoTask>("FrameInfo",
        [](const json& params) {
            FrameInfoTask taskInstance("FrameInfo", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void FrameInfoTask::defineParameters(Task& task) {
    // No parameters needed for frame info retrieval
}

// ==================== UploadModeTask Implementation ====================

auto UploadModeTask::taskName() -> std::string {
    return "UploadMode";
}

void UploadModeTask::execute(const json& params) {
    try {
        validateUploadParameters(params);

        std::string mode = params["mode"];
        spdlog::info("Setting upload mode: {}", mode);

#ifdef MOCK_CAMERA
        auto& controller = MockFrameController::getInstance();
        if (!controller.setUploadMode(mode)) {
            throw atom::error::RuntimeError("Failed to set upload mode");
        }
#endif

        LOG_F(INFO, "Upload mode configuration completed");

    } catch (const std::exception& e) {
        spdlog::error("UploadModeTask failed: {}", e.what());
        throw;
    }
}

auto UploadModeTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<UploadModeTask>("UploadMode",
        [](const json& params) {
            UploadModeTask taskInstance("UploadMode", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void UploadModeTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "mode",
        .type = "string",
        .required = true,
        .defaultValue = "LOCAL",
        .description = "Upload mode (CLIENT, LOCAL, BOTH, CLOUD)"
    });
}

void UploadModeTask::validateUploadParameters(const json& params) {
    if (!params.contains("mode")) {
        throw atom::error::InvalidArgument("Missing required parameter: mode");
    }

    std::string mode = params["mode"];
    std::vector<std::string> validModes = {"CLIENT", "LOCAL", "BOTH", "CLOUD"};
    if (std::find(validModes.begin(), validModes.end(), mode) == validModes.end()) {
        throw atom::error::InvalidArgument("Invalid upload mode");
    }
}

// ==================== FrameStatsTask Implementation ====================

auto FrameStatsTask::taskName() -> std::string {
    return "FrameStats";
}

void FrameStatsTask::execute(const json& params) {
    try {
        spdlog::info("Analyzing frame statistics");

#ifdef MOCK_CAMERA
        auto& controller = MockFrameController::getInstance();
        auto stats = controller.generateFrameStats();

        spdlog::info("Frame statistics: {}", stats.dump(2));
#endif

        LOG_F(INFO, "Frame statistics analysis completed");

    } catch (const std::exception& e) {
        spdlog::error("FrameStatsTask failed: {}", e.what());
        throw;
    }
}

auto FrameStatsTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<FrameStatsTask>("FrameStats",
        [](const json& params) {
            FrameStatsTask taskInstance("FrameStats", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void FrameStatsTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "include_histogram",
        .type = "boolean",
        .required = false,
        .defaultValue = false,
        .description = "Include histogram data in statistics"
    });

    task.addParameter({
        .name = "region",
        .type = "object",
        .required = false,
        .defaultValue = json{},
        .description = "Specific region to analyze (x, y, width, height)"
    });
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register FrameConfigTask
AUTO_REGISTER_TASK(
    FrameConfigTask, "FrameConfig",
    (TaskInfo{
        .name = "FrameConfig",
        .description = "Configures camera frame settings including resolution, binning, and format",
        .category = "Frame",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"width", json{{"type", "integer"},
                                      {"minimum", 1},
                                      {"maximum", 10000}}},
                       {"height", json{{"type", "integer"},
                                       {"minimum", 1},
                                       {"maximum", 10000}}},
                       {"x", json{{"type", "integer"},
                                  {"minimum", 0}}},
                       {"y", json{{"type", "integer"},
                                  {"minimum", 0}}},
                       {"binning", json{{"type", "object"},
                                        {"properties",
                                         json{{"x", json{{"type", "integer"},
                                                         {"minimum", 1},
                                                         {"maximum", 4}}},
                                              {"y", json{{"type", "integer"},
                                                         {"minimum", 1},
                                                         {"maximum", 4}}}}}}},
                       {"frame_type", json{{"type", "string"},
                                           {"enum", json::array({"FITS", "NATIVE", "XISF", "JPG", "PNG", "TIFF"})}}},
                       {"upload_mode", json{{"type", "string"},
                                            {"enum", json::array({"CLIENT", "LOCAL", "BOTH", "CLOUD"})}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register ROIConfigTask
AUTO_REGISTER_TASK(
    ROIConfigTask, "ROIConfig",
    (TaskInfo{
        .name = "ROIConfig",
        .description = "Configures Region of Interest (ROI) for targeted imaging",
        .category = "Frame",
        .requiredParameters = {"x", "y", "width", "height"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"x", json{{"type", "integer"},
                                  {"minimum", 0}}},
                       {"y", json{{"type", "integer"},
                                  {"minimum", 0}}},
                       {"width", json{{"type", "integer"},
                                      {"minimum", 1}}},
                       {"height", json{{"type", "integer"},
                                       {"minimum", 1}}}}},
                 {"required", json::array({"x", "y", "width", "height"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register BinningConfigTask
AUTO_REGISTER_TASK(
    BinningConfigTask, "BinningConfig",
    (TaskInfo{
        .name = "BinningConfig",
        .description = "Configures pixel binning for improved sensitivity or speed",
        .category = "Frame",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"horizontal", json{{"type", "integer"},
                                           {"minimum", 1},
                                           {"maximum", 4}}},
                       {"vertical", json{{"type", "integer"},
                                         {"minimum", 1},
                                         {"maximum", 4}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register FrameInfoTask
AUTO_REGISTER_TASK(
    FrameInfoTask, "FrameInfo",
    (TaskInfo{
        .name = "FrameInfo",
        .description = "Retrieves detailed information about current frame settings",
        .category = "Frame",
        .requiredParameters = {},
        .parameterSchema = json{{"type", "object"}, {"properties", json{}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register UploadModeTask
AUTO_REGISTER_TASK(
    UploadModeTask, "UploadMode",
    (TaskInfo{
        .name = "UploadMode",
        .description = "Configures upload destination for captured images",
        .category = "Frame",
        .requiredParameters = {"mode"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"mode", json{{"type", "string"},
                                     {"enum", json::array({"CLIENT", "LOCAL", "BOTH", "CLOUD"})}}}}},
                 {"required", json::array({"mode"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register FrameStatsTask
AUTO_REGISTER_TASK(
    FrameStatsTask, "FrameStats",
    (TaskInfo{
        .name = "FrameStats",
        .description = "Analyzes frame data and provides statistical information",
        .category = "Frame",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"include_histogram", json{{"type", "boolean"}}},
                       {"region", json{{"type", "object"},
                                       {"properties",
                                        json{{"x", json{{"type", "integer"}}},
                                             {"y", json{{"type", "integer"}}},
                                             {"width", json{{"type", "integer"}}},
                                             {"height", json{{"type", "integer"}}}}}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

}  // namespace
