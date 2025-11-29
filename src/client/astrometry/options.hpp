/*
 * options.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: Astrometry.net solver options with modern C++ builder pattern

*************************************************/

#pragma once

#include "../common/process_runner.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace lithium::client::astrometry {

/**
 * @brief Scale units for Astrometry.net
 */
enum class ScaleUnits {
    DegWidth,       // degwidth
    ArcminWidth,    // arcminwidth
    ArcsecPerPix,   // arcsecperpix
    FocalMm         // focalmm
};

[[nodiscard]] constexpr auto scaleUnitsToString(ScaleUnits units) noexcept
    -> std::string_view {
    switch (units) {
        case ScaleUnits::DegWidth:
            return "degwidth";
        case ScaleUnits::ArcminWidth:
            return "arcminwidth";
        case ScaleUnits::ArcsecPerPix:
            return "arcsecperpix";
        case ScaleUnits::FocalMm:
            return "focalmm";
    }
    return "arcsecperpix";
}

/**
 * @brief Parity options
 */
enum class Parity { Auto, Positive, Negative };

[[nodiscard]] constexpr auto parityToString(Parity parity) noexcept
    -> std::string_view {
    switch (parity) {
        case Parity::Positive:
            return "pos";
        case Parity::Negative:
            return "neg";
        default:
            return "";
    }
}

/**
 * @brief Astrometry.net solver options
 *
 * Comprehensive options structure covering all solve-field parameters
 */
struct Options {
    // ==================== Basic Options ====================
    std::optional<std::string> backendConfig;  // --backend-config
    std::optional<std::string> config;         // --config
    bool batch{false};                         // --batch
    bool noPlots{true};                        // --no-plots
    bool overwrite{true};                      // --overwrite
    bool skipSolved{false};                    // --skip-solved
    bool continueRun{false};                   // --continue
    bool timestamp{false};                     // --timestamp
    bool noDeleteTemp{false};                  // --no-delete-temp

    // ==================== Scale Options ====================
    std::optional<double> scaleLow;                // --scale-low
    std::optional<double> scaleHigh;               // --scale-high
    std::optional<ScaleUnits> scaleUnits;          // --scale-units
    bool guessScale{false};                        // --guess-scale

    // ==================== Position Options ====================
    std::optional<double> ra;                      // --ra (degrees)
    std::optional<double> dec;                     // --dec (degrees)
    std::optional<double> radius;                  // --radius (degrees)

    // ==================== Processing Options ====================
    std::optional<int> depth;                      // --depth
    std::optional<int> objs;                       // --objs
    std::optional<int> cpuLimit;                   // --cpulimit (seconds)
    std::optional<int> downsample;                 // --downsample
    bool invert{false};                            // --invert
    bool noBackgroundSubtraction{false};           // --no-background-subtraction
    std::optional<float> sigma;                    // --sigma
    std::optional<float> nsigma;                   // --nsigma
    bool noRemoveLines{false};                     // --no-remove-lines
    std::optional<int> uniformize;                 // --uniformize
    bool noVerifyUniformize{false};                // --no-verify-uniformize
    bool noVerifyDedup{false};                     // --no-verify-dedup
    bool resort{false};                            // --resort

    // ==================== Parity and Tolerance ====================
    std::optional<Parity> parity;                  // --parity
    std::optional<double> codeTolerance;           // --code-tolerance
    std::optional<int> pixelError;                 // --pixel-error

    // ==================== Quad Size ====================
    std::optional<double> quadSizeMin;             // --quad-size-min
    std::optional<double> quadSizeMax;             // --quad-size-max

    // ==================== Odds ====================
    std::optional<double> oddsTuneUp;              // --odds-to-tune-up
    std::optional<double> oddsSolve;               // --odds-to-solve
    std::optional<double> oddsReject;              // --odds-to-reject
    std::optional<double> oddsStopLooking;         // --odds-to-stop-looking

    // ==================== Output Options ====================
    std::optional<std::string> newFits;            // --new-fits
    std::optional<std::string> wcs;                // --wcs
    std::optional<std::string> corr;               // --corr
    std::optional<std::string> match;              // --match
    std::optional<std::string> rdls;               // --rdls
    std::optional<std::string> indexXyls;          // --index-xyls
    bool tagAll{false};                            // --tag-all

    // ==================== WCS Options ====================
    bool crpixCenter{false};                       // --crpix-center
    std::optional<int> crpixX;                     // --crpix-x
    std::optional<int> crpixY;                     // --crpix-y
    bool noTweak{false};                           // --no-tweak
    std::optional<int> tweakOrder;                 // --tweak-order
    std::optional<std::string> predistort;         // --predistort
    std::optional<double> xscale;                  // --xscale

    // ==================== Verification ====================
    std::optional<std::string> verify;             // --verify
    bool noVerify{false};                          // --no-verify

    // ==================== Source Extractor ====================
    bool useSourceExtractor{false};                // --use-source-extractor
    std::optional<std::string> sourceExtractorConfig;
    std::optional<std::string> sourceExtractorPath;

    // ==================== SCAMP ====================
    std::optional<std::string> scamp;              // --scamp
    std::optional<std::string> scampConfig;        // --scamp-config

    // ==================== Misc ====================
    std::optional<int> extension;                  // --extension
    bool fitsImage{false};                         // --fits-image
    std::optional<std::string> tempDir;            // --temp-dir
    std::optional<std::string> cancel;             // --cancel
    std::optional<std::string> solved;             // --solved
};

/**
 * @brief Builder for Astrometry.net command line
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
     * @brief Set scale range in arcsec/pixel
     */
    auto setScaleRange(double low, double high,
                       ScaleUnits units = ScaleUnits::ArcsecPerPix)
        -> OptionsBuilder&;

    /**
     * @brief Set position hint
     */
    auto setPositionHint(double ra, double dec, double radius = 10.0)
        -> OptionsBuilder&;

    /**
     * @brief Set timeout in seconds
     */
    auto setTimeout(int seconds) -> OptionsBuilder&;

    /**
     * @brief Set downsample factor
     */
    auto setDownsample(int factor) -> OptionsBuilder&;

    /**
     * @brief Enable/disable plots
     */
    auto setNoPlots(bool noPlots = true) -> OptionsBuilder&;

    /**
     * @brief Set output WCS file path
     */
    auto setWcsOutput(std::string_view path) -> OptionsBuilder&;

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
    opts.noPlots = true;
    opts.overwrite = true;
    opts.noVerify = true;
    opts.cpuLimit = 60;
    return opts;
}

/**
 * @brief Create options for blind solving (no hints)
 */
[[nodiscard]] inline auto createBlindSolveOptions() -> Options {
    Options opts;
    opts.noPlots = true;
    opts.overwrite = true;
    opts.guessScale = true;
    opts.cpuLimit = 300;
    return opts;
}

/**
 * @brief Create options for precise solving with hints
 */
[[nodiscard]] inline auto createPreciseSolveOptions(double ra, double dec,
                                                    double radius = 5.0)
    -> Options {
    Options opts;
    opts.noPlots = true;
    opts.overwrite = true;
    opts.ra = ra;
    opts.dec = dec;
    opts.radius = radius;
    opts.tweakOrder = 3;
    opts.cpuLimit = 120;
    return opts;
}

}  // namespace lithium::client::astrometry
