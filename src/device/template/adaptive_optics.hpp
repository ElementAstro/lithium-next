/*
 * adaptive_optics.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomAdaptiveOptics device following INDI architecture

*************************************************/

#pragma once

#include "device.hpp"

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

enum class AOState {
    IDLE,
    CORRECTING,
    CALIBRATING,
    ERROR
};

enum class AOMode {
    OPEN_LOOP,
    CLOSED_LOOP,
    MANUAL
};

// Tip-tilt information
struct TipTiltData {
    double tip{0.0};    // arcseconds
    double tilt{0.0};   // arcseconds
    double magnitude{0.0}; // total correction
    std::chrono::system_clock::time_point timestamp;
} ATOM_ALIGNAS(32);

// Wavefront sensor data
struct WavefrontData {
    std::vector<double> slope_x;  // x-slopes across subapertures
    std::vector<double> slope_y;  // y-slopes across subapertures
    double seeing{0.0};           // arcseconds
    double coherence_time{0.0};   // milliseconds
    double isoplanatic_angle{0.0}; // arcseconds
    std::chrono::system_clock::time_point timestamp;
} ATOM_ALIGNAS(64);

// AO capabilities
struct AOCapabilities {
    bool hasTipTilt{true};
    bool hasDeformableMirror{false};
    bool hasWavefrontSensor{false};
    int num_actuators{0};
    int num_subapertures{0};
    double max_stroke{0.0};       // microns
    double resolution{0.0};       // nm
    double correction_rate{1000.0}; // Hz
} ATOM_ALIGNAS(32);

// AO correction parameters
struct AOParameters {
    // Control loop parameters
    double loop_gain{0.3};
    double bandwidth{100.0};      // Hz
    bool enable_tip_tilt{true};
    bool enable_focus{false};
    bool enable_higher_order{false};
    
    // Tip-tilt parameters
    double tip_gain{0.5};
    double tilt_gain{0.5};
    double max_tip{5.0};          // arcseconds
    double max_tilt{5.0};         // arcseconds
    
    // Deformable mirror parameters
    std::vector<double> actuator_gains;
    double max_actuator_stroke{1.0}; // microns
    bool enable_zernike_correction{false};
    
    // Wavefront sensor parameters
    double exposure_time{0.001};  // seconds
    int binning{1};
    double threshold{0.1};
} ATOM_ALIGNAS(128);

// AO statistics
struct AOStatistics {
    double rms_tip{0.0};          // arcseconds
    double rms_tilt{0.0};         // arcseconds
    double rms_total{0.0};        // arcseconds
    double strehl_ratio{0.0};     // 0-1
    double correction_rate{0.0};  // Hz
    uint64_t correction_count{0};
    std::chrono::seconds run_time{0};
    std::chrono::system_clock::time_point session_start;
} ATOM_ALIGNAS(64);

class AtomAdaptiveOptics : public AtomDriver {
public:
    explicit AtomAdaptiveOptics(std::string name) : AtomDriver(std::move(name)) {
        setType("AdaptiveOptics");
        ao_statistics_.session_start = std::chrono::system_clock::now();
    }
    
    ~AtomAdaptiveOptics() override = default;

    // Capabilities
    const AOCapabilities& getAOCapabilities() const { return ao_capabilities_; }
    void setAOCapabilities(const AOCapabilities& caps) { ao_capabilities_ = caps; }

    // Parameters
    const AOParameters& getAOParameters() const { return ao_parameters_; }
    void setAOParameters(const AOParameters& params) { ao_parameters_ = params; }

    // State management
    AOState getAOState() const { return ao_state_; }
    AOMode getAOMode() const { return ao_mode_; }
    virtual bool isCorrecting() const = 0;

    // Control loop
    virtual auto startCorrection() -> bool = 0;
    virtual auto stopCorrection() -> bool = 0;
    virtual auto setMode(AOMode mode) -> bool = 0;
    virtual auto setLoopGain(double gain) -> bool = 0;
    virtual auto getLoopGain() -> double = 0;

    // Tip-tilt control
    virtual auto enableTipTilt(bool enable) -> bool = 0;
    virtual auto setTipTiltGains(double tip_gain, double tilt_gain) -> bool = 0;
    virtual auto getTipTiltData() -> TipTiltData = 0;
    virtual auto setTipTiltCorrection(double tip, double tilt) -> bool = 0;
    virtual auto zeroTipTilt() -> bool = 0;

    // Deformable mirror control (if available)
    virtual auto enableDeformableMirror(bool enable) -> bool = 0;
    virtual auto setActuatorVoltages(const std::vector<double>& voltages) -> bool = 0;
    virtual auto getActuatorVoltages() -> std::vector<double> = 0;
    virtual auto zeroDeformableMirror() -> bool = 0;
    virtual auto applyZernikeMode(int mode, double amplitude) -> bool = 0;

    // Wavefront sensing (if available)
    virtual auto enableWavefrontSensor(bool enable) -> bool = 0;
    virtual auto getWavefrontData() -> WavefrontData = 0;
    virtual auto calibrateWavefrontSensor() -> bool = 0;
    virtual auto setWFSExposure(double exposure) -> bool = 0;

    // Calibration
    virtual auto startCalibration() -> bool = 0;
    virtual auto stopCalibration() -> bool = 0;
    virtual auto isCalibrated() -> bool = 0;
    virtual auto loadCalibration(const std::string& filename) -> bool = 0;
    virtual auto saveCalibration(const std::string& filename) -> bool = 0;
    virtual auto resetCalibration() -> bool = 0;

