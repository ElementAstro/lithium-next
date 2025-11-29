/*
 * commands.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: PHD2 RPC command builders with modern C++ features

*************************************************/

#pragma once

#include "types.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace phd2 {

/**
 * @brief PHD2 RPC command builder
 *
 * Provides type-safe command building for PHD2 JSON-RPC interface
 */
class Commands {
public:
    // ==================== Guiding Control ====================

    /**
     * @brief Build guide command
     * @param settle Settle parameters
     * @param recalibrate Force recalibration
     * @return JSON params for guide RPC
     */
    [[nodiscard]] static auto guide(const SettleParams& settle,
                                    bool recalibrate = false) -> json;

    /**
     * @brief Build dither command
     * @param amount Dither amount in pixels
     * @param raOnly Only dither in RA
     * @param settle Settle parameters
     * @return JSON params for dither RPC
     */
    [[nodiscard]] static auto dither(double amount, bool raOnly,
                                     const SettleParams& settle) -> json;

    /**
     * @brief Build stop_capture command
     */
    [[nodiscard]] static auto stopCapture() -> json;

    /**
     * @brief Build set_paused command
     * @param paused Pause state
     * @param full Full pause (stop looping)
     */
    [[nodiscard]] static auto setPaused(bool paused, bool full = false) -> json;

    /**
     * @brief Build loop command
     */
    [[nodiscard]] static auto loop() -> json;

    // ==================== Calibration ====================

    /**
     * @brief Build clear_calibration command
     * @param which "Mount", "AO", or "both"
     */
    [[nodiscard]] static auto clearCalibration(std::string_view which = "both")
        -> json;

    /**
     * @brief Build flip_calibration command
     */
    [[nodiscard]] static auto flipCalibration() -> json;

    // ==================== Star Selection ====================

    /**
     * @brief Build find_star command
     * @param roi Optional region of interest [x, y, width, height]
     */
    [[nodiscard]] static auto findStar(
        const std::optional<std::array<int, 4>>& roi = std::nullopt) -> json;

    /**
     * @brief Build set_lock_position command
     * @param x X coordinate
     * @param y Y coordinate
     * @param exact Use exact position
     */
    [[nodiscard]] static auto setLockPosition(double x, double y,
                                              bool exact = true) -> json;

    // ==================== Camera Control ====================

    /**
     * @brief Build set_exposure command
     * @param exposureMs Exposure in milliseconds
     */
    [[nodiscard]] static auto setExposure(int exposureMs) -> json;

    /**
     * @brief Build capture_single_frame command
     * @param exposureMs Optional exposure time
     * @param subframe Optional subframe [x, y, width, height]
     */
    [[nodiscard]] static auto captureSingleFrame(
        std::optional<int> exposureMs = std::nullopt,
        std::optional<std::array<int, 4>> subframe = std::nullopt) -> json;

    // ==================== Profile Management ====================

    /**
     * @brief Build set_profile command
     * @param profileId Profile ID
     */
    [[nodiscard]] static auto setProfile(int profileId) -> json;

    // ==================== Equipment ====================

    /**
     * @brief Build set_connected command
     * @param connect Connection state
     */
    [[nodiscard]] static auto setConnected(bool connect) -> json;

    /**
     * @brief Build guide_pulse command
     * @param amount Pulse duration in ms
     * @param direction Direction (N/S/E/W)
     * @param which "Mount" or "AO"
     */
    [[nodiscard]] static auto guidePulse(int amount, std::string_view direction,
                                         std::string_view which = "Mount")
        -> json;

    // ==================== Algorithm Parameters ====================

    /**
     * @brief Build set_algo_param command
     * @param axis Axis name (ra, dec, x, y)
     * @param name Parameter name
     * @param value Parameter value
     */
    [[nodiscard]] static auto setAlgoParam(std::string_view axis,
                                           std::string_view name, double value)
        -> json;

    /**
     * @brief Build get_algo_param command
     * @param axis Axis name
     * @param name Parameter name
     */
    [[nodiscard]] static auto getAlgoParam(std::string_view axis,
                                           std::string_view name) -> json;

    // ==================== Settings ====================

    /**
     * @brief Build set_dec_guide_mode command
     * @param mode "Off", "Auto", "North", or "South"
     */
    [[nodiscard]] static auto setDecGuideMode(std::string_view mode) -> json;

