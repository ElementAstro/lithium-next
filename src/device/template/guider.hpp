/*
 * guider.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomGuider device following INDI architecture

*************************************************/

#pragma once

#include "device.hpp"
#include "camera_frame.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

enum class GuideState {
    IDLE,
    CALIBRATING,
    GUIDING,
    DITHERING,
    SETTLING,
    PAUSED,
    ERROR
};

enum class GuideDirection {
    NORTH,
    SOUTH,
    EAST,
    WEST
};

enum class CalibrationState {
    NOT_STARTED,
    IN_PROGRESS,
    COMPLETED,
    FAILED
};

enum class DitherType {
    RANDOM,
    SPIRAL,
    SQUARE
};

// Guide star information
struct GuideStar {
    double x{0.0};          // pixel coordinates
    double y{0.0};
    double flux{0.0};       // star brightness
    double hfd{0.0};        // half flux diameter
    double snr{0.0};        // signal to noise ratio
    bool selected{false};
} ATOM_ALIGNAS(32);

// Guide error information
struct GuideError {
    double ra_error{0.0};   // arcseconds
    double dec_error{0.0};  // arcseconds
    double total_error{0.0}; // arcseconds
    std::chrono::system_clock::time_point timestamp;
} ATOM_ALIGNAS(32);

// Calibration data
struct CalibrationData {
    CalibrationState state{CalibrationState::NOT_STARTED};
    double ra_rate{0.0};     // arcsec/ms
    double dec_rate{0.0};    // arcsec/ms
    double angle{0.0};       // degrees
    double xrate{0.0};       // pixels/ms
    double yrate{0.0};       // pixels/ms
    double min_move{100};    // minimum pulse duration
    double backlash_ra{0.0}; // ms
    double backlash_dec{0.0}; // ms
    bool valid{false};
} ATOM_ALIGNAS(64);

// Guide parameters
struct GuideParameters {
    // Exposure settings
    double exposure_time{1.0};      // seconds
    int gain{0};                    // camera gain
    
    // Guide algorithm settings
    double min_error{0.15};         // arcseconds
    double max_error{5.0};          // arcseconds
    double aggressivity{100.0};     // percentage
    double min_pulse{10.0};         // ms
    double max_pulse{5000.0};       // ms
    
    // Calibration settings
    double calibration_step{1000.0}; // ms
    int calibration_steps{12};
    double calibration_distance{25.0}; // pixels
    
    // Dithering settings
    double dither_amount{3.0};      // pixels
    int settle_time{10};            // seconds
    double settle_tolerance{1.5};   // pixels
    
    // Star selection
    double min_star_hfd{1.5};       // pixels
    double max_star_hfd{10.0};      // pixels
    double min_star_snr{6.0};
    
    bool enable_dec_guiding{true};
    bool reverse_dec{false};
    bool enable_backlash_compensation{false};
} ATOM_ALIGNAS(128);

// Guide statistics
struct GuideStatistics {
    uint32_t frame_count{0};
    double rms_ra{0.0};             // arcseconds
    double rms_dec{0.0};            // arcseconds
    double rms_total{0.0};          // arcseconds
    double max_error{0.0};          // arcseconds
    double drift_rate_ra{0.0};      // arcsec/min
    double drift_rate_dec{0.0};     // arcsec/min
    std::chrono::seconds guide_time{0};
    std::chrono::system_clock::time_point session_start;
} ATOM_ALIGNAS(64);

class AtomGuider : public AtomDriver {
public:
    explicit AtomGuider(std::string name) : AtomDriver(std::move(name)) {
        setType("Guider");
        guide_statistics_.session_start = std::chrono::system_clock::now();
    }
    
    ~AtomGuider() override = default;

    // State management
    GuideState getGuideState() const { return guide_state_; }
    virtual bool isGuiding() const = 0;
    virtual bool isCalibrated() const = 0;

    // Parameters
    const GuideParameters& getGuideParameters() const { return guide_parameters_; }
    void setGuideParameters(const GuideParameters& params) { guide_parameters_ = params; }

    // Guide control
    virtual auto startGuiding() -> bool = 0;
    virtual auto stopGuiding() -> bool = 0;
    virtual auto pauseGuiding() -> bool = 0;
    virtual auto resumeGuiding() -> bool = 0;

    // Calibration
    virtual auto startCalibration() -> bool = 0;
    virtual auto stopCalibration() -> bool = 0;
    virtual auto clearCalibration() -> bool = 0;
    virtual auto getCalibrationData() -> CalibrationData = 0;
    virtual auto loadCalibration(const CalibrationData& data) -> bool = 0;
    virtual auto saveCalibration(const std::string& filename) -> bool = 0;

    // Star selection and management
    virtual auto selectGuideStar(double x, double y) -> bool = 0;
    virtual auto autoSelectGuideStar() -> bool = 0;
    virtual auto getGuideStar() -> std::optional<GuideStar> = 0;
    virtual auto findStars(std::shared_ptr<AtomCameraFrame> frame) -> std::vector<GuideStar> = 0;

    // Guide frames and images
    virtual auto takeGuideFrame() -> std::shared_ptr<AtomCameraFrame> = 0;
    virtual auto getLastGuideFrame() -> std::shared_ptr<AtomCameraFrame> = 0;
    virtual auto saveGuideFrame(const std::string& filename) -> bool = 0;

