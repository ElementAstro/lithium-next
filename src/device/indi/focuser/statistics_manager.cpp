#include "statistics_manager.hpp"

namespace lithium::device::indi::focuser {

StatisticsManager::StatisticsManager(std::shared_ptr<INDIFocuserCore> core)
    : FocuserComponentBase(std::move(core)) {
    sessionStart_ = std::chrono::steady_clock::now();
}

bool StatisticsManager::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    sessionStart_ = std::chrono::steady_clock::now();
    core->getLogger()->info("{}: Initializing statistics manager", getComponentName());
    return true;
}

void StatisticsManager::shutdown() {
    auto core = getCore();
    if (core) {
        sessionEnd_ = std::chrono::steady_clock::now();
        core->getLogger()->info("{}: Shutting down statistics manager", getComponentName());
    }
}

uint64_t StatisticsManager::getTotalSteps() const {
    // In the new architecture, this could be stored in persistent storage
    // For now, we'll use a static variable
    static uint64_t totalSteps = 0;
    return totalSteps;
}

bool StatisticsManager::resetTotalSteps() {
    static uint64_t totalSteps = 0;
    totalSteps = 0;

    auto core = getCore();
    if (core) {
        core->getLogger()->info("Reset total steps counter");
    }
    return true;
}

int StatisticsManager::getLastMoveSteps() const {
    if (historyCount_ == 0) {
        return 0;
    }

    // Get the most recent entry
    size_t lastIndex = (historyIndex_ + HISTORY_SIZE - 1) % HISTORY_SIZE;
    return stepHistory_[lastIndex];
}

int StatisticsManager::getLastMoveDuration() const {
    if (historyCount_ == 0) {
        return 0;
    }

    // Get the most recent entry
    size_t lastIndex = (historyIndex_ + HISTORY_SIZE - 1) % HISTORY_SIZE;
    return durationHistory_[lastIndex];
}

void StatisticsManager::recordMovement(int steps, int durationMs) {
    // Update static total steps
    static uint64_t totalSteps = 0;
    totalSteps += std::abs(steps);

    // Update move count
    totalMoves_++;

    // Update history
    updateHistory(steps, durationMs);

    auto core = getCore();
    if (core) {
        core->getLogger()->debug("Recorded move: {} steps, {} ms", steps, durationMs);
    }
}

double StatisticsManager::getAverageStepsPerMove() const {
    if (totalMoves_ == 0) {
        return 0.0;
    }

    uint64_t validHistoryCount = std::min(historyCount_, HISTORY_SIZE);
    if (validHistoryCount == 0) {
        return 0.0;
    }

    int totalSteps = 0;
    for (size_t i = 0; i < validHistoryCount; ++i) {
        totalSteps += std::abs(stepHistory_[i]);
    }

    return static_cast<double>(totalSteps) / validHistoryCount;
}

double StatisticsManager::getAverageMoveDuration() const {
    if (totalMoves_ == 0) {
        return 0.0;
    }

    uint64_t validHistoryCount = std::min(historyCount_, HISTORY_SIZE);
    if (validHistoryCount == 0) {
        return 0.0;
    }

    int totalDuration = 0;
    for (size_t i = 0; i < validHistoryCount; ++i) {
        totalDuration += durationHistory_[i];
    }

    return static_cast<double>(totalDuration) / validHistoryCount;
}

uint64_t StatisticsManager::getTotalMoves() const {
    return totalMoves_;
}

void StatisticsManager::startSession() {
    sessionStart_ = std::chrono::steady_clock::now();
    sessionStartSteps_ = getTotalSteps();
    sessionStartMoves_ = totalMoves_;

    auto core = getCore();
    if (core) {
        core->getLogger()->info("Started new statistics session");
    }
}

void StatisticsManager::endSession() {
    sessionEnd_ = std::chrono::steady_clock::now();

    auto core = getCore();
    if (core) {
        auto duration = getSessionDuration();
        core->getLogger()->info("Ended statistics session - Duration: {} ms, Steps: {}, Moves: {}",
                               duration.count(), getSessionSteps(), getSessionMoves());
    }
}

uint64_t StatisticsManager::getSessionSteps() const {
    return getTotalSteps() - sessionStartSteps_;
}

uint64_t StatisticsManager::getSessionMoves() const {
    return totalMoves_ - sessionStartMoves_;
}

std::chrono::milliseconds StatisticsManager::getSessionDuration() const {
    auto endTime = (sessionEnd_ > sessionStart_) ? sessionEnd_ : std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - sessionStart_);
}

void StatisticsManager::updateHistory(int steps, int duration) {
    stepHistory_[historyIndex_] = steps;
    durationHistory_[historyIndex_] = duration;

    historyIndex_ = (historyIndex_ + 1) % HISTORY_SIZE;

    if (historyCount_ < HISTORY_SIZE) {
        historyCount_++;
    }
}

}  // namespace lithium::device::indi::focuser
