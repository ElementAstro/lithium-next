/*
 * options.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: ASTAP options builder implementation

*************************************************/

#include "options.hpp"

namespace lithium::client::astap {

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

auto OptionsBuilder::setFov(double fovDegrees) -> OptionsBuilder& {
    options_.fov = fovDegrees;
    return *this;
}

auto OptionsBuilder::setPositionHint(double ra, double dec) -> OptionsBuilder& {
    options_.ra = ra;
    options_.spd = 90.0 - dec;  // Convert Dec to South Pole Distance
    return *this;
}

auto OptionsBuilder::setSearchRadius(double radius) -> OptionsBuilder& {
    options_.searchRadius = radius;
    return *this;
}

auto OptionsBuilder::setSpeed(int speed) -> OptionsBuilder& {
    options_.speed = speed;
    return *this;
}

auto OptionsBuilder::setDatabase(std::string_view path) -> OptionsBuilder& {
    options_.database = path;
    return *this;
}

auto OptionsBuilder::setDownsample(int factor) -> OptionsBuilder& {
    options_.downsample = factor;
    return *this;
}

auto OptionsBuilder::setUpdate(bool update) -> OptionsBuilder& {
    options_.update = update;
    return *this;
}

auto OptionsBuilder::setAnalyse(bool analyse) -> OptionsBuilder& {
    options_.analyse = analyse;
    return *this;
}

auto OptionsBuilder::build() const -> ProcessConfig {
    CommandBuilder cmd(solverPath_);

    // Image file (required)
    if (!imagePath_.empty()) {
        cmd.addOption("-f", imagePath_);
    }

    // Field of view
    if (options_.fov && *options_.fov > 0) {
        cmd.addOption("-fov", std::to_string(*options_.fov));
    }

    // Position hint
    if (options_.ra) {
        cmd.addOption("-ra", std::to_string(*options_.ra));
    }
    if (options_.spd) {
        cmd.addOption("-spd", std::to_string(*options_.spd));
    }

    // Search radius
    if (options_.searchRadius > 0 && options_.searchRadius < 180) {
        cmd.addOption("-r", std::to_string(options_.searchRadius));
    }

    // Speed mode
    if (options_.speed > 0 && options_.speed <= 4) {
        cmd.addOption("-speed", std::to_string(options_.speed));
    }

    // Max stars
    if (options_.maxStars > 0 && options_.maxStars != 500) {
        cmd.addOption("-s", std::to_string(options_.maxStars));
    }

    // Tolerance
    if (options_.tolerance > 0 && options_.tolerance != 0.007) {
        cmd.addOption("-t", std::to_string(options_.tolerance));
    }

    // Database path
    if (!options_.database.empty()) {
        cmd.addOption("-d", options_.database);
    }

    // Downsample
    if (options_.downsample > 0) {
        cmd.addOption("-z", std::to_string(options_.downsample));
    }

    // Output options
    cmd.addFlagIf(options_.update, "-update")
        .addFlagIf(options_.analyse, "-analyse")
        .addFlagIf(options_.annotate, "-annotate");

    if (!options_.wcsFile.empty()) {
        cmd.addOption("-wcs", options_.wcsFile);
    }

    // Advanced options
    cmd.addFlagIf(options_.useSIP, "-sip")
        .addFlagIf(options_.useTriples, "-triples")
        .addFlagIf(options_.slow, "-slow");

    // Photometry
    if (options_.photometry) {
        cmd.addFlag("-photometry");
        if (!options_.photometryFile.empty()) {
            cmd.addOption("-o", options_.photometryFile);
        }
    }

    // Logging
    if (options_.verbose) {
        cmd.addFlag("-log");
        if (!options_.logFile.empty()) {
            cmd.addOption("-logfile", options_.logFile);
        }
    }

    return cmd.build();
}

auto OptionsBuilder::toString() const -> std::string {
    auto config = build();
    return ProcessRunner::buildCommandLine(config);
}

}  // namespace lithium::client::astap