    // Focus control
    virtual auto enableFocusCorrection(bool enable) -> bool = 0;
    virtual auto setFocusCorrection(double focus) -> bool = 0;
    virtual auto getFocusCorrection() -> double = 0;
    virtual auto autoFocus() -> bool = 0;

    // Atmospheric monitoring
    virtual auto getSeeing() -> double = 0;
    virtual auto getCoherenceTime() -> double = 0;
    virtual auto getIsoplanticAngle() -> double = 0;
    virtual auto getAtmosphericTurbulence() -> double = 0;

    // Statistics and performance
    virtual auto getAOStatistics() -> AOStatistics = 0;
    virtual auto resetStatistics() -> bool = 0;
    virtual auto getCorrectionHistory(int count = 100) -> std::vector<TipTiltData> = 0;
    virtual auto getStrehlRatio() -> double = 0;

    // Configuration management
    virtual auto loadConfiguration(const std::string& filename) -> bool = 0;
    virtual auto saveConfiguration(const std::string& filename) -> bool = 0;
    virtual auto createDefaultConfiguration() -> bool = 0;

    // Diagnostic and testing
    virtual auto runDiagnostics() -> bool = 0;
    virtual auto testTipTilt() -> bool = 0;
    virtual auto testDeformableMirror() -> bool = 0;
    virtual auto testWavefrontSensor() -> bool = 0;
    virtual auto measureSystemResponse() -> bool = 0;

    // Advanced features
    virtual auto enableDisturbanceRejection(bool enable) -> bool = 0;
    virtual auto setTargetStrehl(double strehl) -> bool = 0;
    virtual auto enableAdaptiveGain(bool enable) -> bool = 0;
    virtual auto optimizeControlLoop() -> bool = 0;

    // Integration with other devices
    virtual auto setTargetCamera(const std::string& camera_name) -> bool = 0;
    virtual auto getTargetCamera() -> std::string = 0;
    virtual auto setGuideCamera(const std::string& camera_name) -> bool = 0;
    virtual auto getGuideCamera() -> std::string = 0;

    // Event callbacks
    using CorrectionCallback = std::function<void(const TipTiltData&)>;
    using StateCallback = std::function<void(AOState state, const std::string& message)>;
    using WavefrontCallback = std::function<void(const WavefrontData&)>;
    using StatisticsCallback = std::function<void(const AOStatistics&)>;

    virtual void setCorrectionCallback(CorrectionCallback callback) { correction_callback_ = std::move(callback); }
    virtual void setStateCallback(StateCallback callback) { state_callback_ = std::move(callback); }
    virtual void setWavefrontCallback(WavefrontCallback callback) { wavefront_callback_ = std::move(callback); }
    virtual void setStatisticsCallback(StatisticsCallback callback) { statistics_callback_ = std::move(callback); }

    // Utility methods
    virtual auto tipTiltToString(const TipTiltData& data) -> std::string;
    virtual auto calculateRMS(const std::vector<TipTiltData>& history) -> std::tuple<double, double, double>;
    virtual auto aoStateToString(AOState state) -> std::string;
    virtual auto aoModeToString(AOMode mode) -> std::string;

protected:
    AOState ao_state_{AOState::IDLE};
    AOMode ao_mode_{AOMode::OPEN_LOOP};
    AOCapabilities ao_capabilities_;
    AOParameters ao_parameters_;
    AOStatistics ao_statistics_;
    
    // Current data
    TipTiltData current_tip_tilt_;
    WavefrontData current_wavefront_;
    std::vector<double> actuator_voltages_;
    
    // Correction history for statistics
    std::vector<TipTiltData> correction_history_;
    static constexpr size_t MAX_CORRECTION_HISTORY = 1000;
    
    // Device connections
    std::string target_camera_name_;
    std::string guide_camera_name_;
    
    // Calibration state
    bool calibrated_{false};
    std::string calibration_file_;
    
    // Callbacks
    CorrectionCallback correction_callback_;
    StateCallback state_callback_;
    WavefrontCallback wavefront_callback_;
    StatisticsCallback statistics_callback_;
    
    // Utility methods
    virtual void updateAOState(AOState state) { ao_state_ = state; }
    virtual void updateStatistics(const TipTiltData& correction);
    virtual void addCorrectionToHistory(const TipTiltData& correction);
    virtual void notifyCorrectionUpdate(const TipTiltData& correction);
    virtual void notifyStateChange(AOState state, const std::string& message = "");
    virtual void notifyWavefrontUpdate(const WavefrontData& wavefront);
    virtual void notifyStatisticsUpdate(const AOStatistics& stats);
};

// Inline utility implementations
inline auto AtomAdaptiveOptics::aoStateToString(AOState state) -> std::string {
    switch (state) {
        case AOState::IDLE: return "IDLE";
        case AOState::CORRECTING: return "CORRECTING";
        case AOState::CALIBRATING: return "CALIBRATING";
        case AOState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

inline auto AtomAdaptiveOptics::aoModeToString(AOMode mode) -> std::string {
    switch (mode) {
        case AOMode::OPEN_LOOP: return "OPEN_LOOP";
        case AOMode::CLOSED_LOOP: return "CLOSED_LOOP";
        case AOMode::MANUAL: return "MANUAL";
        default: return "UNKNOWN";
    }
}
