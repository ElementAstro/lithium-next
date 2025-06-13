#include "narrowband_sequence.hpp"
#include "change.hpp"

#include <algorithm>
#include <future>

#include "spdlog/spdlog.h"

namespace lithium::task::filter {

NarrowbandSequenceTask::NarrowbandSequenceTask(const std::string& name)
    : BaseFilterTask(name) {
    setupNarrowbandDefaults();
}

void NarrowbandSequenceTask::setupNarrowbandDefaults() {
    // Narrowband-specific parameters
    addParamDefinition("ha_exposure", "number", false, 300.0,
                       "H-alpha exposure time in seconds");
    addParamDefinition("oiii_exposure", "number", false, 300.0,
                       "OIII exposure time in seconds");
    addParamDefinition("sii_exposure", "number", false, 300.0,
                       "SII exposure time in seconds");
    addParamDefinition("nii_exposure", "number", false, 300.0,
                       "NII exposure time in seconds");
    addParamDefinition("hb_exposure", "number", false, 300.0,
                       "H-beta exposure time in seconds");

    addParamDefinition("ha_count", "number", false, 10,
                       "Number of H-alpha frames");
    addParamDefinition("oiii_count", "number", false, 10,
                       "Number of OIII frames");
    addParamDefinition("sii_count", "number", false, 10,
                       "Number of SII frames");
    addParamDefinition("nii_count", "number", false, 0, "Number of NII frames");
    addParamDefinition("hb_count", "number", false, 0,
                       "Number of H-beta frames");

    addParamDefinition("gain", "number", false, 200, "Camera gain setting");
    addParamDefinition("offset", "number", false, 10, "Camera offset setting");
    addParamDefinition("use_hos", "boolean", false, true,
                       "Use HOS (Hubble) sequence");
    addParamDefinition("use_bicolor", "boolean", false, false,
                       "Use two-filter sequence");
    addParamDefinition("interleaved", "boolean", false, false,
                       "Use interleaved pattern");
    addParamDefinition("sequence_repeats", "number", false, 1,
                       "Number of sequence repeats");
    addParamDefinition("settling_time", "number", false, 2.0,
                       "Filter settling time in seconds");

    setTaskType("narrowband_sequence");
    setTimeout(std::chrono::hours(8));  // 8 hours for long narrowband sequences
    setPriority(6);

    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("Narrowband sequence task exception: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Narrowband sequence exception: " +
                        std::string(e.what()));
        isCancelled_ = true;
    });
}

void NarrowbandSequenceTask::execute(const json& params) {
    addHistoryEntry("Starting narrowband sequence");

    try {
        validateFilterParams(params);

        // Parse parameters into settings structure
        NarrowbandSequenceSettings settings;

        // H-alpha settings
        if (params.value("ha_count", 0) > 0) {
            NarrowbandFilterSettings haSettings;
            haSettings.name = "Ha";
            haSettings.type = NarrowbandFilter::Ha;
            haSettings.exposure = params.value("ha_exposure", 300.0);
            haSettings.frameCount = params.value("ha_count", 10);
            haSettings.gain = params.value("gain", 200);
            haSettings.offset = params.value("offset", 10);
            haSettings.enabled = true;
            settings.filters[NarrowbandFilter::Ha] = haSettings;
        }

        // OIII settings
        if (params.value("oiii_count", 0) > 0) {
            NarrowbandFilterSettings oiiiSettings;
            oiiiSettings.name = "OIII";
            oiiiSettings.type = NarrowbandFilter::OIII;
            oiiiSettings.exposure = params.value("oiii_exposure", 300.0);
            oiiiSettings.frameCount = params.value("oiii_count", 10);
            oiiiSettings.gain = params.value("gain", 200);
            oiiiSettings.offset = params.value("offset", 10);
            oiiiSettings.enabled = true;
            settings.filters[NarrowbandFilter::OIII] = oiiiSettings;
        }

        // SII settings
        if (params.value("sii_count", 0) > 0) {
            NarrowbandFilterSettings siiSettings;
            siiSettings.name = "SII";
            siiSettings.type = NarrowbandFilter::SII;
            siiSettings.exposure = params.value("sii_exposure", 300.0);
            siiSettings.frameCount = params.value("sii_count", 10);
            siiSettings.gain = params.value("gain", 200);
            siiSettings.offset = params.value("offset", 10);
            siiSettings.enabled = true;
            settings.filters[NarrowbandFilter::SII] = siiSettings;
        }

        settings.useHOSSequence = params.value("use_hos", true);
        settings.useBiColorSequence = params.value("use_bicolor", false);
        settings.interleaved = params.value("interleaved", false);
        settings.sequenceRepeats = params.value("sequence_repeats", 1);
        settings.settlingTime = params.value("settling_time", 2.0);

        bool success = executeSequence(settings);

        if (!success) {
            setErrorType(TaskErrorType::SystemError);
            throw std::runtime_error("Narrowband sequence execution failed");
        }

        addHistoryEntry("Narrowband sequence completed successfully");

    } catch (const std::exception& e) {
        handleFilterError("Narrowband", e.what());
        throw;
    }
}

