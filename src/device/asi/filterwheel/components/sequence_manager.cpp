#include "sequence_manager.hpp"
#include "position_manager.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <algorithm>

namespace lithium::device::asi::filterwheel::components {

SequenceManager::SequenceManager(std::shared_ptr<PositionManager> position_mgr)
    : position_manager_(std::move(position_mgr))
    , current_step_(0)
    , current_repeat_(0)
    , is_running_(false)
    , is_paused_(false)
    , stop_requested_(false) {

    initializeTemplates();
    createDefaultSequences();
    spdlog::info("SequenceManager initialized");
}

SequenceManager::~SequenceManager() {
    if (is_running_) {
        stopSequence();
    }
    spdlog::info("SequenceManager destroyed");
}

bool SequenceManager::createSequence(const std::string& name, const std::string& description) {
    if (name.empty()) {
        spdlog::error("Sequence name cannot be empty");
        return false;
    }

    if (sequences_.find(name) != sequences_.end()) {
        spdlog::warn("Sequence '{}' already exists", name);
        return false;
    }

    sequences_[name] = FilterSequence(name, description);
    spdlog::info("Created sequence '{}'", name);
    return true;
}

bool SequenceManager::deleteSequence(const std::string& name) {
    if (current_sequence_ == name && is_running_) {
        spdlog::error("Cannot delete currently running sequence '{}'", name);
        return false;
    }

    auto it = sequences_.find(name);
    if (it == sequences_.end()) {
        spdlog::error("Sequence '{}' not found", name);
        return false;
    }

    sequences_.erase(it);
    spdlog::info("Deleted sequence '{}'", name);
    return true;
}

bool SequenceManager::addStep(const std::string& sequence_name, const SequenceStep& step) {
    auto it = sequences_.find(sequence_name);
    if (it == sequences_.end()) {
        spdlog::error("Sequence '{}' not found", sequence_name);
        return false;
    }

    if (!isValidPosition(step.target_position)) {
        spdlog::error("Invalid position {} in sequence step", step.target_position);
        return false;
    }

    it->second.steps.push_back(step);
    spdlog::info("Added step to sequence '{}': position {}, dwell {} ms",
          sequence_name, step.target_position, step.dwell_time_ms);
    return true;
}

bool SequenceManager::removeStep(const std::string& sequence_name, int step_index) {
    auto it = sequences_.find(sequence_name);
    if (it == sequences_.end()) {
        spdlog::error("Sequence '{}' not found", sequence_name);
        return false;
    }

    if (step_index < 0 || step_index >= static_cast<int>(it->second.steps.size())) {
        spdlog::error("Invalid step index {} for sequence '{}'", step_index, sequence_name);
        return false;
    }

    it->second.steps.erase(it->second.steps.begin() + step_index);
    spdlog::info("Removed step {} from sequence '{}'", step_index, sequence_name);
    return true;
}

bool SequenceManager::clearSequence(const std::string& sequence_name) {
    auto it = sequences_.find(sequence_name);
    if (it == sequences_.end()) {
        spdlog::error("Sequence '{}' not found", sequence_name);
        return false;
    }

    it->second.steps.clear();
    spdlog::info("Cleared all steps from sequence '{}'", sequence_name);
    return true;
}

std::vector<std::string> SequenceManager::getSequenceNames() const {
    std::vector<std::string> names;
    for (const auto& [name, sequence] : sequences_) {
        names.push_back(name);
    }
    return names;
}

bool SequenceManager::sequenceExists(const std::string& name) const {
    return sequences_.find(name) != sequences_.end();
}

bool SequenceManager::setSequenceRepeat(const std::string& name, bool repeat, int count) {
    auto it = sequences_.find(name);
    if (it == sequences_.end()) {
        spdlog::error("Sequence '{}' not found", name);
        return false;
    }

    it->second.repeat = repeat;
    it->second.repeat_count = std::max(1, count);
    spdlog::info("Set sequence '{}' repeat: {} (count: {})",
          name, repeat ? "enabled" : "disabled", it->second.repeat_count);
    return true;
}

bool SequenceManager::setSequenceDelay(const std::string& name, int delay_ms) {
    auto it = sequences_.find(name);
    if (it == sequences_.end()) {
        spdlog::error("Sequence '{}' not found", name);
        return false;
    }

    it->second.delay_between_repeats_ms = std::max(0, delay_ms);
    spdlog::info("Set sequence '{}' repeat delay: {} ms", name, delay_ms);
    return true;
}

std::optional<FilterSequence> SequenceManager::getSequence(const std::string& name) const {
    auto it = sequences_.find(name);
    if (it != sequences_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool SequenceManager::createLinearSequence(const std::string& name, int start_pos, int end_pos, int dwell_time_ms) {
    if (!createSequence(name, "Linear sequence from " + std::to_string(start_pos) + " to " + std::to_string(end_pos))) {
        return false;
    }

    int step = (start_pos <= end_pos) ? 1 : -1;
    for (int pos = start_pos; pos != end_pos + step; pos += step) {
        if (!isValidPosition(pos)) {
            spdlog::error("Invalid position {} in linear sequence", pos);
            deleteSequence(name);
            return false;
        }

        SequenceStep seq_step(pos, dwell_time_ms, "Position " + std::to_string(pos));
        addStep(name, seq_step);
    }

    spdlog::info("Created linear sequence '{}' from {} to {}", name, start_pos, end_pos);
    return true;
}

bool SequenceManager::createCustomSequence(const std::string& name, const std::vector<int>& positions, int dwell_time_ms) {
    if (!createSequence(name, "Custom sequence with " + std::to_string(positions.size()) + " positions")) {
        return false;
    }

    for (size_t i = 0; i < positions.size(); ++i) {
        int pos = positions[i];
        if (!isValidPosition(pos)) {
            spdlog::error("Invalid position {} in custom sequence", pos);
            deleteSequence(name);
            return false;
        }

        SequenceStep seq_step(pos, dwell_time_ms, "Step " + std::to_string(i + 1) + " - Position " + std::to_string(pos));
        addStep(name, seq_step);
    }

    spdlog::info("Created custom sequence '{}' with {} positions", name, positions.size());
    return true;
}

bool SequenceManager::createCalibrationSequence(const std::string& name) {
    if (!position_manager_) {
        spdlog::error("Position manager not available for calibration sequence");
        return false;
    }

    if (!createSequence(name, "Calibration sequence - tests all positions")) {
        return false;
    }

    int slot_count = position_manager_->getFilterCount();
    for (int i = 0; i < slot_count; ++i) {
        SequenceStep seq_step(i, 2000, "Calibration test - Position " + std::to_string(i));
        addStep(name, seq_step);
    }

    spdlog::info("Created calibration sequence '{}' with {} positions", name, slot_count);
    return true;
}

bool SequenceManager::startSequence(const std::string& name) {
    if (is_running_) {
        spdlog::error("Another sequence is already running");
        return false;
    }

    auto it = sequences_.find(name);
    if (it == sequences_.end()) {
        spdlog::error("Sequence '{}' not found", name);
        return false;
    }

    if (it->second.steps.empty()) {
        spdlog::error("Sequence '{}' has no steps", name);
        return false;
    }

    if (!validateSequence(name)) {
        spdlog::error("Sequence '{}' validation failed", name);
        return false;
    }

    current_sequence_ = name;
    current_step_ = 0;
    current_repeat_ = 0;
    is_running_ = true;
    is_paused_ = false;
    stop_requested_ = false;
    sequence_start_time_ = std::chrono::steady_clock::now();

    // Start execution in background thread
    execution_future_ = std::async(std::launch::async, [this]() {
        executeSequenceAsync();
    });

    spdlog::info("Started sequence '{}'", name);
    notifySequenceEvent("sequence_started", 0, -1);
    return true;
}

bool SequenceManager::pauseSequence() {
    if (!is_running_ || is_paused_) {
        return false;
    }

    is_paused_ = true;
    spdlog::info("Paused sequence '{}'", current_sequence_);
    notifySequenceEvent("sequence_paused", current_step_, -1);
    return true;
}

bool SequenceManager::resumeSequence() {
    if (!is_running_ || !is_paused_) {
        return false;
    }

    is_paused_ = false;
    spdlog::info("Resumed sequence '{}'", current_sequence_);
    notifySequenceEvent("sequence_resumed", current_step_, -1);
    return true;
}

bool SequenceManager::stopSequence() {
    if (!is_running_) {
        return false;
    }

    stop_requested_ = true;
    is_paused_ = false;

    // Wait for execution thread to finish
    if (execution_future_.valid()) {
        execution_future_.wait();
    }

    spdlog::info("Stopped sequence '{}'", current_sequence_);
    notifySequenceEvent("sequence_stopped", current_step_, -1);

    resetExecutionState();
    return true;
}

bool SequenceManager::isSequenceRunning() const {
    return is_running_;
}

bool SequenceManager::isSequencePaused() const {
    return is_paused_;
}

std::string SequenceManager::getCurrentSequenceName() const {
    return current_sequence_;
}

int SequenceManager::getCurrentStepIndex() const {
    return current_step_;
}

int SequenceManager::getCurrentRepeatCount() const {
    return current_repeat_;
}

int SequenceManager::getTotalSteps() const {
    if (current_sequence_.empty()) {
        return 0;
    }

    auto it = sequences_.find(current_sequence_);
    if (it != sequences_.end()) {
        const FilterSequence& seq = it->second;
        int total_steps = static_cast<int>(seq.steps.size());
        if (seq.repeat) {
            total_steps *= seq.repeat_count;
        }
        return total_steps;
    }
    return 0;
}

double SequenceManager::getSequenceProgress() const {
    int total = getTotalSteps();
    if (total == 0) {
        return 0.0;
    }

    int completed = current_repeat_ * static_cast<int>(sequences_.at(current_sequence_).steps.size()) + current_step_;
    return static_cast<double>(completed) / static_cast<double>(total);
}

std::chrono::milliseconds SequenceManager::getElapsedTime() const {
    if (!is_running_) {
        return std::chrono::milliseconds::zero();
    }

    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - sequence_start_time_);
}

std::chrono::milliseconds SequenceManager::getEstimatedRemainingTime() const {
    if (!is_running_ || current_sequence_.empty()) {
        return std::chrono::milliseconds::zero();
    }

    auto it = sequences_.find(current_sequence_);
    if (it == sequences_.end()) {
        return std::chrono::milliseconds::zero();
    }

    const FilterSequence& seq = it->second;

    // Calculate remaining time in current repeat
    std::chrono::milliseconds remaining_current_repeat{0};
    for (size_t i = current_step_; i < seq.steps.size(); ++i) {
        remaining_current_repeat += std::chrono::milliseconds(seq.steps[i].dwell_time_ms + 1000); // +1s for movement
    }

    // Calculate time for remaining repeats
    std::chrono::milliseconds remaining_repeats{0};
    if (seq.repeat && current_repeat_ < seq.repeat_count - 1) {
        int remaining_repeat_count = seq.repeat_count - current_repeat_ - 1;
        std::chrono::milliseconds sequence_time = calculateSequenceTime(seq);
        remaining_repeats = sequence_time * remaining_repeat_count;
        remaining_repeats += std::chrono::milliseconds(seq.delay_between_repeats_ms * remaining_repeat_count);
    }

    return remaining_current_repeat + remaining_repeats;
}

void SequenceManager::setSequenceCallback(SequenceCallback callback) {
    sequence_callback_ = std::move(callback);
}

void SequenceManager::clearSequenceCallback() {
    sequence_callback_ = nullptr;
}

bool SequenceManager::validateSequence(const std::string& name) const {
    auto it = sequences_.find(name);
    if (it == sequences_.end()) {
        return false;
    }

    const FilterSequence& seq = it->second;

    // Check if sequence has steps
    if (seq.steps.empty()) {
        return false;
    }

    // Validate all positions
    for (const auto& step : seq.steps) {
        if (!isValidPosition(step.target_position)) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> SequenceManager::getSequenceValidationErrors(const std::string& name) const {
    std::vector<std::string> errors;

    auto it = sequences_.find(name);
    if (it == sequences_.end()) {
        errors.push_back("Sequence not found");
        return errors;
    }

    const FilterSequence& seq = it->second;

    if (seq.steps.empty()) {
        errors.push_back("Sequence has no steps");
    }

    for (size_t i = 0; i < seq.steps.size(); ++i) {
        const auto& step = seq.steps[i];
        if (!isValidPosition(step.target_position)) {
            errors.push_back("Step " + std::to_string(i) + ": Invalid position " + std::to_string(step.target_position));
        }
        if (step.dwell_time_ms < 0) {
            errors.push_back("Step " + std::to_string(i) + ": Negative dwell time");
        }
    }

    return errors;
}

void SequenceManager::createDefaultSequences() {
    // Create a simple test sequence
    createSequence("test", "Simple test sequence");
    addStep("test", SequenceStep(0, 1000, "Test position 0"));
    addStep("test", SequenceStep(1, 1000, "Test position 1"));

    // Create a full scan sequence if position manager is available
    if (position_manager_) {
        createCalibrationSequence("full_scan");
    }
}

void SequenceManager::executeSequenceAsync() {
    auto it = sequences_.find(current_sequence_);
    if (it == sequences_.end()) {
        resetExecutionState();
        return;
    }

    const FilterSequence& sequence = it->second;
    int repeat_count = sequence.repeat ? sequence.repeat_count : 1;

    try {
        for (current_repeat_ = 0; current_repeat_ < repeat_count && !stop_requested_; ++current_repeat_) {
            spdlog::info("Starting repeat {}/{} of sequence '{}'",
                  current_repeat_ + 1, repeat_count, current_sequence_);

            for (current_step_ = 0; current_step_ < static_cast<int>(sequence.steps.size()) && !stop_requested_; ++current_step_) {
                // Wait if paused
                while (is_paused_ && !stop_requested_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                if (stop_requested_) {
                    break;
                }

                const SequenceStep& step = sequence.steps[current_step_];
                step_start_time_ = std::chrono::steady_clock::now();

                spdlog::info("Executing step {}/{}: position {}, dwell {} ms",
                      current_step_ + 1, sequence.steps.size(), step.target_position, step.dwell_time_ms);

                notifySequenceEvent("step_started", current_step_, step.target_position);

                if (!executeStep(step)) {
                    spdlog::error("Failed to execute step {}", current_step_);
                    notifySequenceEvent("step_failed", current_step_, step.target_position);
                    break;
                }

                notifySequenceEvent("step_completed", current_step_, step.target_position);
            }

            // Delay between repeats
            if (current_repeat_ < repeat_count - 1 && sequence.delay_between_repeats_ms > 0 && !stop_requested_) {
                spdlog::info("Waiting {} ms before next repeat", sequence.delay_between_repeats_ms);
                std::this_thread::sleep_for(std::chrono::milliseconds(sequence.delay_between_repeats_ms));
            }
        }

        if (!stop_requested_) {
            spdlog::info("Sequence '{}' completed successfully", current_sequence_);
            notifySequenceEvent("sequence_completed", -1, -1);
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in sequence execution: {}", e.what());
        notifySequenceEvent("sequence_error", current_step_, -1);
    }

    resetExecutionState();
}

bool SequenceManager::executeStep(const SequenceStep& step) {
    if (!position_manager_) {
        spdlog::error("Position manager not available");
        return false;
    }

    // Move to target position
    if (!position_manager_->setPosition(step.target_position)) {
        spdlog::error("Failed to move to position {}", step.target_position);
        return false;
    }

    // Wait for movement to complete
    if (!position_manager_->waitForMovement(30000)) { // 30 second timeout
        spdlog::error("Movement timeout for position {}", step.target_position);
        return false;
    }

    // Dwell at position
    if (step.dwell_time_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(step.dwell_time_ms));
    }

    return true;
}

void SequenceManager::notifySequenceEvent(const std::string& event, int step_index, int position) {
    if (sequence_callback_) {
        try {
            sequence_callback_(event, step_index, position);
        } catch (const std::exception& e) {
            spdlog::error("Exception in sequence callback: {}", e.what());
        }
    }
}

bool SequenceManager::isValidPosition(int position) const {
    if (!position_manager_) {
        return position >= 0 && position < 32; // Default assumption
    }
    return position_manager_->isValidPosition(position);
}

std::chrono::milliseconds SequenceManager::calculateSequenceTime(const FilterSequence& sequence) const {
    std::chrono::milliseconds total_time{0};

    for (const auto& step : sequence.steps) {
        total_time += std::chrono::milliseconds(step.dwell_time_ms + 1000); // +1s for movement
    }

    return total_time;
}

void SequenceManager::resetExecutionState() {
    is_running_ = false;
    is_paused_ = false;
    stop_requested_ = false;
    current_sequence_.clear();
    current_step_ = 0;
    current_repeat_ = 0;
}

void SequenceManager::initializeTemplates() {
    // Initialize common sequence templates
    // Templates can be loaded from files or created programmatically
    spdlog::info("Sequence templates initialized");
}

bool SequenceManager::saveSequenceTemplate(const std::string& sequence_name, const std::string& template_name) {
    auto it = sequences_.find(sequence_name);
    if (it == sequences_.end()) {
        spdlog::error("Sequence '{}' not found", sequence_name);
        return false;
    }

    sequence_templates_[template_name] = it->second;
    sequence_templates_[template_name].name = template_name;
    spdlog::info("Saved sequence template '{}'", template_name);
    return true;
}

bool SequenceManager::loadSequenceTemplate(const std::string& template_name, const std::string& new_sequence_name) {
    auto it = sequence_templates_.find(template_name);
    if (it == sequence_templates_.end()) {
        spdlog::error("Sequence template '{}' not found", template_name);
        return false;
    }

    sequences_[new_sequence_name] = it->second;
    sequences_[new_sequence_name].name = new_sequence_name;
    spdlog::info("Loaded sequence template '{}' as '{}'", template_name, new_sequence_name);
    return true;
}

std::vector<std::string> SequenceManager::getAvailableTemplates() const {
    std::vector<std::string> templates;
    for (const auto& [name, sequence] : sequence_templates_) {
        templates.push_back(name);
    }
    return templates;
}

} // namespace lithium::device::asi::filterwheel::components
