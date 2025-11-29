/*
 * astrometry_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Astrometry.net plate solver client implementation

*************************************************/

#include "astrometry_client.hpp"
#include "options.hpp"

#include "atom/io/io.hpp"
#include "atom/system/command.hpp"
#include "atom/system/process.hpp"
#include "atom/system/software.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace lithium::client {

AstrometryClient::AstrometryClient(std::string name)
    : SolverClient(std::move(name)) {
    logger_ = spdlog::get("astrometry_client");
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("astrometry_client");
    }
    logger_->info("AstrometryClient created: {}", getName());
}

AstrometryClient::~AstrometryClient() {
    if (isConnected()) {
        disconnect();
    }
    logger_->debug("AstrometryClient destroyed: {}", getName());
}

bool AstrometryClient::initialize() {
    logger_->debug("Initializing AstrometryClient");
    setState(ClientState::Initialized);

    if (scanSolver()) {
        setVersion(solverVersion_);
        emitEvent("initialized", solverPath_);
        return true;
    }

    setError(1, "Astrometry.net not found on system");
    return false;
}

bool AstrometryClient::destroy() {
    logger_->debug("Destroying AstrometryClient");

    if (solving_.load()) {
        abort();
    }

    if (isConnected()) {
        disconnect();
    }

    setState(ClientState::Uninitialized);
    emitEvent("destroyed", "");
    return true;
}

bool AstrometryClient::connect(const std::string& target, int /*timeout*/,
                               int /*maxRetry*/) {
    logger_->debug("Connecting to Astrometry.net at: {}", target);
    setState(ClientState::Connecting);

    if (target.empty()) {
        if (!scanSolver()) {
            setError(2, "Cannot find solve-field executable");
            return false;
        }
    } else {
        if (!atom::io::isFileExists(target)) {
            setError(3, "solve-field not found: " + target);
            return false;
        }
        solverPath_ = target;
    }

    setState(ClientState::Connected);
    emitEvent("connected", solverPath_);
    logger_->info("Connected to Astrometry.net at: {}", solverPath_);
    return true;
}

bool AstrometryClient::disconnect() {
    logger_->debug("Disconnecting from Astrometry.net");
    setState(ClientState::Disconnecting);

    if (solving_.load()) {
        abort();
    }

    // Kill any running solve-field processes
    if (atom::system::isProcessRunning("solve-field")) {
        logger_->info("Terminating running solve-field process");
        try {
            atom::system::killProcessByName("solve-field", 15);
        } catch (const std::exception& ex) {
            logger_->warn("Failed to terminate solve-field: {}", ex.what());
        }
    }

    solverPath_.clear();
    solverVersion_.clear();
    setState(ClientState::Disconnected);
    emitEvent("disconnected", "");
    return true;
}

bool AstrometryClient::isConnected() const {
    return !solverPath_.empty() && getState() == ClientState::Connected;
}

std::vector<std::string> AstrometryClient::scan() {
    logger_->debug("Scanning for Astrometry.net installations");
    std::vector<std::string> results;

    std::vector<std::string> searchPaths = {"/usr/bin/solve-field",
                                            "/usr/local/bin/solve-field",
                                            "/opt/astrometry/bin/solve-field"};

    for (const auto& path : searchPaths) {
        if (atom::io::isFileExists(path)) {
            results.push_back(path);
        }
    }

    return results;
}

bool AstrometryClient::scanSolver() {
    logger_->debug("Scanning for solve-field executable");

    if (!solverPath_.empty()) {
        return true;
    }

    auto paths = scan();
    if (!paths.empty()) {
        solverPath_ = paths[0];
        // Try to get version
        try {
            auto output =
                atom::system::executeCommand(solverPath_ + " --version", false);
            // Parse version from output
            std::regex versionRegex(R"((\d+\.\d+(?:\.\d+)?))");
            std::smatch match;
            if (std::regex_search(output, match, versionRegex)) {
                solverVersion_ = match[1].str();
            } else {
                solverVersion_ = "unknown";
            }
        } catch (...) {
            solverVersion_ = "unknown";
        }

        logger_->info("Found solve-field version {} at {}", solverVersion_,
                      solverPath_);
        return true;
    }

    return false;
}