bool NarrowbandSequenceTask::executeSequence(
    const NarrowbandSequenceSettings& settings) {
    currentSettings_ = settings;
    sequenceStartTime_ = std::chrono::steady_clock::now();
    sequenceProgress_ = 0.0;
    isPaused_ = false;
    isCancelled_ = false;

    // Calculate total frames
    totalFrames_ = 0;
    for (const auto& [filterType, filterSettings] : settings.filters) {
        if (filterSettings.enabled) {
            totalFrames_ += filterSettings.frameCount;
        }
    }
    totalFrames_ *= settings.sequenceRepeats;
    completedFrames_ = 0;

    spdlog::info(
        "Starting narrowband sequence with {} total frames across {} repeats",
        totalFrames_, settings.sequenceRepeats);

    addHistoryEntry("Narrowband sequence parameters: " +
                    std::to_string(totalFrames_) + " total frames, " +
                    std::to_string(settings.sequenceRepeats) + " repeats");

    try {
        for (int repeat = 0; repeat < settings.sequenceRepeats; ++repeat) {
            if (isCancelled_) {
                addHistoryEntry("Narrowband sequence cancelled");
                return false;
            }

            spdlog::info("Starting narrowband sequence repeat {} of {}",
                         repeat + 1, settings.sequenceRepeats);
            addHistoryEntry("Starting repeat " + std::to_string(repeat + 1) +
                            "/" + std::to_string(settings.sequenceRepeats));

            if (settings.interleaved) {
                if (!executeInterleavedPattern(settings)) {
                    return false;
                }
            } else {
                if (!executeSequentialPattern(settings)) {
                    return false;
                }
            }
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Narrowband sequence execution failed: {}", e.what());
        return false;
    }
}

std::future<bool> NarrowbandSequenceTask::executeSequenceAsync(
    const NarrowbandSequenceSettings& settings) {
    return std::async(std::launch::async,
                      [this, settings]() { return executeSequence(settings); });
}

bool NarrowbandSequenceTask::executeSequentialPattern(
    const NarrowbandSequenceSettings& settings) {
    addHistoryEntry("Executing sequential narrowband pattern");

    // Determine sequence order (HOS for Hubble palette)
    std::vector<NarrowbandFilter> sequence;

    if (settings.useHOSSequence) {
        sequence = {NarrowbandFilter::Ha, NarrowbandFilter::OIII,
                    NarrowbandFilter::SII};
    } else if (settings.useBiColorSequence) {
        sequence = {NarrowbandFilter::Ha, NarrowbandFilter::OIII};
    } else {
        // Use all enabled filters
        for (const auto& [filterType, filterSettings] : settings.filters) {
            if (filterSettings.enabled) {
                sequence.push_back(filterType);
            }
        }
    }

    for (NarrowbandFilter filterType : sequence) {
        if (isCancelled_) {
            addHistoryEntry("Narrowband sequence cancelled");
            return false;
        }

        auto it = settings.filters.find(filterType);
        if (it != settings.filters.end() && it->second.enabled) {
            // Wait if paused
            while (isPaused_ && !isCancelled_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (isCancelled_)
                break;

            const auto& filterSettings = it->second;
            spdlog::info("Capturing {} frames with {} filter ({}s exposure)",
                         filterSettings.frameCount, filterSettings.name,
                         filterSettings.exposure);

            bool success = captureNarrowbandFrames(filterSettings);
            if (!success) {
                return false;
            }
        }
    }

    return true;
}

bool NarrowbandSequenceTask::executeInterleavedPattern(
    const NarrowbandSequenceSettings& settings) {
    addHistoryEntry("Executing interleaved narrowband pattern");

    // Get enabled filters
    std::vector<NarrowbandFilterSettings> enabledFilters;
    for (const auto& [filterType, filterSettings] : settings.filters) {
        if (filterSettings.enabled) {
            enabledFilters.push_back(filterSettings);
        }
    }

    if (enabledFilters.empty()) {
        spdlog::error("No enabled filters for narrowband sequence");
        return false;
    }

    // Find maximum frame count
    int maxFrames = 0;
    for (const auto& filterSettings : enabledFilters) {
        maxFrames = std::max(maxFrames, filterSettings.frameCount);
    }

    // Execute interleaved pattern
    for (int frameIndex = 0; frameIndex < maxFrames; ++frameIndex) {
        if (isCancelled_) {
            addHistoryEntry("Narrowband sequence cancelled");
            return false;
        }

        for (const auto& filterSettings : enabledFilters) {
            if (frameIndex < filterSettings.frameCount) {
                // Wait if paused
                while (isPaused_ && !isCancelled_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                if (isCancelled_)
                    break;

                spdlog::info("Capturing frame {} of {} with {} filter",
                             frameIndex + 1, filterSettings.frameCount,
                             filterSettings.name);

                // Create single-frame version of settings
                NarrowbandFilterSettings singleFrameSettings = filterSettings;
                singleFrameSettings.frameCount = 1;

                bool success = captureNarrowbandFrames(singleFrameSettings);
                if (!success) {
                    return false;
                }
            }
        }

        if (isCancelled_)
            break;
    }

    return !isCancelled_;
}

bool NarrowbandSequenceTask::captureNarrowbandFrames(
    const NarrowbandFilterSettings& filterSettings) {
    try {
        // Change to the specified filter
        FilterChangeTask filterChanger("temp_filter_change");
        json changeParams = {{"filterName", filterSettings.name},
                             {"timeout", 30},
                             {"verify", true}};

        filterChanger.execute(changeParams);

        // Wait for settling
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<int>(currentSettings_.settlingTime * 1000)));

        // Update filter progress
        filterProgress_[filterSettings.name] = 0.0;

        // Simulate frame capture
        for (int i = 0; i < filterSettings.frameCount; ++i) {
            if (isCancelled_) {
                return false;
            }

            while (isPaused_ && !isCancelled_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            spdlog::info(
                "Capturing frame {} of {} with {} filter ({}s exposure)", i + 1,
                filterSettings.frameCount, filterSettings.name,
                filterSettings.exposure);

            addHistoryEntry("Capturing " + filterSettings.name + " frame " +
                            std::to_string(i + 1) + "/" +
                            std::to_string(filterSettings.frameCount));

            // Simulate exposure time (scaled down for testing)
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(
                    filterSettings.exposure * 10)));  // Scaled down

            completedFrames_++;
            updateProgress(completedFrames_, totalFrames_);

            // Update filter-specific progress
            double filterProgress =
                (static_cast<double>(i + 1) / filterSettings.frameCount) *
                100.0;
            filterProgress_[filterSettings.name] = filterProgress;

            addHistoryEntry("Frame completed: " + filterSettings.name + " " +
                            std::to_string(i + 1) + "/" +
                            std::to_string(filterSettings.frameCount));
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to capture {} frames: {}", filterSettings.name,
                      e.what());
        handleFilterError(filterSettings.name,
                          "Frame capture failed: " + std::string(e.what()));
        return false;
    }
}

