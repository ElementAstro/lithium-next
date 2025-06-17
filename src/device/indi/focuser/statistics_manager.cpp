#include "statistics_manager.hpp"
#include <numeric>

namespace lithium::device::indi::focuser {

bool StatisticsManager::initialize(FocuserState& state) {
    state_ = &state;
    state_->logger_->info("{}: Initializing statistics manager", getComponentName());
    
    // Initialize history arrays
    stepHistory_.fill(0);
    durationHistory_.fill(0);
    historyIndex_ = 0;
    historyCount_ = 0;
    
    return true;
}

void StatisticsManager::cleanup() {
    if (state_) {
        state_->logger_->info("{}: Cleaning up statistics manager", getComponentName());
    }
    state_ = nullptr;
}

uint64_t StatisticsManager::getTotalSteps() const {
    if (!state_) {
        return 0;
    }
    return state_->totalSteps_.load();
}

int StatisticsManager::getLastMoveSteps() const {
    if (!state_) {
        return 0;
    }
    return state_->lastMoveSteps_.load();
}

int StatisticsManager::getLastMoveDuration() const {
    if (!state_) {
        return 0;
    }
    return state_->lastMoveDuration_.load();
}

bool StatisticsManager::resetTotalSteps() {
    if (!state_) {
        return false;
    }

    state_->totalSteps_ = 0;
    totalMoves_ = 0;
    historyIndex_ = 0;
    historyCount_ = 0;
    stepHistory_.fill(0);
    durationHistory_.fill(0);
    
    state_->logger_->info("Reset total steps and move counters");
    return true;
}

void StatisticsManager::recordMovement(int steps, int durationMs) {
    if (!state_) {
        return;
    }

    state_->lastMoveSteps_ = steps;
    state_->totalSteps_ += std::abs(steps);
    ++totalMoves_;
    
    if (durationMs > 0) {
        state_->lastMoveDuration_ = durationMs;
    }
    
    updateHistory(std::abs(steps), durationMs);
    
    state_->logger_->debug("Recorded movement: {} steps, {} ms", steps, durationMs);
}

double StatisticsManager::getAverageStepsPerMove() const {
    if (totalMoves_ == 0) {
        return 0.0;
    }
    
    if (historyCount_ > 0) {
        // Use history for more recent average
        size_t count = std::min(historyCount_, HISTORY_SIZE);
        int total = std::accumulate(stepHistory_.begin(), stepHistory_.begin() + count, 0);
        return static_cast<double>(total) / count;
    }
    
    return static_cast<double>(getTotalSteps()) / totalMoves_;
}

double StatisticsManager::getAverageMoveDuration() const {
    if (historyCount_ == 0) {
        return 0.0;
    }
    
    size_t count = std::min(historyCount_, HISTORY_SIZE);
    int total = std::accumulate(durationHistory_.begin(), durationHistory_.begin() + count, 0);
    return static_cast<double>(total) / count;
}

uint64_t StatisticsManager::getTotalMoves() const {
    return totalMoves_;
}

void StatisticsManager::startSession() {
    sessionStart_ = std::chrono::steady_clock::now();
    sessionStartSteps_ = getTotalSteps();
    sessionStartMoves_ = totalMoves_;
    
    if (state_) {
        state_->logger_->info("Started new focuser session");
    }
}

void StatisticsManager::endSession() {
    sessionEnd_ = std::chrono::steady_clock::now();
    
    if (state_) {
        auto duration = getSessionDuration();
        auto steps = getSessionSteps();
        auto moves = getSessionMoves();
        
        state_->logger_->info("Ended focuser session - Duration: {}ms, Steps: {}, Moves: {}", 
                             duration.count(), steps, moves);
    }
}

uint64_t StatisticsManager::getSessionSteps() const {
    return getTotalSteps() - sessionStartSteps_;
}

uint64_t StatisticsManager::getSessionMoves() const {
    return totalMoves_ - sessionStartMoves_;
}

std::chrono::milliseconds StatisticsManager::getSessionDuration() const {
    auto end = (sessionEnd_.time_since_epoch().count() > 0) ? 
               sessionEnd_ : std::chrono::steady_clock::now();
    
    if (sessionStart_.time_since_epoch().count() == 0) {
        return std::chrono::milliseconds(0);
    }
    
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - sessionStart_);
}

void StatisticsManager::updateHistory(int steps, int duration) {
    stepHistory_[historyIndex_] = steps;
    durationHistory_[historyIndex_] = duration;
    
    historyIndex_ = (historyIndex_ + 1) % HISTORY_SIZE;
    if (historyCount_ < HISTORY_SIZE) {
        ++historyCount_;
    }
}

} // namespace lithium::device::indi::focuser
