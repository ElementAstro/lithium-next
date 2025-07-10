/*
 * telescope_coordinator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Telescope Coordination Component

*************************************************/

#pragma once

#include <memory>
#include <optional>
#include <atomic>
#include <functional>
#include <thread>

namespace lithium::ascom::dome::components {

class HardwareInterface;
class AzimuthManager;

/**
 * @brief Telescope Coordination Component for ASCOM Dome
 */
class TelescopeCoordinator {
public:
    struct TelescopeParameters {
        double radius_from_center{0.0};  // meters
        double height_offset{0.0};       // meters
        double azimuth_offset{0.0};      // degrees
        double altitude_offset{0.0};     // degrees
    };

    explicit TelescopeCoordinator(std::shared_ptr<HardwareInterface> hardware,
                                  std::shared_ptr<AzimuthManager> azimuth_manager);
    virtual ~TelescopeCoordinator();

    // === Telescope Following ===
    auto followTelescope(bool enable) -> bool;
    auto isFollowingTelescope() -> bool;
    auto setTelescopePosition(double az, double alt) -> bool;
    auto getTelescopePosition() -> std::optional<std::pair<double, double>>;

    // === Dome Calculations ===
    auto calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double;
    auto calculateSlitPosition(double telescopeAz, double telescopeAlt) -> std::pair<double, double>;
    auto isTelescopeInSlit() -> bool;
    auto getSlitOffset() -> double;

    // === Configuration ===
    auto setTelescopeParameters(const TelescopeParameters& params) -> bool;
    auto getTelescopeParameters() -> TelescopeParameters;
    auto setFollowingTolerance(double tolerance) -> bool;
    auto getFollowingTolerance() -> double;
    auto setFollowingDelay(int delay) -> bool;
    auto getFollowingDelay() -> int;

    // === Automatic Coordination ===
    auto startAutomaticFollowing() -> bool;
    auto stopAutomaticFollowing() -> bool;
    auto isAutomaticFollowing() -> bool;
    auto setFollowingCallback(std::function<void(bool, const std::string&)> callback) -> void;

private:
    std::shared_ptr<HardwareInterface> hardware_;
    std::shared_ptr<AzimuthManager> azimuth_manager_;

    std::atomic<bool> is_following_{false};
    std::atomic<bool> is_automatic_following_{false};
    std::atomic<double> telescope_azimuth_{0.0};
    std::atomic<double> telescope_altitude_{0.0};
    std::atomic<double> following_tolerance_{1.0}; // degrees
    
    TelescopeParameters telescope_params_;
    int following_delay_{1000}; // milliseconds

    std::function<void(bool, const std::string&)> following_callback_;
    std::unique_ptr<std::thread> following_thread_;
    std::atomic<bool> stop_following_{false};

    auto updateFollowingStatus() -> void;
    auto followingLoop() -> void;
    auto calculateGeometricOffset(double telescopeAz, double telescopeAlt) -> double;
};

} // namespace lithium::ascom::dome::components
