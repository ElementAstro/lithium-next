/*
 * options.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: StellarSolver options with comprehensive parameter support
             Based on StellarSolver 2.5 API

*************************************************/

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lithium::client::stellarsolver {

/**
 * @brief Parameter profile presets
 */
enum class Profile {
    Default,              // Default balanced settings
    SingleThreadSolving,  // Single-threaded for stability
    ParallelLargeScale,   // Parallel for wide-field images
    ParallelSmallScale,   // Parallel for narrow-field images
    SmallScaleStars,      // Optimized for small/faint stars
    Custom                // Custom user-defined settings
};

[[nodiscard]] constexpr auto profileToString(Profile profile) noexcept
    -> std::string_view {
    switch (profile) {
        case Profile::Default:
            return "default";
        case Profile::SingleThreadSolving:
            return "singleThread";
        case Profile::ParallelLargeScale:
            return "parallelLarge";
        case Profile::ParallelSmallScale:
            return "parallelSmall";
        case Profile::SmallScaleStars:
            return "smallStars";
        case Profile::Custom:
            return "custom";
    }
    return "default";
}

/**
 * @brief Scale units for FOV specification
 */
enum class ScaleUnits {
    DegWidth,      // Degrees (image width)
    ArcMinWidth,   // Arc minutes (image width)
    ArcSecPerPix,  // Arcseconds per pixel
    FocalMM        // Focal length in mm (with sensor size)
};

[[nodiscard]] constexpr auto scaleUnitsToString(ScaleUnits units) noexcept
    -> std::string_view {
    switch (units) {
        case ScaleUnits::DegWidth:
            return "degwidth";
        case ScaleUnits::ArcMinWidth:
            return "arcminwidth";
        case ScaleUnits::ArcSecPerPix:
            return "arcsecperpix";
        case ScaleUnits::FocalMM:
            return "focalmm";
    }
    return "arcsecperpix";
}

/**
 * @brief Convolution filter type for star extraction
 */
enum class ConvFilterType {
    Default,   // Default filter
    Gaussian,  // Gaussian filter
    Mexhat,    // Mexican hat filter
    Custom     // User-defined filter
};

/**
 * @brief Multi-algorithm mode for parallel solving
 */
enum class MultiAlgorithm {
    None,           // Single solver instance
    FITS,           // Use FITS subdivisions
    ParallelSolve,  // Parallel solve attempts
    ParallelAll     // Parallel extraction and solving
};

/**
 * @brief StellarSolver options
 *
 * Comprehensive options structure for StellarSolver library
 */
struct Options {
    // ==================== Profile ====================
    Profile profile{Profile::Default};

    // ==================== Scale Settings ====================
    std::optional<double> scaleLow;   // Lower scale bound
    std::optional<double> scaleHigh;  // Upper scale bound
    ScaleUnits scaleUnits{ScaleUnits::ArcSecPerPix};

    // ==================== Position Hints ====================
    std::optional<double> searchRA;      // RA hint in degrees
    std::optional<double> searchDec;     // Dec hint in degrees
    std::optional<double> searchRadius;  // Search radius in degrees

    // ==================== Processing Options ====================
    bool calculateHFR{false};    // Calculate Half-Flux Radius
    bool extractOnly{false};     // Extract stars only, no solving
    int downsample{0};           // Downsample factor (0=auto)
    bool autoDownsample{true};   // Enable automatic downsampling
    int partitionThreads{4};     // Number of parallel threads

    // ==================== Index File Settings ====================
    std::vector<std::string> indexFolders;  // Index file directories
    int indexToUse{-1};                     // Specific index to use (-1=all)
    int healpixToUse{-1};                   // Specific healpix to use

    // ==================== External Program Paths ====================
    std::string sextractorPath;  // SExtractor path (if using external)
    std::string solverPath;      // solve-field path (if using external)
    std::string configFilePath;  // astrometry.cfg path
    std::string wcsPath;         // wcsinfo path

    // ==================== Convolution Filter ====================
    ConvFilterType convFilterType{ConvFilterType::Default};
    double convFilterFWHM{3.5};  // Filter FWHM in pixels
    std::vector<float> customFilter;  // Custom filter coefficients

    // ==================== Star Extraction Parameters ====================
    int minArea{5};             // Minimum star area in pixels
    double deblendNThresh{32};  // Deblending threshold count
    double deblendMinCont{0.005};  // Deblending minimum contrast
    bool cleanResults{true};    // Clean extracted star list
    double cleanParam{1.0};     // Cleaning parameter

    // ==================== Quad Generation ====================
    double minWidth{0.1};    // Minimum quad width (degrees)
    double maxWidth{30.0};   // Maximum quad width (degrees)
    int quadSizeMin{4};      // Minimum stars in quad
    int quadSizeMax{8};      // Maximum stars in quad

    // ==================== Solving Parameters ====================
    double tolerance{0.01};  // Match tolerance
    int maxIterations{20};   // Maximum solve iterations
    bool resort{false};      // Resort stars by flux
    bool keepTemp{false};    // Keep temporary files

    // ==================== Multi-Algorithm ====================
    MultiAlgorithm multiAlgorithm{MultiAlgorithm::FITS};
    bool useParallel{true};  // Enable parallel processing

    // ==================== Subframe ====================
    std::optional<int> subframeX;       // Subframe X offset
    std::optional<int> subframeY;       // Subframe Y offset
    std::optional<int> subframeWidth;   // Subframe width
    std::optional<int> subframeHeight;  // Subframe height

    // ==================== Output ====================
    bool generateWCS{true};    // Generate WCS solution
    bool saveSolution{false};  // Save solution to file
    std::string outputPath;    // Output file path

    /**
     * @brief Convert options to JSON
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Create options from JSON
     */
    static auto fromJson(const nlohmann::json& json) -> Options;

    /**
     * @brief Apply a profile preset
     */
    void applyProfile(Profile preset);
};

/**
 * @brief Create default options
 */
[[nodiscard]] inline auto createDefaultOptions() -> Options {
    Options opts;
    opts.profile = Profile::Default;
    opts.calculateHFR = true;
    opts.autoDownsample = true;
    return opts;
}

/**
 * @brief Create options for HFR calculation only
 */
[[nodiscard]] inline auto createHfrOnlyOptions() -> Options {
    Options opts;
    opts.extractOnly = true;
    opts.calculateHFR = true;
    opts.minArea = 5;
    opts.convFilterFWHM = 3.5;
    return opts;
}

/**
 * @brief Create options for quick solving with position hint
 */
[[nodiscard]] inline auto createQuickSolveOptions(double ra, double dec,
                                                   double radius = 15.0)
    -> Options {
    Options opts;
    opts.profile = Profile::ParallelSmallScale;
    opts.searchRA = ra;
    opts.searchDec = dec;
    opts.searchRadius = radius;
    opts.useParallel = true;
    return opts;
}

/**
 * @brief Create options for blind solving
 */
[[nodiscard]] inline auto createBlindSolveOptions() -> Options {
    Options opts;
    opts.profile = Profile::ParallelLargeScale;
    opts.useParallel = true;
    opts.multiAlgorithm = MultiAlgorithm::ParallelSolve;
    opts.maxIterations = 50;
    return opts;
}

/**
 * @brief Create options for small/faint stars
 */
[[nodiscard]] inline auto createSmallStarsOptions() -> Options {
    Options opts;
    opts.profile = Profile::SmallScaleStars;
    opts.convFilterFWHM = 2.0;
    opts.minArea = 3;
    opts.deblendNThresh = 64;
    return opts;
}

/**
 * @brief Create options for wide-field images
 */
[[nodiscard]] inline auto createWideFieldOptions(double fovDegrees) -> Options {
    Options opts;
    opts.profile = Profile::ParallelLargeScale;
    opts.scaleLow = fovDegrees * 0.8;
    opts.scaleHigh = fovDegrees * 1.2;
    opts.scaleUnits = ScaleUnits::DegWidth;
    opts.downsample = 2;
    opts.minWidth = 5.0;
    opts.maxWidth = 180.0;
    return opts;
}

/**
 * @brief Create options optimized for focusing (HFR only, fast)
 */
[[nodiscard]] inline auto createFocusingOptions() -> Options {
    Options opts;
    opts.extractOnly = true;
    opts.calculateHFR = true;
    opts.downsample = 0;  // Auto
    opts.minArea = 5;
    opts.convFilterFWHM = 4.0;  // Larger for defocused stars
    opts.cleanResults = true;
    return opts;
}

}  // namespace lithium::client::stellarsolver
