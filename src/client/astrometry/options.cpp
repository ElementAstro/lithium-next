/*
 * options.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: Astrometry.net options builder implementation

*************************************************/

#include "options.hpp"

namespace lithium::client::astrometry {

OptionsBuilder::OptionsBuilder(std::string_view solverPath)
    : solverPath_(solverPath) {}

auto OptionsBuilder::setImageFile(std::string_view imagePath)
    -> OptionsBuilder& {
    imagePath_ = imagePath;
    return *this;
}

auto OptionsBuilder::applyOptions(const Options& options) -> OptionsBuilder& {
    options_ = options;
    return *this;
}

auto OptionsBuilder::setScaleRange(double low, double high, ScaleUnits units)
    -> OptionsBuilder& {
    options_.scaleLow = low;
    options_.scaleHigh = high;
    options_.scaleUnits = units;
    return *this;
}

auto OptionsBuilder::setPositionHint(double ra, double dec, double radius)
    -> OptionsBuilder& {
    options_.ra = ra;
    options_.dec = dec;
    options_.radius = radius;
    return *this;
}

auto OptionsBuilder::setTimeout(int seconds) -> OptionsBuilder& {
    options_.cpuLimit = seconds;
    return *this;
}

auto OptionsBuilder::setDownsample(int factor) -> OptionsBuilder& {
    options_.downsample = factor;
    return *this;
}

auto OptionsBuilder::setNoPlots(bool noPlots) -> OptionsBuilder& {
    options_.noPlots = noPlots;
    return *this;
}

auto OptionsBuilder::setWcsOutput(std::string_view path) -> OptionsBuilder& {
    options_.wcs = std::string(path);
    return *this;
}

auto OptionsBuilder::build() const -> ProcessConfig {
    CommandBuilder cmd(solverPath_);

    // Add image file first
    if (!imagePath_.empty()) {
        cmd.addArg(imagePath_);
    }

    // Basic options
    cmd.addFlagIf(options_.noPlots, "--no-plots")
        .addFlagIf(options_.overwrite, "--overwrite")
        .addFlagIf(options_.skipSolved, "--skip-solved")
        .addFlagIf(options_.continueRun, "--continue")
        .addFlagIf(options_.timestamp, "--timestamp")
        .addFlagIf(options_.noDeleteTemp, "--no-delete-temp")
        .addFlagIf(options_.batch, "--batch");

    // Scale options
    cmd.addOptional("--scale-low", options_.scaleLow)
        .addOptional("--scale-high", options_.scaleHigh)
        .addFlagIf(options_.guessScale, "--guess-scale");

    if (options_.scaleUnits) {
        cmd.addOption("--scale-units",
                      std::string(scaleUnitsToString(*options_.scaleUnits)));
    }

    // Position options
    if (options_.ra) {
        cmd.addOption("--ra", std::to_string(*options_.ra));
    }
    if (options_.dec) {
        cmd.addOption("--dec", std::to_string(*options_.dec));
    }
    cmd.addOptional("--radius", options_.radius);

    // Processing options
    cmd.addOptional("--depth", options_.depth)
        .addOptional("--objs", options_.objs)
        .addOptional("--cpulimit", options_.cpuLimit)
        .addOptional("--downsample", options_.downsample)
        .addFlagIf(options_.invert, "--invert")
        .addFlagIf(options_.noBackgroundSubtraction,
                   "--no-background-subtraction")
        .addOptional("--sigma", options_.sigma)
        .addOptional("--nsigma", options_.nsigma)
        .addFlagIf(options_.noRemoveLines, "--no-remove-lines")
        .addOptional("--uniformize", options_.uniformize)
        .addFlagIf(options_.noVerifyUniformize, "--no-verify-uniformize")
        .addFlagIf(options_.noVerifyDedup, "--no-verify-dedup")
        .addFlagIf(options_.resort, "--resort");

    // Parity and tolerance
    if (options_.parity && *options_.parity != Parity::Auto) {
        cmd.addOption("--parity",
                      std::string(parityToString(*options_.parity)));
    }
    cmd.addOptional("--code-tolerance", options_.codeTolerance)
        .addOptional("--pixel-error", options_.pixelError);

    // Quad size
    cmd.addOptional("--quad-size-min", options_.quadSizeMin)
        .addOptional("--quad-size-max", options_.quadSizeMax);

    // Odds
    cmd.addOptional("--odds-to-tune-up", options_.oddsTuneUp)
        .addOptional("--odds-to-solve", options_.oddsSolve)
        .addOptional("--odds-to-reject", options_.oddsReject)
        .addOptional("--odds-to-stop-looking", options_.oddsStopLooking);

    // Output options
    cmd.addOptional("--new-fits", options_.newFits)
        .addOptional("--wcs", options_.wcs)
        .addOptional("--corr", options_.corr)
        .addOptional("--match", options_.match)
        .addOptional("--rdls", options_.rdls)
        .addOptional("--index-xyls", options_.indexXyls)
        .addFlagIf(options_.tagAll, "--tag-all");

    // WCS options
    cmd.addFlagIf(options_.crpixCenter, "--crpix-center")
        .addOptional("--crpix-x", options_.crpixX)
        .addOptional("--crpix-y", options_.crpixY)
        .addFlagIf(options_.noTweak, "--no-tweak")
        .addOptional("--tweak-order", options_.tweakOrder)
        .addOptional("--predistort", options_.predistort)
        .addOptional("--xscale", options_.xscale);

    // Verification
    cmd.addOptional("--verify", options_.verify)
        .addFlagIf(options_.noVerify, "--no-verify");

    // Source extractor
    if (options_.useSourceExtractor) {
        cmd.addFlag("--use-source-extractor")
            .addOptional("--source-extractor-config",
                         options_.sourceExtractorConfig)
            .addOptional("--source-extractor-path",
                         options_.sourceExtractorPath);
    }

    // SCAMP
    cmd.addOptional("--scamp", options_.scamp)
        .addOptional("--scamp-config", options_.scampConfig);

    // Config files
    cmd.addOptional("--config", options_.config)
        .addOptional("--backend-config", options_.backendConfig);

    // Misc
    cmd.addOptional("--extension", options_.extension)
        .addFlagIf(options_.fitsImage, "--fits-image")
        .addOptional("--temp-dir", options_.tempDir)
        .addOptional("--cancel", options_.cancel)
        .addOptional("--solved", options_.solved);

    return cmd.build();
}

auto OptionsBuilder::toString() const -> std::string {
    auto config = build();
    return ProcessRunner::buildCommandLine(config);
}

}  // namespace lithium::client::astrometry
