/*
 * options.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: ASTAP solver options with modern C++ builder pattern

*************************************************/

#pragma once

#include "../common/process_runner.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace lithium::client::astap {

/**
 * @brief ASTAP solver options
 */
struct Options {
    // ==================== Basic Options ====================
    std::optional<double> fov;           // -fov (field of view in degrees)
    std::optional<double> ra;            // -ra (hint RA in degrees)
    std::optional<double> spd;           // -spd (South Pole Distance = 90 - Dec)
    double searchRadius{180.0};          // -r (search radius in degrees)

    // ==================== Solving Parameters ====================
    int speed{0};                        // -speed (1-4, 0=auto)
    int maxStars{500};                   // -s (max stars for solving)
    double tolerance{0.007};             // -t (hash code tolerance)
    std::string database;                // -d (database path)
    int downsample{0};                   // -z (downsample factor)

    // ==================== Output Options ====================
    bool update{false};                  // -update (update FITS header)
    bool analyse{false};                 // -analyse (HFD, background analysis)
    bool annotate{false};                // -annotate (annotate solved image)
    std::string wcsFile;                 // -wcs (output WCS file)

    // ==================== Advanced Options ====================
    bool useSIP{false};                  // -sip (SIP polynomial distortion)
    bool useTriples{false};              // -triples (use triples for sparse fields)
    bool slow{false};                    // -slow (50% overlap search)

    // ==================== Photometry ====================
    bool photometry{false};              // -photometry
    std::string photometryFile;          // -o (photometry output file)

    // ==================== Logging ====================
    bool verbose{false};                 // -log
    std::string logFile;                 // -logfile
};

/**
 * @brief Builder for ASTAP command line
 */
class OptionsBuilder {
public:
    /**
     * @brief Construct builder with solver path
     */
    explicit OptionsBuilder(std::string_view solverPath);

    /**
     * @brief Set image file to solve
     */
    auto setImageFile(std::string_view imagePath) -> OptionsBuilder&;

    /**
     * @brief Apply options structure
     */
    auto applyOptions(const Options& options) -> OptionsBuilder&;

    /**
     * @brief Set field of view in degrees
     */
    auto setFov(double fovDegrees) -> OptionsBuilder&;

    /**
     * @brief Set position hint (RA/Dec in degrees)
     */
    auto setPositionHint(double ra, double dec) -> OptionsBuilder&;

    /**
     * @brief Set search radius in degrees
     */
    auto setSearchRadius(double radius) -> OptionsBuilder&;

    /**
     * @brief Set speed mode (1-4, higher = faster but less accurate)
     */
    auto setSpeed(int speed) -> OptionsBuilder&;

    /**
     * @brief Set database path
     */
    auto setDatabase(std::string_view path) -> OptionsBuilder&;

    /**
     * @brief Set downsample factor
     */
    auto setDownsample(int factor) -> OptionsBuilder&;

    /**
     * @brief Enable FITS header update with WCS
     */
    auto setUpdate(bool update = true) -> OptionsBuilder&;

    /**
     * @brief Enable analysis mode (HFD calculation)
     */
    auto setAnalyse(bool analyse = true) -> OptionsBuilder&;

    /**
     * @brief Build process configuration
     */
    [[nodiscard]] auto build() const -> ProcessConfig;

    /**
     * @brief Build command line string
     */
    [[nodiscard]] auto toString() const -> std::string;

private:
    std::string solverPath_;
    std::string imagePath_;
    Options options_;
};

/**
 * @brief Create default options for quick solving
 */
[[nodiscard]] inline auto createQuickSolveOptions() -> Options {
    Options opts;
    opts.speed = 2;
    opts.maxStars = 500;
    return opts;
}

/**
 * @brief Create options for high-accuracy solving
 */
[[nodiscard]] inline auto createPreciseSolveOptions() -> Options {
    Options opts;
    opts.speed = 1;
    opts.maxStars = 1000;
    opts.useSIP = true;
    return opts;
}

/**
 * @brief Create options with position hint
 */
[[nodiscard]] inline auto createHintedSolveOptions(double ra, double dec,
                                                   double fov,
                                                   double radius = 10.0)
    -> Options {
    Options opts;
    opts.ra = ra;
    opts.spd = 90.0 - dec;  // Convert Dec to SPD
    opts.fov = fov;
    opts.searchRadius = radius;
    opts.speed = 2;
    return opts;
}

}  // namespace lithium::client::astap
