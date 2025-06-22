#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <future>

namespace lithium::device::asi::filterwheel::components {

class PositionManager;

/**
 * @brief Represents a single step in a filter sequence
 */
struct SequenceStep {
    int target_position;
    int dwell_time_ms;        // Time to wait at this position
    std::string description;
    
    SequenceStep(int pos = 0, int dwell = 0, const std::string& desc = "")
        : target_position(pos), dwell_time_ms(dwell), description(desc) {}
};

/**
 * @brief Represents a complete filter sequence
 */
struct FilterSequence {
    std::string name;
    std::string description;
    std::vector<SequenceStep> steps;
    bool repeat;
    int repeat_count;
    int delay_between_repeats_ms;
    
    FilterSequence(const std::string& seq_name = "", const std::string& desc = "")
        : name(seq_name), description(desc), repeat(false), repeat_count(1), delay_between_repeats_ms(0) {}
};

/**
 * @brief Callback function type for sequence events
 */
using SequenceCallback = std::function<void(const std::string& event, int step_index, int position)>;

/**
 * @brief Manages automated filter sequences including creation, execution, and monitoring
 */
class SequenceManager {
public:
    explicit SequenceManager(std::shared_ptr<PositionManager> position_mgr);
    ~SequenceManager();

    // Sequence management
    bool createSequence(const std::string& name, const std::string& description = "");
    bool deleteSequence(const std::string& name);
    bool addStep(const std::string& sequence_name, const SequenceStep& step);
    bool removeStep(const std::string& sequence_name, int step_index);
    bool clearSequence(const std::string& sequence_name);
    std::vector<std::string> getSequenceNames() const;
    bool sequenceExists(const std::string& name) const;

    // Sequence configuration
    bool setSequenceRepeat(const std::string& name, bool repeat, int count = 1);
    bool setSequenceDelay(const std::string& name, int delay_ms);
    std::optional<FilterSequence> getSequence(const std::string& name) const;
    
    // Quick sequence builders
    bool createLinearSequence(const std::string& name, int start_pos, int end_pos, int dwell_time_ms = 1000);
    bool createCustomSequence(const std::string& name, const std::vector<int>& positions, int dwell_time_ms = 1000);
    bool createCalibrationSequence(const std::string& name);
    
    // Execution control
    bool startSequence(const std::string& name);
    bool pauseSequence();
    bool resumeSequence();
    bool stopSequence();
    bool isSequenceRunning() const;
    bool isSequencePaused() const;
    
    // Monitoring and status
    std::string getCurrentSequenceName() const;
    int getCurrentStepIndex() const;
    int getCurrentRepeatCount() const;
    int getTotalSteps() const;
    double getSequenceProgress() const; // 0.0 to 1.0
    std::chrono::milliseconds getElapsedTime() const;
    std::chrono::milliseconds getEstimatedRemainingTime() const;
    
    // Event handling
    void setSequenceCallback(SequenceCallback callback);
    void clearSequenceCallback();
    
    // Sequence validation
    bool validateSequence(const std::string& name) const;
    std::vector<std::string> getSequenceValidationErrors(const std::string& name) const;
    
    // Presets and templates
    void createDefaultSequences();
    bool saveSequenceTemplate(const std::string& sequence_name, const std::string& template_name);
    bool loadSequenceTemplate(const std::string& template_name, const std::string& new_sequence_name);
    std::vector<std::string> getAvailableTemplates() const;

private:
    std::shared_ptr<PositionManager> position_manager_;
    std::unordered_map<std::string, FilterSequence> sequences_;
    
    // Execution state
    std::string current_sequence_;
    int current_step_;
    int current_repeat_;
    bool is_running_;
    bool is_paused_;
    std::chrono::steady_clock::time_point sequence_start_time_;
    std::chrono::steady_clock::time_point step_start_time_;
    
    // Async execution
    std::future<void> execution_future_;
    std::atomic<bool> stop_requested_;
    
    // Event callback
    SequenceCallback sequence_callback_;
    
    // Helper methods
    void executeSequenceAsync();
    bool executeStep(const SequenceStep& step);
    void notifySequenceEvent(const std::string& event, int step_index = -1, int position = -1);
    bool isValidPosition(int position) const;
    std::chrono::milliseconds calculateSequenceTime(const FilterSequence& sequence) const;
    void resetExecutionState();
    
    // Template management
    std::unordered_map<std::string, FilterSequence> sequence_templates_;
    void initializeTemplates();
};

} // namespace lithium::device::asi::filterwheel::components
