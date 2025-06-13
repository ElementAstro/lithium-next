#ifndef LITHIUM_TASK_FILTER_LRGB_SEQUENCE_TASK_HPP
#define LITHIUM_TASK_FILTER_LRGB_SEQUENCE_TASK_HPP

#include <future>

#include "base.hpp"

namespace lithium::task::filter {

/**
 * @struct LRGBSettings
 * @brief Settings for LRGB (Luminance, Red, Green, Blue) imaging sequence.
 */
struct LRGBSettings {
    double luminanceExposure{60.0};  ///< Luminance exposure time in seconds
    double redExposure{60.0};        ///< Red exposure time in seconds
    double greenExposure{60.0};      ///< Green exposure time in seconds
    double blueExposure{60.0};       ///< Blue exposure time in seconds

    int luminanceCount{10};  ///< Number of luminance frames
    int redCount{5};         ///< Number of red frames
    int greenCount{5};       ///< Number of green frames
    int blueCount{5};        ///< Number of blue frames

    int gain{100};   ///< Camera gain setting
    int offset{10};  ///< Camera offset setting

    bool startWithLuminance{true};  ///< Whether to start with luminance filter
    bool interleaved{false};        ///< Whether to interleave LRGB sequence
};

/**
 * @class LRGBSequenceTask
 * @brief Task for executing LRGB (Luminance, Red, Green, Blue) imaging
 * sequences.
 *
 * This task manages the complete LRGB imaging workflow, including filter
 * changes, exposure sequences, and progress monitoring. It supports both
 * sequential and interleaved imaging patterns for optimal results.
 */
class LRGBSequenceTask : public BaseFilterTask {
public:
    /**
     * @brief Constructs an LRGBSequenceTask.
     * @param name Optional custom name for the task (defaults to
     * "LRGBSequence").
     */
    LRGBSequenceTask(const std::string& name = "LRGBSequence");

    /**
     * @brief Executes the LRGB sequence with the provided parameters.
     * @param params JSON object containing LRGB sequence configuration.
     *
     * Parameters:
     * - luminance_exposure (number): Luminance exposure time (default: 60.0)
     * - red_exposure (number): Red exposure time (default: 60.0)
     * - green_exposure (number): Green exposure time (default: 60.0)
     * - blue_exposure (number): Blue exposure time (default: 60.0)
     * - luminance_count (number): Number of luminance frames (default: 10)
     * - red_count (number): Number of red frames (default: 5)
     * - green_count (number): Number of green frames (default: 5)
     * - blue_count (number): Number of blue frames (default: 5)
     * - gain (number): Camera gain (default: 100)
     * - offset (number): Camera offset (default: 10)
     * - start_with_luminance (boolean): Start with luminance (default: true)
     * - interleaved (boolean): Use interleaved sequence (default: false)
     */
    void execute(const json& params) override;

    /**
     * @brief Executes LRGB sequence with specific settings.
     * @param settings LRGBSettings structure with sequence configuration.
     * @return True if the sequence completed successfully, false otherwise.
     */
    bool executeSequence(const LRGBSettings& settings);

    /**
     * @brief Executes the sequence asynchronously.
     * @param settings LRGBSettings structure with sequence configuration.
     * @return Future that resolves when the sequence completes.
     */
    std::future<bool> executeSequenceAsync(const LRGBSettings& settings);

    /**
     * @brief Gets the current progress of the LRGB sequence.
     * @return Progress as a percentage (0.0 to 100.0).
     */
    double getSequenceProgress() const;

    /**
     * @brief Gets the estimated remaining time for the sequence.
     * @return Estimated remaining time in seconds.
     */
    std::chrono::seconds getEstimatedRemainingTime() const;

    /**
     * @brief Pauses the current sequence.
     */
    void pauseSequence();

    /**
     * @brief Resumes a paused sequence.
     */
    void resumeSequence();

    /**
     * @brief Cancels the current sequence.
     */
    void cancelSequence();

private:
    /**
     * @brief Sets up parameter definitions specific to LRGB sequences.
     */
    void setupLRGBDefaults();

    /**
     * @brief Executes a sequential LRGB pattern (L->R->G->B).
     * @param settings The LRGB settings to use.
     * @return True if successful, false otherwise.
     */
    bool executeSequentialPattern(const LRGBSettings& settings);

    /**
     * @brief Executes an interleaved LRGB pattern.
     * @param settings The LRGB settings to use.
     * @return True if successful, false otherwise.
     */
    bool executeInterleavedPattern(const LRGBSettings& settings);

    /**
     * @brief Captures frames for a specific filter.
     * @param filterName The name of the filter to use.
     * @param exposure Exposure time in seconds.
     * @param count Number of frames to capture.
     * @param gain Camera gain setting.
     * @param offset Camera offset setting.
     * @return True if all frames were captured successfully.
     */
    bool captureFilterFrames(const std::string& filterName, double exposure,
                             int count, int gain, int offset);

    /**
     * @brief Updates the sequence progress.
     * @param completedFrames Number of completed frames.
     * @param totalFrames Total number of frames in sequence.
     */
    void updateProgress(int completedFrames, int totalFrames);

    LRGBSettings currentSettings_;               ///< Current sequence settings
    std::atomic<double> sequenceProgress_{0.0};  ///< Current sequence progress
    std::atomic<bool> isPaused_{false};          ///< Whether sequence is paused
    std::atomic<bool> isCancelled_{false};  ///< Whether sequence is cancelled
    std::chrono::steady_clock::time_point
        sequenceStartTime_;   ///< Start time of sequence
    int completedFrames_{0};  ///< Number of completed frames
    int totalFrames_{0};      ///< Total frames in sequence
};

}  // namespace lithium::task::filter

#endif  // LITHIUM_TASK_FILTER_LRGB_SEQUENCE_TASK_HPP