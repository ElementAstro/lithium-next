#ifndef LITHIUM_INDI_FOCUSER_STATISTICS_MANAGER_HPP
#define LITHIUM_INDI_FOCUSER_STATISTICS_MANAGER_HPP

#include <array>
#include <chrono>
#include "types.hpp"

namespace lithium::device::indi::focuser {

/**
 * @brief Manages focuser movement statistics and tracking.
 *
 * This class provides interfaces for tracking, retrieving, and managing
 * statistics related to focuser movement, including total steps, move
 * durations, averages, and session-based statistics. It maintains a history
 * buffer for moving averages and supports session-based tracking for advanced
 * analysis.
 */
class StatisticsManager : public IFocuserComponent {
public:
    /**
     * @brief Default constructor.
     */
    StatisticsManager() = default;
    /**
     * @brief Virtual destructor.
     */
    ~StatisticsManager() override = default;

    /**
     * @brief Initialize the statistics manager with the shared focuser state.
     * @param state Reference to the shared FocuserState structure.
     * @return true if initialization was successful, false otherwise.
     */
    bool initialize(FocuserState& state) override;

    /**
     * @brief Cleanup resources and detach from the focuser state.
     */
    void cleanup() override;

    /**
     * @brief Get the component's name for logging and identification.
     * @return Name of the component.
     */
    std::string getComponentName() const override {
        return "StatisticsManager";
    }

    // Statistics retrieval

    /**
     * @brief Get the total number of steps moved by the focuser.
     * @return Total steps as a 64-bit unsigned integer.
     */
    uint64_t getTotalSteps() const;

    /**
     * @brief Get the number of steps moved in the last move operation.
     * @return Number of steps in the last move.
     */
    int getLastMoveSteps() const;

    /**
     * @brief Get the duration of the last move operation in milliseconds.
     * @return Duration in milliseconds.
     */
    int getLastMoveDuration() const;

    // Statistics management

    /**
     * @brief Reset the total steps counter to zero.
     * @return true if reset was successful, false otherwise.
     */
    bool resetTotalSteps();

    /**
     * @brief Record a movement event with the given number of steps and
     * duration.
     * @param steps Number of steps moved.
     * @param durationMs Duration of the move in milliseconds (default: 0).
     */
    void recordMovement(int steps, int durationMs = 0);

    // Advanced statistics

    /**
     * @brief Get the average number of steps per move over the history buffer.
     * @return Average steps per move as a double.
     */
    double getAverageStepsPerMove() const;

    /**
     * @brief Get the average move duration over the history buffer.
     * @return Average move duration in milliseconds as a double.
     */
    double getAverageMoveDuration() const;

    /**
     * @brief Get the total number of move operations performed.
     * @return Total number of moves as a 64-bit unsigned integer.
     */
    uint64_t getTotalMoves() const;

    // Session statistics

    /**
     * @brief Start a new statistics session, recording the current state.
     */
    void startSession();

    /**
     * @brief End the current statistics session, recording the end time.
     */
    void endSession();

    /**
     * @brief Get the total number of steps moved during the current session.
     * @return Number of steps moved in the session.
     */
    uint64_t getSessionSteps() const;

    /**
     * @brief Get the total number of moves performed during the current
     * session.
     * @return Number of moves in the session.
     */
    uint64_t getSessionMoves() const;

    /**
     * @brief Get the duration of the current session.
     * @return Session duration as a std::chrono::milliseconds object.
     */
    std::chrono::milliseconds getSessionDuration() const;

private:
    /**
     * @brief Pointer to the shared focuser state structure.
     */
    FocuserState* state_{nullptr};

    // Extended statistics
    /**
     * @brief Total number of move operations performed.
     */
    uint64_t totalMoves_{0};
    /**
     * @brief Number of steps at the start of the current session.
     */
    uint64_t sessionStartSteps_{0};
    /**
     * @brief Number of moves at the start of the current session.
     */
    uint64_t sessionStartMoves_{0};
    /**
     * @brief Start time of the current session.
     */
    std::chrono::steady_clock::time_point sessionStart_;
    /**
     * @brief End time of the current session.
     */
    std::chrono::steady_clock::time_point sessionEnd_;

    // Moving averages
    /**
     * @brief Size of the history buffer for moving averages.
     */
    static constexpr size_t HISTORY_SIZE = 100;
    /**
     * @brief Circular buffer storing the number of steps for recent moves.
     */
    std::array<int, HISTORY_SIZE> stepHistory_{};
    /**
     * @brief Circular buffer storing the duration (ms) for recent moves.
     */
    std::array<int, HISTORY_SIZE> durationHistory_{};
    /**
     * @brief Current index in the history buffer.
     */
    size_t historyIndex_{0};
    /**
     * @brief Number of valid entries in the history buffer.
     */
    size_t historyCount_{0};

    /**
     * @brief Update the history buffers with a new move event.
     * @param steps Number of steps moved.
     * @param duration Duration of the move in milliseconds.
     */
    void updateHistory(int steps, int duration);
};

}  // namespace lithium::device::indi::focuser

#endif  // LITHIUM_INDI_FOCUSER_STATISTICS_MANAGER_HPP
