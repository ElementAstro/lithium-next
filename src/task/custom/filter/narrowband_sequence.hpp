#ifndef LITHIUM_TASK_FILTER_NARROWBAND_SEQUENCE_TASK_HPP
#define LITHIUM_TASK_FILTER_NARROWBAND_SEQUENCE_TASK_HPP

#include <atomic>
#include <future>
#include <map>

#include "base.hpp"

namespace lithium::task::filter {

/**
 * @enum NarrowbandFilter
 * @brief Represents different types of narrowband filters.
 */
enum class NarrowbandFilter {
    Ha,     ///< Hydrogen-alpha (656.3nm)
    OIII,   ///< Oxygen III (500.7nm)
    SII,    ///< Sulfur II (672.4nm)
    NII,    ///< Nitrogen II (658.3nm)
    Hb,     ///< Hydrogen-beta (486.1nm)
    Custom  ///< Custom narrowband filter
};

/**
 * @struct NarrowbandFilterSettings
 * @brief Settings for a single narrowband filter.
 */
struct NarrowbandFilterSettings {
    std::string name;       ///< Filter name
    NarrowbandFilter type;  ///< Filter type
    double exposure;        ///< Exposure time in seconds
    int frameCount;         ///< Number of frames to capture
    int gain;               ///< Camera gain setting
    int offset;             ///< Camera offset setting
    bool enabled;           ///< Whether this filter is enabled in sequence
};

/**
 * @struct NarrowbandSequenceSettings
 * @brief Complete settings for narrowband imaging sequence.
 */
struct NarrowbandSequenceSettings {
    std::map<NarrowbandFilter, NarrowbandFilterSettings>
        filters;                     ///< Filter settings
    bool useHOSSequence{true};       ///< Use Hubble palette (Ha, OIII, SII)
    bool useBiColorSequence{false};  ///< Use two-filter sequence
    bool interleaved{false};         ///< Interleave filters instead of batching
    int sequenceRepeats{1};          ///< Number of times to repeat the sequence
    double settlingTime{2.0};  ///< Time to wait after filter change (seconds)
};

/**
 * @class NarrowbandSequenceTask
 * @brief Task for executing narrowband imaging sequences.
 *
 * This task specializes in narrowband filter imaging, supporting common
 * narrowband filters like Ha, OIII, SII, and custom configurations.
 * It includes optimizations for long-exposure narrowband imaging and
 * supports various sequence patterns including Hubble palette (HOS).
 */
class NarrowbandSequenceTask : public BaseFilterTask {
public:
    /**
     * @brief Constructs a NarrowbandSequenceTask.
     * @param name Optional custom name for the task (defaults to
     * "NarrowbandSequence").
     */
    NarrowbandSequenceTask(const std::string& name = "NarrowbandSequence");

    /**
     * @brief Executes the narrowband sequence with the provided parameters.
     * @param params JSON object containing narrowband sequence configuration.
     *
     * Parameters:
     * - ha_exposure (number): H-alpha exposure time (default: 300.0)
     * - oiii_exposure (number): OIII exposure time (default: 300.0)
     * - sii_exposure (number): SII exposure time (default: 300.0)
     * - ha_count (number): Number of H-alpha frames (default: 10)
     * - oiii_count (number): Number of OIII frames (default: 10)
     * - sii_count (number): Number of SII frames (default: 10)
     * - gain (number): Camera gain (default: 200)
     * - offset (number): Camera offset (default: 10)
     * - use_hos (boolean): Use HOS sequence (default: true)
     * - interleaved (boolean): Use interleaved sequence (default: false)
     * - sequence_repeats (number): Number of sequence repeats (default: 1)
     * - settling_time (number): Filter settling time (default: 2.0)
     */
    void execute(const json& params) override;

    /**
     * @brief Executes narrowband sequence with specific settings.
     * @param settings NarrowbandSequenceSettings with configuration.
     * @return True if the sequence completed successfully, false otherwise.
     */
    bool executeSequence(const NarrowbandSequenceSettings& settings);

    /**
     * @brief Executes the sequence asynchronously.
     * @param settings NarrowbandSequenceSettings with configuration.
     * @return Future that resolves when the sequence completes.
     */
    std::future<bool> executeSequenceAsync(
        const NarrowbandSequenceSettings& settings);

    /**
     * @brief Adds a custom narrowband filter to the sequence.
     * @param filterName The name of the custom filter.
     * @param exposure Exposure time in seconds.
     * @param frameCount Number of frames to capture.
     * @param gain Camera gain setting.
     * @param offset Camera offset setting.
     */
    void addCustomFilter(const std::string& filterName, double exposure,
                         int frameCount, int gain = 200, int offset = 10);

    /**
     * @brief Sets up a Hubble palette sequence (Ha, OIII, SII).
     * @param haExposure H-alpha exposure time.
     * @param oiiiExposure OIII exposure time.
     * @param siiExposure SII exposure time.
     * @param frameCount Number of frames per filter.
     * @param gain Camera gain setting.
     * @param offset Camera offset setting.
     */
    void setupHubblePalette(double haExposure = 300.0,
                            double oiiiExposure = 300.0,
                            double siiExposure = 300.0, int frameCount = 10,
                            int gain = 200, int offset = 10);

    /**
     * @brief Gets the current progress of the narrowband sequence.
     * @return Progress as a percentage (0.0 to 100.0).
     */
    double getSequenceProgress() const;

    /**
     * @brief Gets detailed progress information for each filter.
     * @return Map of filter names to progress percentages.
     */
    std::map<std::string, double> getFilterProgress() const;

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
     * @brief Sets up parameter definitions specific to narrowband sequences.
     */
    void setupNarrowbandDefaults();

    /**
     * @brief Executes a sequential narrowband pattern.
     * @param settings The narrowband settings to use.
     * @return True if successful, false otherwise.
     */
    bool executeSequentialPattern(const NarrowbandSequenceSettings& settings);

    /**
     * @brief Executes an interleaved narrowband pattern.
     * @param settings The narrowband settings to use.
     * @return True if successful, false otherwise.
     */
    bool executeInterleavedPattern(const NarrowbandSequenceSettings& settings);

    /**
     * @brief Captures frames for a specific narrowband filter.
     * @param filterSettings The settings for the filter.
     * @return True if all frames were captured successfully.
     */
    bool captureNarrowbandFrames(
        const NarrowbandFilterSettings& filterSettings);

    /**
     * @brief Converts NarrowbandFilter enum to string.
     * @param filter The filter enum value.
     * @return String representation of the filter.
     */
    std::string narrowbandFilterToString(NarrowbandFilter filter) const;

    /**
     * @brief Updates the sequence progress.
     * @param completedFrames Number of completed frames.
     * @param totalFrames Total number of frames in sequence.
     */
    void updateProgress(int completedFrames, int totalFrames);

    NarrowbandSequenceSettings currentSettings_;  ///< Current sequence settings
    std::atomic<double> sequenceProgress_{0.0};   ///< Current sequence progress
    std::map<std::string, double> filterProgress_;  ///< Progress per filter
    std::atomic<bool> isPaused_{false};     ///< Whether sequence is paused
    std::atomic<bool> isCancelled_{false};  ///< Whether sequence is cancelled
    std::chrono::steady_clock::time_point
        sequenceStartTime_;   ///< Start time of sequence
    int completedFrames_{0};  ///< Number of completed frames
    int totalFrames_{0};      ///< Total frames in sequence
};

}  // namespace lithium::task::filter

#endif  // LITHIUM_TASK_FILTER_NARROWBAND_SEQUENCE_TASK_HPP