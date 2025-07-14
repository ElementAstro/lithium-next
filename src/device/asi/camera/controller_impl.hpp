/*
 * controller_impl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Controller Implementation Details

This header contains the implementation details for the ASI Camera Controller,
including private member functions and internal data structures.

*************************************************/

#pragma once

#include "controller.hpp"
#include <chrono>
#include <thread>

namespace lithium::device::asi::camera {

/**
 * @brief Implementation details for ASI Camera Controller
 *
 * This namespace contains internal implementation details that are
 * not part of the public interface.
 */
namespace impl {

/**
 * @brief Camera state information
 */
struct CameraState {
    bool initialized = false;
    bool connected = false;
    bool exposing = false;
    bool video_active = false;
    bool sequence_active = false;
    bool cooling_enabled = false;

    int camera_id = -1;
    double current_temperature = 20.0;
    double target_temperature = -10.0;

    std::chrono::steady_clock::time_point exposure_start_time;
    double exposure_duration_ms = 0.0;

    std::string last_error;
    std::chrono::steady_clock::time_point last_error_time;
};

/**
 * @brief Camera configuration parameters
 */
struct CameraConfig {
    // Image settings
    int width = 1920;
    int height = 1080;
    int bin_x = 1;
    int bin_y = 1;
    int roi_x = 0;
    int roi_y = 0;
    int roi_width = 0;
    int roi_height = 0;

    // Exposure settings
    double gain = 0.0;
    double offset = 0.0;
    bool high_speed_mode = false;
    bool hardware_binning = false;

    // USB settings
    int usb_traffic = 40;

    // Image format
    std::string format = "RAW16";

    // Flip settings
    bool flip_horizontal = false;
    bool flip_vertical = false;

    // White balance (for color cameras)
    double wb_red = 1.0;
    double wb_green = 1.0;
    double wb_blue = 1.0;
    bool auto_wb = false;
};

/**
 * @brief Exposure information
 */
struct ExposureInfo {
    bool is_dark = false;
    bool is_ready = false;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    double duration_ms = 0.0;
    size_t image_size = 0;
};

/**
 * @brief Sequence information
 */
struct SequenceInfo {
    bool active = false;
    bool paused = false;
    int total_frames = 0;
    int completed_frames = 0;
    int current_frame = 0;
    std::string config;
    std::chrono::steady_clock::time_point start_time;
};

/**
 * @brief Video streaming information
 */
struct VideoInfo {
    bool active = false;
    int fps = 30;
    int frame_count = 0;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_frame_time;
};

/**
 * @brief Temperature control information
 */
struct TemperatureInfo {
    bool cooling_enabled = false;
    double current_temp = 20.0;
    double target_temp = -10.0;
    double cooling_power = 0.0; // 0-100%
    std::chrono::steady_clock::time_point last_temp_read;
};

/**
 * @brief Error tracking information
 */
struct ErrorInfo {
    std::string last_error;
    std::chrono::steady_clock::time_point last_error_time;
    int error_count = 0;
    std::vector<std::pair<std::chrono::steady_clock::time_point, std::string>> error_history;
};

/**
 * @brief Statistics tracking
 */
struct Statistics {
    int total_exposures = 0;
    int successful_exposures = 0;
    int failed_exposures = 0;
    double total_exposure_time = 0.0;

    int total_sequences = 0;
    int successful_sequences = 0;
    int failed_sequences = 0;

    int total_video_sessions = 0;
    int total_video_frames = 0;

    std::chrono::steady_clock::time_point session_start_time;
    std::chrono::steady_clock::time_point last_activity_time;
};

/**
 * @brief Performance metrics
 */
struct PerformanceMetrics {
    double avg_exposure_overhead_ms = 0.0;
    double avg_download_speed_mbps = 0.0;
    double avg_temperature_stability = 0.0;
    int dropped_frames = 0;

    std::chrono::steady_clock::time_point last_metric_update;
};

} // namespace impl

/**
 * @brief Extended ASI Camera Controller with implementation details
 *
 * This class extends the public ASI Camera Controller with additional
 * implementation-specific functionality and data members.
 */
class ASICameraControllerImpl : public ASICameraController {
public:
    ASICameraControllerImpl();
    ~ASICameraControllerImpl() override = default;

    // Additional implementation-specific methods
    auto getCameraState() const -> impl::CameraState;
    auto getCameraConfig() const -> impl::CameraConfig;
    auto getExposureInfo() const -> impl::ExposureInfo;
    auto getSequenceInfo() const -> impl::SequenceInfo;
    auto getVideoInfo() const -> impl::VideoInfo;
    auto getTemperatureInfo() const -> impl::TemperatureInfo;
    auto getErrorInfo() const -> impl::ErrorInfo;
    auto getStatistics() const -> impl::Statistics;
    auto getPerformanceMetrics() const -> impl::PerformanceMetrics;

    // Internal state management
    void updateCameraState();
    void resetStatistics();
    void updatePerformanceMetrics();

    // Internal error handling
    void recordError(const std::string& error);
    void clearErrorHistory();

    // Internal monitoring
    void startInternalMonitoring();
    void stopInternalMonitoring();

private:
    // Implementation state
    impl::CameraState m_state;
    impl::CameraConfig m_config;
    impl::ExposureInfo m_exposure_info;
    impl::SequenceInfo m_sequence_info;
    impl::VideoInfo m_video_info;
    impl::TemperatureInfo m_temperature_info;
    impl::ErrorInfo m_error_info;
    impl::Statistics m_statistics;
    impl::PerformanceMetrics m_performance_metrics;

    // Internal monitoring
    std::thread m_monitoring_thread;
    std::atomic<bool> m_monitoring_active{false};
    std::condition_variable m_monitoring_cv;
    mutable std::mutex m_monitoring_mutex;

    // Internal helper methods
    void updateStateInternal();
    void updateTemperatureInternal();
    void updateExposureProgressInternal();
    void updateVideoStatsInternal();
    void updateSequenceProgressInternal();
    void updatePerformanceMetricsInternal();

    void monitoringLoop();
    void handleInternalError(const std::string& error);

    // Validation helpers
    bool validateCameraId(int camera_id) const;
    bool validateExposureParameters(double duration_ms) const;
    bool validateTemperatureRange(double temp) const;
    bool validateROI(int x, int y, int width, int height) const;
    bool validateBinning(int binx, int biny) const;
};

} // namespace lithium::device::asi::camera
