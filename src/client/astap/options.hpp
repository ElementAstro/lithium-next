/*
 * options.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASTAP solver options with modern C++ builder pattern
             Updated based on ASTAP 2025.x documentation

*************************************************/

#pragma once

#include "../common/process_runner.hpp"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace lithium::client::astap {

/**
 * @brief Database type for ASTAP
 */
enum class DatabaseType {
    Auto,  // Automatic selection
    D05,   // 500 stars/sq degree, smallest
    D20,   // 2000 stars/sq degree
    D50,   // 5000 stars/sq degree
    D80,   // 8000 stars/sq degree, largest
    V05,   // Photometry database (Johnson-V) 500/sq degree
    V50,   // Photometry database (Johnson-V) 5000/sq degree
    G05,   // Wide field database
    W08    // Wide field mag 8
};

[[nodiscard]] constexpr auto databaseTypeToString(DatabaseType db) noexcept
    -> std::string_view {
    switch (db) {
        case DatabaseType::D05:
            return "d05";
        case DatabaseType::D20:
            return "d20";
        case DatabaseType::D50:
            return "d50";
        case DatabaseType::D80:
            return "d80";
        case DatabaseType::V05:
            return "v05";
        case DatabaseType::V50:
            return "v50";
        case DatabaseType::G05:
            return "g05";
        case DatabaseType::W08:
            return "w08";
        default:
            return "auto";
    }
}

/**
 * @brief ASTAP solver options
 *
 * Comprehensive options structure covering all ASTAP command line parameters
 * as of version 2025.x
 */
struct Options {
    // ==================== Basic Options ====================
    std::optional<double> fov;   // -fov (field of view in degrees, image height)
    std::optional<double> ra;    // -ra (hint RA in degrees)
    std::optional<double> spd;   // -spd (South Pole Distance = 90 - Dec)
    double searchRadius{180.0};  // -r (search radius in degrees)

    // ==================== Solving Parameters ====================
    int speed{0};             // -speed (1-4, 0=auto, higher=faster but less accurate)
    int maxStars{500};        // -s (max stars for building quads, default 500)
    double tolerance{0.007};  // -t (hash code tolerance, default 0.007)
    std::string database;     // -d (database path or type like "d50")
    int downsample{0};        // -z (downsample factor, 0=auto for large images)
    DatabaseType databaseType{DatabaseType::Auto};  // Database selection

    // ==================== NEW: Star Detection Options ====================
    std::optional<int> minStars;        // Minimum stars required for solving
    std::optional<double> minStar;      // -min_star (minimum star flux)
    std::optional<double> saturation;   // -saturation (saturation level)
    bool force{false};                  // -force (force solving even if already solved)
    bool extractOnly{false};            // -extract (extract stars only, no solving)

    // ==================== Output Options ====================
    bool update{false};    // -update (update FITS header with WCS)
    bool analyse{false};   // -analyse (HFD, background, noise analysis)
    bool annotate{false};  // -annotate (create annotated image)
    std::string wcsFile;   // -wcs (output WCS file path)
    std::optional<std::string> outputDir;  // -o (output directory)

    // ==================== Advanced Options ====================
    bool useSIP{false};       // -sip (use SIP polynomial distortion model)
    bool useTriples{false};   // -triples (use triples for sparse star fields)
    bool slow{false};         // -slow (50% overlap search for difficult fields)
    bool quadrant{false};     // -quad (process image in quadrants)
    std::optional<int> maxTrials;  // -m (maximum solve trials)

    // ==================== Photometry ====================
    bool photometry{false};       // -photometry (enable photometry)
    std::string photometryFile;   // Output photometry file
    bool usePhotometryDb{false};  // Use V05/V50 photometry database

    // ==================== Logging ====================
    bool verbose{false};   // -log (enable logging)
    std::string logFile;   // -logfile (log file path)
    bool silent{false};    // -silent (suppress all output)

    // ==================== NEW: Distortion and Calibration ====================
    std::optional<int> sipOrder;    // SIP polynomial order (default 3)
    bool autoRotate{false};         // Auto-rotate image based on WCS
    std::optional<double> pixelScale;  // Pixel scale hint in arcsec/pixel

    /**
     * @brief Convert options to JSON
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Create options from JSON
     */
    static auto fromJson(const nlohmann::json& json) -> Options;
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

/**
 * @brief Create options for blind solving (no hints)
 */
[[nodiscard]] inline auto createBlindSolveOptions() -> Options {
    Options opts;
    opts.speed = 2;
    opts.maxStars = 500;
    opts.searchRadius = 180.0;  // Full sky search
    opts.databaseType = DatabaseType::D50;  // Use larger database
    return opts;
}

/**
 * @brief Create options for photometry analysis
 */
[[nodiscard]] inline auto createPhotometryOptions() -> Options {
    Options opts;
    opts.photometry = true;
    opts.analyse = true;
    opts.usePhotometryDb = true;
    opts.databaseType = DatabaseType::V50;  // Photometry database
    return opts;
}

/**
 * @brief Create options for star extraction only (no solving)
 */
[[nodiscard]] inline auto createExtractionOptions() -> Options {
    Options opts;
    opts.extractOnly = true;
    opts.analyse = true;
    return opts;
}

/**
 * @brief Create options for wide-field images (>10 degrees FOV)
 */
[[nodiscard]] inline auto createWideFieldOptions(double fov) -> Options {
    Options opts;
    opts.fov = fov;
    opts.databaseType = DatabaseType::W08;  // Wide field database
    opts.speed = 2;
    opts.downsample = 2;  // Downsample for faster processing
    return opts;
}

}  // namespace lithium::client::astap