void NarrowbandSequenceTask::addCustomFilter(const std::string& filterName,
                                             double exposure, int frameCount,
                                             int gain, int offset) {
    NarrowbandFilterSettings customSettings;
    customSettings.name = filterName;
    customSettings.type = NarrowbandFilter::Custom;
    customSettings.exposure = exposure;
    customSettings.frameCount = frameCount;
    customSettings.gain = gain;
    customSettings.offset = offset;
    customSettings.enabled = true;

    currentSettings_.filters[NarrowbandFilter::Custom] = customSettings;

    addHistoryEntry("Custom narrowband filter added: " + filterName + " (" +
                    std::to_string(frameCount) + " frames, " +
                    std::to_string(exposure) + "s exposure)");
}

void NarrowbandSequenceTask::setupHubblePalette(double haExposure,
                                                double oiiiExposure,
                                                double siiExposure,
                                                int frameCount, int gain,
                                                int offset) {
    currentSettings_.filters.clear();
    currentSettings_.useHOSSequence = true;

    // H-alpha
    NarrowbandFilterSettings haSettings;
    haSettings.name = "Ha";
    haSettings.type = NarrowbandFilter::Ha;
    haSettings.exposure = haExposure;
    haSettings.frameCount = frameCount;
    haSettings.gain = gain;
    haSettings.offset = offset;
    haSettings.enabled = true;
    currentSettings_.filters[NarrowbandFilter::Ha] = haSettings;

    // OIII
    NarrowbandFilterSettings oiiiSettings;
    oiiiSettings.name = "OIII";
    oiiiSettings.type = NarrowbandFilter::OIII;
    oiiiSettings.exposure = oiiiExposure;
    oiiiSettings.frameCount = frameCount;
    oiiiSettings.gain = gain;
    oiiiSettings.offset = offset;
    oiiiSettings.enabled = true;
    currentSettings_.filters[NarrowbandFilter::OIII] = oiiiSettings;

    // SII
    NarrowbandFilterSettings siiSettings;
    siiSettings.name = "SII";
    siiSettings.type = NarrowbandFilter::SII;
    siiSettings.exposure = siiExposure;
    siiSettings.frameCount = frameCount;
    siiSettings.gain = gain;
    siiSettings.offset = offset;
    siiSettings.enabled = true;
    currentSettings_.filters[NarrowbandFilter::SII] = siiSettings;

    addHistoryEntry("Hubble palette setup: Ha=" + std::to_string(haExposure) +
                    "s, OIII=" + std::to_string(oiiiExposure) +
                    "s, SII=" + std::to_string(siiExposure) + "s");
}

