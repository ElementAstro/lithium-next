#include "star.hpp"
#include "atom/error/exception.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== FindStarTask Implementation ====================

FindStarTask::FindStarTask()
    : Task("FindStar", [this](const json& params) { findGuideStar(params); }) {
    setTaskType("FindStar");

    // Set default priority and timeout
    setPriority(7);  // High priority for star finding
    setTimeout(std::chrono::seconds(30));

    // Add parameter definitions
    addParamDefinition("roi_x", "integer", false, json(-1),
                       "Region of interest X coordinate (-1 for auto)");
    addParamDefinition("roi_y", "integer", false, json(-1),
                       "Region of interest Y coordinate (-1 for auto)");
    addParamDefinition("roi_width", "integer", false, json(-1),
                       "Region of interest width (-1 for auto)");
    addParamDefinition("roi_height", "integer", false, json(-1),
                       "Region of interest height (-1 for auto)");
}

void FindStarTask::execute(const json& params) {
    try {
        addHistoryEntry("Finding guide star");

        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            THROW_INVALID_ARGUMENT(errorMsg);
        }

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to find guide star: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to find guide star: " + std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to find guide star: {}", e.what());
    }
}

void FindStarTask::findGuideStar(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    int roi_x = params.value("roi_x", -1);
    int roi_y = params.value("roi_y", -1);
    int roi_width = params.value("roi_width", -1);
    int roi_height = params.value("roi_height", -1);

    std::optional<std::array<int, 4>> roi = std::nullopt;

    if (roi_x >= 0 && roi_y >= 0 && roi_width > 0 && roi_height > 0) {
        roi = std::array<int, 4>{roi_x, roi_y, roi_width, roi_height};
        spdlog::info("Finding star in ROI: ({}, {}, {}, {})", roi_x, roi_y,
                     roi_width, roi_height);
        addHistoryEntry("Finding star in specified region");
    } else {
        spdlog::info("Finding star automatically");
        addHistoryEntry("Finding star automatically");
    }

    auto star_pos = phd2_client.value()->findStar(roi);

    spdlog::info("Star found at position: ({}, {})", star_pos[0], star_pos[1]);
    addHistoryEntry("Star found at position: (" + std::to_string(star_pos[0]) +
                    ", " + std::to_string(star_pos[1]) + ")");

    setResult({{"x", star_pos[0]}, {"y", star_pos[1]}});
}

// ==================== SetLockPositionTask Implementation ====================

SetLockPositionTask::SetLockPositionTask()
    : Task("SetLockPosition",
           [this](const json& params) { setLockPosition(params); }) {
    setTaskType("SetLockPosition");

    // Set default priority and timeout
    setPriority(7);  // High priority for lock position setting
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("x", "number", true, json(0.0),
                       "X coordinate for lock position");
    addParamDefinition("y", "number", true, json(0.0),
                       "Y coordinate for lock position");
    addParamDefinition("exact", "boolean", false, json(true),
                       "Use exact position or find nearest star");
}

void SetLockPositionTask::execute(const json& params) {
    try {
        addHistoryEntry("Setting lock position");

        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            THROW_INVALID_ARGUMENT(errorMsg);
        }

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to set lock position: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to set lock position: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to set lock position: {}", e.what());
    }
}

void SetLockPositionTask::setLockPosition(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    double x = params.value("x", 0.0);
    double y = params.value("y", 0.0);
    bool exact = params.value("exact", true);

    if (x < 0 || y < 0) {
        THROW_INVALID_ARGUMENT("Coordinates must be non-negative");
    }

    spdlog::info("Setting lock position to: ({}, {}), exact={}", x, y, exact);
    addHistoryEntry("Setting lock position to: (" + std::to_string(x) + ", " +
                    std::to_string(y) + ")");

    phd2_client.value()->setLockPosition(x, y, exact);

    spdlog::info("Lock position set successfully");
    addHistoryEntry("Lock position set successfully");
}

// ==================== GetLockPositionTask Implementation ====================

GetLockPositionTask::GetLockPositionTask()
    : Task("GetLockPosition",
           [this](const json& params) { getLockPosition(params); }) {
    setTaskType("GetLockPosition");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for getting position
    setTimeout(std::chrono::seconds(10));
}

void GetLockPositionTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting lock position");

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get lock position: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get lock position: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get lock position: {}", e.what());
    }
}

void GetLockPositionTask::getLockPosition(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    spdlog::info("Getting current lock position");
    addHistoryEntry("Getting current lock position");

    auto lock_pos = phd2_client.value()->getLockPosition();

    if (lock_pos.has_value()) {
        double x = lock_pos.value()[0];
        double y = lock_pos.value()[1];
        spdlog::info("Current lock position: ({}, {})", x, y);
        addHistoryEntry("Current lock position: (" + std::to_string(x) + ", " +
                        std::to_string(y) + ")");

        setResult({{"x", x}, {"y", y}, {"has_position", true}});
    } else {
        spdlog::info("No lock position set");
        addHistoryEntry("No lock position is currently set");

        setResult({{"has_position", false}});
    }
}

// ==================== GetPixelScaleTask Implementation ====================

GetPixelScaleTask::GetPixelScaleTask()
    : Task("GetPixelScale",
           [this](const json& params) { getPixelScale(params); }) {
    setTaskType("GetPixelScale");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for information retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetPixelScaleTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting pixel scale");

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get pixel scale: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get pixel scale: " + std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get pixel scale: {}", e.what());
    }
}

void GetPixelScaleTask::getPixelScale(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    spdlog::info("Getting pixel scale");
    addHistoryEntry("Getting pixel scale");

    double pixel_scale = phd2_client.value()->getPixelScale();

    spdlog::info("Pixel scale: {} arcsec/pixel", pixel_scale);
    addHistoryEntry("Pixel scale: " + std::to_string(pixel_scale) +
                    " arcsec/pixel");

    setResult({{"pixel_scale", pixel_scale}, {"units", "arcsec_per_pixel"}});
}

}  // namespace lithium::task::guide