PlateSolveResult AstrometryClient::solve(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH, int /*imageWidth*/, int /*imageHeight*/) {
    auto startTime = std::chrono::steady_clock::now();
    lastResult_.clear();

    logger_->debug("Starting plate solve for: {}", imageFilePath);

    if (!isConnected()) {
        lastResult_.errorMessage = "Solver not connected";
        setError(10, lastResult_.errorMessage);
        return lastResult_;
    }

    if (!atom::io::isFileExists(imageFilePath)) {
        lastResult_.errorMessage = "Image file not found: " + imageFilePath;
        setError(11, lastResult_.errorMessage);
        return lastResult_;
    }

    solving_.store(true);
    abortRequested_.store(false);
    emitEvent("solve_started", imageFilePath);

    bool success = executeSolve(imageFilePath, initialCoordinates, fovW, fovH);

    if (abortRequested_.load()) {
        lastResult_.errorMessage = "Solve aborted by user";
        solving_.store(false);
        emitEvent("solve_aborted", imageFilePath);
        return lastResult_;
    }

    if (success) {
        // Read WCS from output file using new parser
        std::string wcsFile = getOutputPath(imageFilePath);
        if (atom::io::isFileExists(wcsFile)) {
            auto wcsResult = AstrometryOutputParser::parseWcsFile(wcsFile);
            if (wcsResult) {
                lastResult_ = wcsToResult(*wcsResult);
            } else {
                lastResult_.errorMessage = "Failed to parse WCS file";
            }
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    lastResult_.solveTime =
        std::chrono::duration<double>(endTime - startTime).count();

    solving_.store(false);

    if (lastResult_.success) {
        logger_->info(
            "Solve successful: RA={:.4f}, Dec={:.4f}, Scale={:.2f}\"/px",
            lastResult_.coordinates.ra, lastResult_.coordinates.dec,
            lastResult_.pixelScale);
        emitEvent("solve_completed", imageFilePath);
    } else {
        logger_->error("Solve failed for: {}", imageFilePath);
        emitEvent("solve_failed", lastResult_.errorMessage);
    }

    return lastResult_;
}

void AstrometryClient::abort() {
    if (!solving_.load()) {
        return;
    }

    logger_->info("Aborting Astrometry.net solve");
    abortRequested_.store(true);

    // Use ProcessRunner to abort
    processRunner_.abort();

    // Also try to kill by name as fallback
    try {
        atom::system::killProcessByName("solve-field", 15);
    } catch (const std::exception& ex) {
        logger_->warn("Failed to kill solve-field: {}", ex.what());
    }

    SolverClient::abort();
}

std::string AstrometryClient::buildCommand(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH) {
    std::ostringstream cmd;
    cmd << "\"" << solverPath_ << "\"";
    cmd << " \"" << imageFilePath << "\"";

    // ==================== Basic Options ====================

    // No plots by default
    if (astrometryOptions_.noPlots) {
        cmd << " --no-plots";
    }

    // Overwrite existing files
    if (astrometryOptions_.overwrite) {
        cmd << " --overwrite";
    }

    // Skip already solved
    if (astrometryOptions_.skipSolved) {
        cmd << " --skip-solved";
    }

    // Continue from previous run
    if (astrometryOptions_.continueRun) {
        cmd << " --continue";
    }

    // Timestamp logging
    if (astrometryOptions_.timestamp) {
        cmd << " --timestamp";
    }

    // Keep temp files
    if (astrometryOptions_.noDeleteTemp) {
        cmd << " --no-delete-temp";
    }

    // Batch mode
    if (astrometryOptions_.batch) {
        cmd << " --batch";
    }

    // ==================== Scale Options ====================

    if (options_.scaleLow || astrometryOptions_.scaleLow) {
        double low =
            astrometryOptions_.scaleLow.value_or(options_.scaleLow.value_or(0));
        cmd << " --scale-low " << low;
    }
    if (options_.scaleHigh || astrometryOptions_.scaleHigh) {
        double high = astrometryOptions_.scaleHigh.value_or(
            options_.scaleHigh.value_or(0));
        cmd << " --scale-high " << high;
    }
    if (astrometryOptions_.scaleUnits) {
        cmd << " --scale-units " << *astrometryOptions_.scaleUnits;
    }
    if (astrometryOptions_.guessScale) {
        cmd << " --guess-scale";
    }

    // ==================== Position Options ====================

    if (initialCoordinates && initialCoordinates->isValid()) {
        cmd << " --ra " << initialCoordinates->ra;
        cmd << " --dec " << initialCoordinates->dec;
        if (options_.searchRadius) {
            cmd << " --radius " << *options_.searchRadius;
        } else if (astrometryOptions_.radius) {
            cmd << " --radius " << *astrometryOptions_.radius;
        }
    } else if (astrometryOptions_.ra && astrometryOptions_.dec) {
        cmd << " --ra " << *astrometryOptions_.ra;
        cmd << " --dec " << *astrometryOptions_.dec;
        if (astrometryOptions_.radius) {
            cmd << " --radius " << *astrometryOptions_.radius;
        }
    }

    // ==================== Processing Options ====================

    if (astrometryOptions_.depth) {
        cmd << " --depth " << *astrometryOptions_.depth;
    }

    if (astrometryOptions_.objs) {
        cmd << " --objs " << *astrometryOptions_.objs;
    }

    if (options_.downsample || astrometryOptions_.downsample) {
        int ds = astrometryOptions_.downsample.value_or(
            options_.downsample.value_or(0));
        if (ds > 0) {
            cmd << " --downsample " << ds;
        }
    }

    if (astrometryOptions_.cpuLimit) {
        cmd << " --cpulimit " << *astrometryOptions_.cpuLimit;
    } else if (options_.timeout > 0) {
        cmd << " --cpulimit " << options_.timeout;
    }

    if (astrometryOptions_.invert) {
        cmd << " --invert";
    }

    if (astrometryOptions_.noBackgroundSubtraction) {
        cmd << " --no-background-subtraction";
    }

    if (astrometryOptions_.sigma) {
        cmd << " --sigma " << *astrometryOptions_.sigma;
    }

    if (astrometryOptions_.nsigma) {
        cmd << " --nsigma " << *astrometryOptions_.nsigma;
    }

    if (astrometryOptions_.noRemoveLines) {
        cmd << " --no-remove-lines";
    }

    if (astrometryOptions_.uniformize) {
        cmd << " --uniformize " << *astrometryOptions_.uniformize;
    }

    if (astrometryOptions_.noVerifyUniformize) {
        cmd << " --no-verify-uniformize";
    }

    if (astrometryOptions_.noVerifyDedup) {
        cmd << " --no-verify-dedup";
    }

    if (astrometryOptions_.resort) {
        cmd << " --resort";
    }

    // ==================== Parity and Tolerance ====================

    if (astrometryOptions_.parity) {
        cmd << " --parity " << *astrometryOptions_.parity;
    }

    if (astrometryOptions_.codeTolerance) {
        cmd << " --code-tolerance " << *astrometryOptions_.codeTolerance;
    }

    if (astrometryOptions_.pixelError) {
        cmd << " --pixel-error " << *astrometryOptions_.pixelError;
    }

    // ==================== Quad Size ====================

    if (astrometryOptions_.quadSizeMin) {
        cmd << " --quad-size-min " << *astrometryOptions_.quadSizeMin;
    }

    if (astrometryOptions_.quadSizeMax) {
        cmd << " --quad-size-max " << *astrometryOptions_.quadSizeMax;
    }

    // ==================== Odds ====================

    if (astrometryOptions_.oddsTuneUp) {
        cmd << " --odds-to-tune-up " << *astrometryOptions_.oddsTuneUp;
    }

    if (astrometryOptions_.oddsSolve) {
        cmd << " --odds-to-solve " << *astrometryOptions_.oddsSolve;
    }

    if (astrometryOptions_.oddsReject) {
        cmd << " --odds-to-reject " << *astrometryOptions_.oddsReject;
    }

    if (astrometryOptions_.oddsStopLooking) {
        cmd << " --odds-to-stop-looking "
            << *astrometryOptions_.oddsStopLooking;
    }

    // ==================== Output Options ====================

    if (astrometryOptions_.newFits) {
        cmd << " --new-fits \"" << *astrometryOptions_.newFits << "\"";
    }

    if (astrometryOptions_.wcs) {
        cmd << " --wcs \"" << *astrometryOptions_.wcs << "\"";
    }

    if (astrometryOptions_.corr) {
        cmd << " --corr \"" << *astrometryOptions_.corr << "\"";
    }

    if (astrometryOptions_.match) {
        cmd << " --match \"" << *astrometryOptions_.match << "\"";
    }

    if (astrometryOptions_.rdls) {
        cmd << " --rdls \"" << *astrometryOptions_.rdls << "\"";
    }

    if (astrometryOptions_.indexXyls) {
        cmd << " --index-xyls \"" << *astrometryOptions_.indexXyls << "\"";
    }

    // ==================== WCS Options ====================

    if (astrometryOptions_.crpixCenter) {
        cmd << " --crpix-center";
    }

    if (astrometryOptions_.crpixX) {
        cmd << " --crpix-x " << *astrometryOptions_.crpixX;
    }

    if (astrometryOptions_.crpixY) {
        cmd << " --crpix-y " << *astrometryOptions_.crpixY;
    }

    if (astrometryOptions_.noTweak) {
        cmd << " --no-tweak";
    } else if (astrometryOptions_.tweakOrder) {
        cmd << " --tweak-order " << *astrometryOptions_.tweakOrder;
    }

    if (astrometryOptions_.predistort) {
        cmd << " --predistort \"" << *astrometryOptions_.predistort << "\"";
    }

    if (astrometryOptions_.xscale) {
        cmd << " --xscale " << *astrometryOptions_.xscale;
    }

    // ==================== Verification ====================

    if (astrometryOptions_.verify) {
        cmd << " --verify \"" << *astrometryOptions_.verify << "\"";
    }

    if (astrometryOptions_.noVerify) {
        cmd << " --no-verify";
    }

    // ==================== Source Extractor ====================

    if (astrometryOptions_.useSourceExtractor) {
        cmd << " --use-source-extractor";
        if (astrometryOptions_.sourceExtractorPath) {
            cmd << " --source-extractor-path \""
                << *astrometryOptions_.sourceExtractorPath << "\"";
        }
        if (astrometryOptions_.sourceExtractorConfig) {
            cmd << " --source-extractor-config \""
                << *astrometryOptions_.sourceExtractorConfig << "\"";
        }
    }

    // ==================== SCAMP ====================

    if (astrometryOptions_.scamp) {
        cmd << " --scamp \"" << *astrometryOptions_.scamp << "\"";
    }

    if (astrometryOptions_.scampConfig) {
        cmd << " --scamp-config \"" << *astrometryOptions_.scampConfig << "\"";
    }

    // ==================== Config Files ====================

    if (astrometryOptions_.config) {
        cmd << " --config \"" << *astrometryOptions_.config << "\"";
    }

    if (astrometryOptions_.backendConfig) {
        cmd << " --backend-config \"" << *astrometryOptions_.backendConfig
            << "\"";
    }

    // ==================== FITS Extension ====================

    if (astrometryOptions_.extension) {
        cmd << " --extension " << *astrometryOptions_.extension;
    }

    if (astrometryOptions_.fitsImage) {
        cmd << " --fits-image";
    }

    // ==================== Temp Directory ====================

    if (astrometryOptions_.tempDir) {
        cmd << " --temp-dir \"" << *astrometryOptions_.tempDir << "\"";
    }

    // ==================== Cancel/Solved Files ====================

    if (astrometryOptions_.cancel) {
        cmd << " --cancel \"" << *astrometryOptions_.cancel << "\"";
    }

    if (astrometryOptions_.solved) {
        cmd << " --solved \"" << *astrometryOptions_.solved << "\"";
    }

    return cmd.str();
}

bool AstrometryClient::executeSolve(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH) {
    // Build command using new options builder
    astrometry::OptionsBuilder builder(solverPath_);
    builder.setImageFile(imageFilePath).applyOptions(astrometryOptions_);

    // Apply position hint if provided
    if (initialCoordinates && initialCoordinates->isValid()) {
        double radius = options_.searchRadius.value_or(
            astrometryOptions_.radius.value_or(10.0));
        builder.setPositionHint(initialCoordinates->ra, initialCoordinates->dec,
                                radius);
    }

    // Apply scale hints from FOV if provided
    if (fovW > 0 && fovH > 0) {
        // Convert FOV to arcsec/pixel (approximate)
        double avgFov = (fovW + fovH) / 2.0;
        double scaleLow = avgFov * 0.8 * 3600.0 / 1000.0;  // Rough estimate
        double scaleHigh = avgFov * 1.2 * 3600.0 / 1000.0;
        builder.setScaleRange(scaleLow, scaleHigh);
    }

    // Apply timeout
    if (options_.timeout > 0) {
        builder.setTimeout(options_.timeout);
    }

    auto config = builder.build();
    logger_->debug("Executing: {}", ProcessRunner::buildCommandLine(config));

    // Execute using ProcessRunner
    auto result = processRunner_.execute(config);

    if (!result) {
        lastResult_.errorMessage = "Failed to execute solve-field";
        return false;
    }

    // Check for success indicators
    if (AstrometryOutputParser::isSuccessful(result->stdOut)) {
        return true;
    }

    // Check for error
    if (auto error = AstrometryOutputParser::extractError(result->stdOut)) {
        lastResult_.errorMessage = *error;
        return false;
    }

    // Check if WCS file was created
    std::string wcsFile = getOutputPath(imageFilePath);
    return atom::io::isFileExists(wcsFile);
}

PlateSolveResult AstrometryClient::wcsToResult(const WCSData& wcs) {
    PlateSolveResult result;
    result.success = wcs.isValid();
    result.coordinates.ra = wcs.getRaDeg();
    result.coordinates.dec = wcs.getDecDeg();
    result.pixelScale = wcs.getPixelScaleArcsec();
    result.positionAngle = wcs.getRotationDeg();
    return result;
}

std::string AstrometryClient::getOutputPath(
    const std::string& imageFilePath) const {
    std::filesystem::path path(imageFilePath);
    return (path.parent_path() / (path.stem().string() + ".wcs")).string();
}

std::vector<std::string> AstrometryClient::getIndexFiles(
    const std::vector<std::string>& directories) const {
    std::vector<std::string> results;
    std::vector<std::string> searchDirs = directories;

    if (searchDirs.empty()) {
        searchDirs = {"/usr/share/astrometry", "/usr/local/share/astrometry",
                      "~/.local/share/astrometry"};
    }

    for (const auto& dir : searchDirs) {
        if (!std::filesystem::exists(dir))
            continue;

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".fits" &&
                entry.path().filename().string().find("index-") == 0) {
                results.push_back(entry.path().string());
            }
        }
    }

    return results;
}

bool AstrometryClient::isAstrometryInstalled() {
    return atom::system::checkSoftwareInstalled("solve-field");
}

std::string AstrometryClient::getDefaultPath() {
    return "/usr/bin/solve-field";
}

// Register with client registry
LITHIUM_REGISTER_CLIENT(AstrometryClient, "astrometry",
                        "Astrometry.net Plate Solver", ClientType::Solver,
                        "1.0.0", "solve-field")

}  // namespace lithium::client
