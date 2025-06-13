#include "base.hpp"

#include "spdlog/spdlog.h"

namespace lithium::task::filter {

BaseFilterTask::BaseFilterTask(const std::string& name)
    : Task(name, [this](const json& params) { execute(params); }),
      isConnected_(false) {
    setupFilterDefaults();

    // Initialize available filters (this would typically come from hardware)
    availableFilters_ = {
        {"Luminance", FilterType::LRGB, 1, 60.0, "Clear luminance filter"},
        {"Red", FilterType::LRGB, 2, 60.0, "Red color filter"},
        {"Green", FilterType::LRGB, 3, 60.0, "Green color filter"},
        {"Blue", FilterType::LRGB, 4, 60.0, "Blue color filter"},
        {"Ha", FilterType::Narrowband, 5, 300.0,
         "Hydrogen-alpha narrowband filter"},
        {"OIII", FilterType::Narrowband, 6, 300.0,
         "Oxygen III narrowband filter"},
        {"SII", FilterType::Narrowband, 7, 300.0,
         "Sulfur II narrowband filter"},
        {"Clear", FilterType::Broadband, 8, 30.0, "Clear broadband filter"}};
}

void BaseFilterTask::setupFilterDefaults() {
    // Common filter parameters
    addParamDefinition("filterName", "string", false, "",
                       "Name of the filter to use");
    addParamDefinition("timeout", "number", false, 30,
                       "Filter change timeout in seconds");
    addParamDefinition("verify", "boolean", false, true,
                       "Verify filter position after change");
    addParamDefinition("retries", "number", false, 3,
                       "Number of retry attempts");
    addParamDefinition("settlingTime", "number", false, 1.0,
                       "Time to wait after filter change");

    // Set task defaults
    setTimeout(std::chrono::seconds(300));
    setPriority(6);
    setLogLevel(2);
    setTaskType("filter_task");

    // Set exception callback
    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("Filter task exception: {}", e.what());
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Filter exception: " + std::string(e.what()));
    });
}

std::vector<FilterInfo> BaseFilterTask::getAvailableFilters() const {
    return availableFilters_;
}

bool BaseFilterTask::isFilterAvailable(const std::string& filterName) const {
    for (const auto& filter : availableFilters_) {
        if (filter.name == filterName) {
            return true;
        }
    }
    return false;
}

std::string BaseFilterTask::getCurrentFilter() const { return currentFilter_; }

bool BaseFilterTask::isFilterWheelMoving() const {
    // This would query the actual hardware
    // For now, return false (assuming not moving)
    return false;
}

bool BaseFilterTask::changeFilter(const std::string& filterName) {
    addHistoryEntry("Changing to filter: " + filterName);

    if (!isFilterAvailable(filterName)) {
        handleFilterError(filterName, "Filter not available");
        return false;
    }

    if (currentFilter_ == filterName) {
        addHistoryEntry("Filter already selected: " + filterName);
        return true;
    }

    try {
        // Simulate filter wheel movement
        spdlog::info("Changing filter from '{}' to '{}'", currentFilter_,
                     filterName);

        // Here you would send commands to the actual filter wheel hardware
        // For simulation, just wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        currentFilter_ = filterName;
        addHistoryEntry("Filter changed to: " + filterName);
        return true;

    } catch (const std::exception& e) {
        handleFilterError(filterName,
                          "Filter change failed: " + std::string(e.what()));
        return false;
    }
}

bool BaseFilterTask::waitForFilterWheel(int timeoutSeconds) {
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeoutSeconds);

    while (isFilterWheelMoving()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > timeout) {
            handleFilterError("", "Filter wheel timeout");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return true;
}

void BaseFilterTask::validateFilterParams(const json& params) {
    if (!validateParams(params)) {
        auto errors = getParamErrors();
        std::string errorMsg = "Filter parameter validation failed: ";
        for (const auto& error : errors) {
            errorMsg += error + "; ";
        }
        setErrorType(TaskErrorType::InvalidParameter);
        throw std::invalid_argument(errorMsg);
    }
}

void BaseFilterTask::handleFilterError(const std::string& filterName,
                                       const std::string& error) {
    std::string fullError = "Filter error";
    if (!filterName.empty()) {
        fullError += " [" + filterName + "]";
    }
    fullError += ": " + error;

    spdlog::error(fullError);
    setErrorType(TaskErrorType::DeviceError);
    addHistoryEntry(fullError);
}

}  // namespace lithium::task::filter