    /**
     * @brief Build set_guide_output_enabled command
     * @param enable Enable state
     */
    [[nodiscard]] static auto setGuideOutputEnabled(bool enable) -> json;

    /**
     * @brief Build set_lock_shift_enabled command
     * @param enable Enable state
     */
    [[nodiscard]] static auto setLockShiftEnabled(bool enable) -> json;

    /**
     * @brief Build set_lock_shift_params command
     * @param params Lock shift parameters
     */
    [[nodiscard]] static auto setLockShiftParams(const json& params) -> json;

    // ==================== Misc ====================

    /**
     * @brief Build save_image command
     */
    [[nodiscard]] static auto saveImage() -> json;

    /**
     * @brief Build shutdown command
     */
    [[nodiscard]] static auto shutdown() -> json;
};

/**
 * @brief PHD2 RPC method names
 */
namespace methods {
// Guiding control
inline constexpr std::string_view kGuide = "guide";
inline constexpr std::string_view kDither = "dither";
inline constexpr std::string_view kStopCapture = "stop_capture";
inline constexpr std::string_view kSetPaused = "set_paused";
inline constexpr std::string_view kLoop = "loop";

// Calibration
inline constexpr std::string_view kClearCalibration = "clear_calibration";
inline constexpr std::string_view kFlipCalibration = "flip_calibration";
inline constexpr std::string_view kGetCalibrated = "get_calibrated";
inline constexpr std::string_view kGetCalibrationData = "get_calibration_data";

// Star selection
inline constexpr std::string_view kFindStar = "find_star";
inline constexpr std::string_view kSetLockPosition = "set_lock_position";
inline constexpr std::string_view kGetLockPosition = "get_lock_position";

// Camera
inline constexpr std::string_view kGetExposure = "get_exposure";
inline constexpr std::string_view kSetExposure = "set_exposure";
inline constexpr std::string_view kGetExposureDurations = "get_exposure_durations";
inline constexpr std::string_view kCaptureSingleFrame = "capture_single_frame";
inline constexpr std::string_view kGetCameraFrameSize = "get_camera_frame_size";
inline constexpr std::string_view kGetCameraBinning = "get_camera_binning";

// Status
inline constexpr std::string_view kGetAppState = "get_app_state";
inline constexpr std::string_view kGetPixelScale = "get_pixel_scale";
inline constexpr std::string_view kGetSettling = "get_settling";
inline constexpr std::string_view kGetSearchRegion = "get_search_region";

// Profile
inline constexpr std::string_view kGetProfile = "get_profile";
inline constexpr std::string_view kSetProfile = "set_profile";
inline constexpr std::string_view kGetProfiles = "get_profiles";

// Equipment
inline constexpr std::string_view kGetConnected = "get_connected";
inline constexpr std::string_view kSetConnected = "set_connected";
inline constexpr std::string_view kGetCurrentEquipment = "get_current_equipment";
inline constexpr std::string_view kGuidePulse = "guide_pulse";

// Algorithm
inline constexpr std::string_view kGetAlgoParamNames = "get_algo_param_names";
inline constexpr std::string_view kGetAlgoParam = "get_algo_param";
inline constexpr std::string_view kSetAlgoParam = "set_algo_param";

// Settings
inline constexpr std::string_view kGetDecGuideMode = "get_dec_guide_mode";
inline constexpr std::string_view kSetDecGuideMode = "set_dec_guide_mode";
inline constexpr std::string_view kGetGuideOutputEnabled = "get_guide_output_enabled";
inline constexpr std::string_view kSetGuideOutputEnabled = "set_guide_output_enabled";
inline constexpr std::string_view kGetLockShiftEnabled = "get_lock_shift_enabled";
inline constexpr std::string_view kSetLockShiftEnabled = "set_lock_shift_enabled";
inline constexpr std::string_view kGetLockShiftParams = "get_lock_shift_params";
inline constexpr std::string_view kSetLockShiftParams = "set_lock_shift_params";

// Misc
inline constexpr std::string_view kSaveImage = "save_image";
inline constexpr std::string_view kGetStarImage = "get_star_image";
inline constexpr std::string_view kGetCcdTemperature = "get_ccd_temperature";
inline constexpr std::string_view kGetCoolerStatus = "get_cooler_status";
inline constexpr std::string_view kExportConfigSettings = "export_config_settings";
inline constexpr std::string_view kShutdown = "shutdown";
}  // namespace methods

}  // namespace phd2