double NarrowbandSequenceTask::getSequenceProgress() const {
    return sequenceProgress_.load();
}

std::map<std::string, double> NarrowbandSequenceTask::getFilterProgress()
    const {
    return filterProgress_;
}

void NarrowbandSequenceTask::pauseSequence() {
    isPaused_ = true;
    addHistoryEntry("Narrowband sequence paused");
    spdlog::info("Narrowband sequence paused");
}

void NarrowbandSequenceTask::resumeSequence() {
    isPaused_ = false;
    addHistoryEntry("Narrowband sequence resumed");
    spdlog::info("Narrowband sequence resumed");
}

void NarrowbandSequenceTask::cancelSequence() {
    isCancelled_ = true;
    addHistoryEntry("Narrowband sequence cancelled");
    spdlog::info("Narrowband sequence cancelled");
}

std::string NarrowbandSequenceTask::narrowbandFilterToString(
    NarrowbandFilter filter) const {
    switch (filter) {
        case NarrowbandFilter::Ha:
            return "Ha";
        case NarrowbandFilter::OIII:
            return "OIII";
        case NarrowbandFilter::SII:
            return "SII";
        case NarrowbandFilter::NII:
            return "NII";
        case NarrowbandFilter::Hb:
            return "Hb";
        case NarrowbandFilter::Custom:
            return "Custom";
        default:
            return "Unknown";
    }
}

void NarrowbandSequenceTask::updateProgress(int completedFrames,
                                            int totalFrames) {
    if (totalFrames > 0) {
        double progress =
            (static_cast<double>(completedFrames) / totalFrames) * 100.0;
        sequenceProgress_ = progress;

        if (completedFrames % 10 == 0) {  // Log every 10 frames
            spdlog::info("Narrowband sequence progress: {:.1f}% ({}/{})",
                         progress, completedFrames, totalFrames);
        }
    }
}

}  // namespace lithium::task::filter