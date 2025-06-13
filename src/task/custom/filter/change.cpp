#include "change.hpp"

#include <thread>
#include "spdlog/spdlog.h"

namespace lithium::task::filter {

FilterChangeTask::FilterChangeTask(const std::string& name)
    : BaseFilterTask(name) {
    setupFilterChangeDefaults();
}

void FilterChangeTask::setupFilterChangeDefaults() {
    // Override base class defaults with specific ones for filter changes
    addParamDefinition("filterName", "string", true, nullptr,
                       "Name of the filter to change to");
    addParamDefinition("position", "number", false, -1,
                       "Filter position number (alternative to filterName)");
    addParamDefinition("timeout", "number", false, 30,
                       "Maximum wait time in seconds");
    addParamDefinition("verify", "boolean", false, true,
                       "Verify filter position after change");
    addParamDefinition("retries", "number", false, 3,
                       "Number of retry attempts on failure");
    addParamDefinition("settlingTime", "number", false, 1.0,
                       "Time to wait after filter change");

    setTaskType("filter_change");
    setTimeout(std::chrono::seconds(60));
    setPriority(7);  // High priority for filter changes

    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("Filter change task exception: {}", e.what());
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Filter change exception: " + std::string(e.what()));
    });
}

void FilterChangeTask::execute(const json& params) {
    addHistoryEntry("Starting filter change task");

    try {
        validateFilterParams(params);

        std::string filterName;
        int position = params.value("position", -1);

        if (params.contains("filterName") &&
            !params["filterName"].get<std::string>().empty()) {
            filterName = params["filterName"].get<std::string>();
        } else if (position >= 0) {
            // Find filter by position
            auto filters = getAvailableFilters();
            for (const auto& filter : filters) {
                if (filter.position == position) {
                    filterName = filter.name;
                    break;
                }
            }
            if (filterName.empty()) {
                throw std::invalid_argument("No filter found at position " +
                                            std::to_string(position));
            }
        } else {
            throw std::invalid_argument(
                "Either filterName or position must be specified");
        }

        int timeout = params.value("timeout", 30);
        bool verify = params.value("verify", true);
        maxRetries_ = params.value("retries", 3);

        bool success = changeToFilter(filterName, timeout, verify);

        if (!success) {
            setErrorType(TaskErrorType::DeviceError);
            throw std::runtime_error("Filter change failed: " + filterName);
        }

        // Optional settling time
        double settlingTime = params.value("settlingTime", 1.0);
        if (settlingTime > 0) {
            addHistoryEntry("Waiting for filter to settle: " +
                            std::to_string(settlingTime) + "s");
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(settlingTime * 1000)));
        }

        addHistoryEntry("Filter change completed successfully: " + filterName);

    } catch (const std::exception& e) {
        handleFilterError(params.value("filterName", "unknown"), e.what());
        throw;
    }
}

bool FilterChangeTask::changeToFilter(const std::string& filterName,
                                      int timeout, bool verify) {
    spdlog::info("Changing to filter: {} (timeout: {}s, verify: {})",
                 filterName, timeout, verify);
    addHistoryEntry("Attempting filter change: " + filterName);

    auto startTime = std::chrono::steady_clock::now();

    for (int attempt = 1; attempt <= maxRetries_; ++attempt) {
        try {
            addHistoryEntry("Filter change attempt " + std::to_string(attempt) +
                            "/" + std::to_string(maxRetries_));

            // Perform the actual filter change
            bool changeResult = changeFilter(filterName);

            if (!changeResult) {
                if (attempt < maxRetries_) {
                    spdlog::warn("Filter change attempt {} failed, retrying...",
                                 attempt);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                } else {
                    return false;
                }
            }

            // Wait for filter wheel to stop moving
            if (!waitForFilterWheel(timeout)) {
                if (attempt < maxRetries_) {
                    spdlog::warn(
                        "Filter wheel timeout on attempt {}, retrying...",
                        attempt);
                    continue;
                } else {
                    return false;
                }
            }

            // Verify position if requested
            if (verify && !verifyFilterPosition(filterName)) {
                if (attempt < maxRetries_) {
                    spdlog::warn(
                        "Filter position verification failed on attempt {}, "
                        "retrying...",
                        attempt);
                    continue;
                } else {
                    return false;
                }
            }

            // Success - record timing
            auto endTime = std::chrono::steady_clock::now();
            lastChangeTime_ =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    endTime - startTime);

            spdlog::info("Filter change successful: {} (took {}ms)", filterName,
                         lastChangeTime_.count());
            addHistoryEntry("Filter change successful: " + filterName +
                            " (took " +
                            std::to_string(lastChangeTime_.count()) + "ms)");

            return true;

        } catch (const std::exception& e) {
            spdlog::error("Filter change attempt {} failed: {}", attempt,
                          e.what());
            if (attempt >= maxRetries_) {
                throw;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    return false;
}

bool FilterChangeTask::changeToPosition(int position, int timeout,
                                        bool verify) {
    // Find filter name by position
    auto filters = getAvailableFilters();
    for (const auto& filter : filters) {
        if (filter.position == position) {
            return changeToFilter(filter.name, timeout, verify);
        }
    }

    handleFilterError(
        "position_" + std::to_string(position),
        "No filter found at position " + std::to_string(position));
    return false;
}

std::chrono::milliseconds FilterChangeTask::getLastChangeTime() const {
    return lastChangeTime_;
}

bool FilterChangeTask::verifyFilterPosition(const std::string& expectedFilter) {
    addHistoryEntry("Verifying filter position: " + expectedFilter);

    try {
        std::string currentFilter = getCurrentFilter();

        if (currentFilter == expectedFilter) {
            addHistoryEntry("Filter position verified: " + expectedFilter);
            return true;
        } else {
            spdlog::error("Filter position mismatch: expected '{}', got '{}'",
                          expectedFilter, currentFilter);
            addHistoryEntry("Filter position mismatch: expected '" +
                            expectedFilter + "', got '" + currentFilter + "'");
            return false;
        }

    } catch (const std::exception& e) {
        spdlog::error("Filter position verification failed: {}", e.what());
        addHistoryEntry("Filter position verification failed: " +
                        std::string(e.what()));
        return false;
    }
}

}  // namespace lithium::task::filter