    // Manual guiding
    virtual auto guide(GuideDirection direction, int duration_ms) -> bool = 0;
    virtual auto pulseGuide(double ra_ms, double dec_ms) -> bool = 0;

    // Dithering
    virtual auto dither(DitherType type = DitherType::RANDOM) -> bool = 0;
    virtual auto isDithering() -> bool = 0;
    virtual auto isSettling() -> bool = 0;
    virtual auto getSettleProgress() -> double = 0;

    // Error and statistics
    virtual auto getCurrentError() -> GuideError = 0;
    virtual auto getGuideStatistics() -> GuideStatistics = 0;
    virtual auto resetStatistics() -> bool = 0;
    virtual auto getErrorHistory(int count = 100) -> std::vector<GuideError> = 0;

    // PHD2 compatibility (if needed)
    virtual auto connectToPHD2() -> bool = 0;
    virtual auto disconnectFromPHD2() -> bool = 0;
    virtual auto isPHD2Connected() -> bool = 0;

    // Camera integration
    virtual auto setGuideCamera(const std::string& camera_name) -> bool = 0;
    virtual auto getGuideCamera() -> std::string = 0;
    virtual auto setExposureTime(double seconds) -> bool = 0;
    virtual auto getExposureTime() -> double = 0;

    // Mount integration
    virtual auto setGuideMount(const std::string& mount_name) -> bool = 0;
    virtual auto getGuideMount() -> std::string = 0;
    virtual auto testMountConnection() -> bool = 0;

    // Advanced features
    virtual auto enableSubframing(bool enable) -> bool = 0;
    virtual auto isSubframingEnabled() -> bool = 0;
    virtual auto setSubframe(int x, int y, int width, int height) -> bool = 0;
    virtual auto getSubframe() -> std::tuple<int, int, int, int> = 0;

    // Dark frame management
    virtual auto takeDarkFrame() -> bool = 0;
    virtual auto setDarkFrame(std::shared_ptr<AtomCameraFrame> dark) -> bool = 0;
    virtual auto enableDarkSubtraction(bool enable) -> bool = 0;
    virtual auto isDarkSubtractionEnabled() -> bool = 0;

    // Event callbacks
    using GuideCallback = std::function<void(const GuideError&)>;
    using StateCallback = std::function<void(GuideState state, const std::string& message)>;
    using StarCallback = std::function<void(const GuideStar&)>;
    using CalibrationCallback = std::function<void(CalibrationState state, double progress)>;
    using DitherCallback = std::function<void(bool success, const std::string& message)>;

    virtual void setGuideCallback(GuideCallback callback) { guide_callback_ = std::move(callback); }
    virtual void setStateCallback(StateCallback callback) { state_callback_ = std::move(callback); }
    virtual void setStarCallback(StarCallback callback) { star_callback_ = std::move(callback); }
    virtual void setCalibrationCallback(CalibrationCallback callback) { calibration_callback_ = std::move(callback); }
    virtual void setDitherCallback(DitherCallback callback) { dither_callback_ = std::move(callback); }

    // Utility methods
    virtual auto calculateGuideCorrection(const GuideError& error) -> std::pair<double, double> = 0;
    virtual auto calculateRMS(const std::vector<GuideError>& errors) -> std::tuple<double, double, double> = 0;
    virtual auto pixelsToArcseconds(double pixels) -> double = 0;
    virtual auto arcsecondsToPixels(double arcsec) -> double = 0;

protected:
    GuideState guide_state_{GuideState::IDLE};
    GuideParameters guide_parameters_;
    CalibrationData calibration_data_;
    GuideStatistics guide_statistics_;
    
    // Current state
    std::optional<GuideStar> current_guide_star_;
    std::shared_ptr<AtomCameraFrame> last_guide_frame_;
    std::shared_ptr<AtomCameraFrame> dark_frame_;
    GuideError current_error_;
    
    // Error history for statistics
    std::vector<GuideError> error_history_;
    static constexpr size_t MAX_ERROR_HISTORY = 1000;
    
    // Device connections
    std::string guide_camera_name_;
    std::string guide_mount_name_;
    
    // Settings
    bool subframing_enabled_{false};
    int subframe_x_{0}, subframe_y_{0}, subframe_width_{0}, subframe_height_{0};
    bool dark_subtraction_enabled_{false};
    double pixel_scale_{1.0}; // arcsec/pixel
    
    // Callbacks
    GuideCallback guide_callback_;
    StateCallback state_callback_;
    StarCallback star_callback_;
    CalibrationCallback calibration_callback_;
    DitherCallback dither_callback_;
    
    // Utility methods
    virtual void updateGuideState(GuideState state) { guide_state_ = state; }
    virtual void updateStatistics(const GuideError& error);
    virtual void addErrorToHistory(const GuideError& error);
    virtual void notifyGuideUpdate(const GuideError& error);
    virtual void notifyStateChange(GuideState state, const std::string& message = "");
    virtual void notifyStarUpdate(const GuideStar& star);
    virtual void notifyCalibrationUpdate(CalibrationState state, double progress);
    virtual void notifyDitherComplete(bool success, const std::string& message = "");
};
