/*
 * astap_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: ASTAP plate solver client implementation

*************************************************/

#include "astap_client.hpp"
#include "options.hpp"

#include "atom/io/io.hpp"
#include "atom/system/command.hpp"
#include "atom/system/process.hpp"
#include "atom/system/software.hpp"

#include <fitsio.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace lithium::client {

AstapClient::AstapClient(std::string name) : SolverClient(std::move(name)) {
    logger_ = spdlog::get("astap_client");
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("astap_client");
    }
    logger_->info("AstapClient created: {}", getName());
}

AstapClient::~AstapClient() {
    if (isConnected()) {
        disconnect();
    }
    logger_->debug("AstapClient destroyed: {}", getName());
}

bool AstapClient::initialize() {
    logger_->debug("Initializing AstapClient");
    setState(ClientState::Initialized);

    if (scanSolver()) {
        setVersion(solverVersion_);
        emitEvent("initialized", solverPath_);
        return true;
    }

    setError(1, "ASTAP not found on system");
    return false;
}

bool AstapClient::destroy() {
    logger_->debug("Destroying AstapClient");

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

bool AstapClient::connect(const std::string& target, int /*timeout*/,
                          int /*maxRetry*/) {
    logger_->debug("Connecting to ASTAP at: {}", target);
    setState(ClientState::Connecting);

    if (target.empty()) {
        // Try to auto-detect
        if (!scanSolver()) {
            setError(2, "Cannot find ASTAP executable");
            return false;
        }
    } else {
        if (!atom::io::isFileExists(target)) {
            setError(3, "ASTAP executable not found: " + target);
            return false;
        }
        solverPath_ = target;
        solverVersion_ = atom::system::getAppVersion(target);
    }

    setState(ClientState::Connected);
    emitEvent("connected", solverPath_);
    logger_->info("Connected to ASTAP at: {}", solverPath_);
    return true;
}

bool AstapClient::disconnect() {
    logger_->debug("Disconnecting from ASTAP");
    setState(ClientState::Disconnecting);

    // Abort any running solve
    if (solving_.load()) {
        abort();
    }

    // Kill any running ASTAP processes
    if (atom::system::isProcessRunning("astap")) {
        logger_->info("Terminating running ASTAP process");
        try {
            atom::system::killProcessByName("astap", 15);
        } catch (const std::exception& ex) {
            logger_->warn("Failed to terminate ASTAP process: {}", ex.what());
        }
    }

    solverPath_.clear();
    solverVersion_.clear();
    setState(ClientState::Disconnected);
    emitEvent("disconnected", "");
    return true;
}

bool AstapClient::isConnected() const {
    return !solverPath_.empty() && getState() == ClientState::Connected;
}

std::vector<std::string> AstapClient::scan() {
    logger_->debug("Scanning for ASTAP installations");
    std::vector<std::string> results;

    // Check common locations
    std::vector<std::string> searchPaths;

#ifdef _WIN32
    searchPaths = {"C:\\Program Files\\astap\\astap.exe",
                   "C:\\Program Files (x86)\\astap\\astap.exe",
                   "C:\\astap\\astap.exe"};
#else
    searchPaths = {"/usr/bin/astap", "/usr/local/bin/astap",
                   "/opt/astap/astap"};
#endif

    for (const auto& path : searchPaths) {
        if (atom::io::isFileExists(path)) {
            results.push_back(path);
            logger_->debug("Found ASTAP at: {}", path);
        }
    }

    // Also try system path
    if (atom::system::checkSoftwareInstalled("astap")) {
        auto sysPath = atom::system::getAppPath("astap");
        if (!sysPath.empty()) {
            std::string pathStr = sysPath.string();
            if (std::find(results.begin(), results.end(), pathStr) ==
                results.end()) {
                results.push_back(pathStr);
            }
        }
    }

    return results;
}

bool AstapClient::scanSolver() {
    logger_->debug("Scanning for ASTAP executable");

    if (!solverPath_.empty()) {
        return true;
    }

    if (!atom::system::checkSoftwareInstalled("astap")) {
        logger_->error("ASTAP not installed on system");
        return false;
    }

    auto astapPath = atom::system::getAppPath("astap");
    if (astapPath.empty()) {
        logger_->error("Cannot find ASTAP path");
        return false;
    }

    solverPath_ = astapPath.string();
    solverVersion_ = atom::system::getAppVersion(solverPath_);

    if (solverVersion_.empty()) {
        solverVersion_ = "unknown";
    }

    logger_->info("Found ASTAP version {} at {}", solverVersion_, solverPath_);
    return true;
}

PlateSolveResult AstapClient::solve(
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
        // Try to read from INI file first (ASTAP output)
        std::string iniFile = imageFilePath;
        auto dotPos = iniFile.rfind('.');
        if (dotPos != std::string::npos) {
            iniFile = iniFile.substr(0, dotPos) + ".ini";
        }

        if (atom::io::isFileExists(iniFile)) {
            auto wcsResult = AstapOutputParser::parseIniFile(iniFile);
            if (wcsResult) {
                lastResult_ = wcsToResult(*wcsResult);
            } else {
                lastResult_.errorMessage = "Failed to parse INI file";
            }
        } else {
            // Fall back to reading WCS from FITS
            auto wcsResult = FitsHeaderParser::parseWCSFromFile(imageFilePath);
            if (wcsResult) {
                lastResult_ = wcsToResult(*wcsResult);
            } else {
                lastResult_.errorMessage = "Failed to parse WCS from FITS";
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

void AstapClient::abort() {
    if (!solving_.load()) {
        return;
    }

    logger_->info("Aborting ASTAP solve");
    abortRequested_.store(true);

    // Use ProcessRunner to abort
    processRunner_.abort();

    // Also try to kill by name as fallback
    try {
        atom::system::killProcessByName("astap", 15);
    } catch (const std::exception& ex) {
        logger_->warn("Failed to kill ASTAP process: {}", ex.what());
    }

    SolverClient::abort();
}

std::string AstapClient::buildCommand(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH) {
    std::ostringstream cmd;
    cmd << "\"" << solverPath_ << "\"";
    cmd << " -f \"" << imageFilePath << "\"";

    // Field of view (use average or height)
    double fov =
        (fovW > 0 && fovH > 0) ? (fovW + fovH) / 2.0 : std::max(fovW, fovH);
    if (fov > 0) {
        cmd << " -fov " << fov;
    }

    // Initial coordinates hint
    if (initialCoordinates && initialCoordinates->isValid()) {
        cmd << " -ra " << initialCoordinates->ra;
        cmd << " -spd "
            << (90.0 - initialCoordinates
                           ->dec);  // ASTAP uses SPD (South Pole Distance)
    }

    // Search radius
    if (astapOptions_.searchRadius > 0 && astapOptions_.searchRadius < 180) {
        cmd << " -r " << astapOptions_.searchRadius;
    }

    // Speed mode (1-4, higher is faster but less accurate)
    if (astapOptions_.speed > 0 && astapOptions_.speed <= 4) {
        cmd << " -speed " << astapOptions_.speed;
    }

    // Max stars for solving
    if (astapOptions_.maxStars > 0 && astapOptions_.maxStars != 500) {
        cmd << " -s " << astapOptions_.maxStars;
    }

    // Hash code tolerance
    if (astapOptions_.tolerance > 0 && astapOptions_.tolerance != 0.007) {
        cmd << " -t " << astapOptions_.tolerance;
    }

    // Database path
    if (!astapOptions_.database.empty()) {
        cmd << " -d \"" << astapOptions_.database << "\"";
    }

    // Downsample factor
    if (astapOptions_.downsample > 0) {
        cmd << " -z " << astapOptions_.downsample;
    } else if (options_.downsample && *options_.downsample > 0) {
        cmd << " -z " << *options_.downsample;
    }

    // Update FITS header with WCS
    if (astapOptions_.update) {
        cmd << " -update";
    }

    // Analyse mode (HFD, background)
    if (astapOptions_.analyse) {
        cmd << " -analyse";
    }

    // Annotate solved image
    if (astapOptions_.annotate) {
        cmd << " -annotate";
    }

    // SIP polynomial coefficients for distortion
    if (astapOptions_.useSIP) {
        cmd << " -sip";
    }

    // Use triples instead of quads (for sparse star fields)
    if (astapOptions_.useTriples) {
        cmd << " -triples";
    }

    // Force 50% overlap between search fields
    if (astapOptions_.slow) {
        cmd << " -slow";
    }

    // Photometry calibration
    if (astapOptions_.photometry) {
        cmd << " -photometry";
        if (!astapOptions_.photometryFile.empty()) {
            cmd << " -o \"" << astapOptions_.photometryFile << "\"";
        }
    }

    // WCS output file
    if (!astapOptions_.wcsFile.empty()) {
        cmd << " -wcs \"" << astapOptions_.wcsFile << "\"";
    }

    // Verbose logging
    if (astapOptions_.verbose) {
        cmd << " -log";
        if (!astapOptions_.logFile.empty()) {
            cmd << " -logfile \"" << astapOptions_.logFile << "\"";
        }
    }

    return cmd.str();
}

bool AstapClient::executeSolve(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH) {
    // Build command using new options builder
    astap::OptionsBuilder builder(solverPath_);
    builder.setImageFile(imageFilePath).applyOptions(astapOptions_);

    // Apply FOV if provided
    double fov =
        (fovW > 0 && fovH > 0) ? (fovW + fovH) / 2.0 : std::max(fovW, fovH);
    if (fov > 0) {
        builder.setFov(fov);
    }

    // Apply position hint if provided
    if (initialCoordinates && initialCoordinates->isValid()) {
        builder.setPositionHint(initialCoordinates->ra,
                                initialCoordinates->dec);
    }

    // Apply downsample from base options if set
    if (options_.downsample && *options_.downsample > 0) {
        builder.setDownsample(*options_.downsample);
    }

    auto config = builder.build();
    logger_->debug("Executing: {}", ProcessRunner::buildCommandLine(config));

    // Execute using ProcessRunner
    auto result = processRunner_.execute(config);

    if (!result) {
        lastResult_.errorMessage = "Failed to execute ASTAP";
        return false;
    }

    // Check for success indicators
    if (AstapOutputParser::isSuccessful(result->stdOut)) {
        return true;
    }

    // Check if INI file was created (indicates success)
    std::string iniFile = imageFilePath;
    auto dotPos = iniFile.rfind('.');
    if (dotPos != std::string::npos) {
        iniFile = iniFile.substr(0, dotPos) + ".ini";
    }

    return atom::io::isFileExists(iniFile);
}

PlateSolveResult AstapClient::wcsToResult(const WCSData& wcs) {
    PlateSolveResult result;
    result.success = wcs.isValid();
    result.coordinates.ra = wcs.getRaDeg();
    result.coordinates.dec = wcs.getDecDeg();
    result.pixelScale = wcs.getPixelScaleArcsec();
    result.positionAngle = wcs.getRotationDeg();
    return result;
}

std::string AstapClient::getOutputPath(const std::string& imageFilePath) const {
    std::filesystem::path path(imageFilePath);
    return (path.parent_path() / (path.stem().string() + ".wcs")).string();
}

bool AstapClient::isAstapInstalled() {
    return atom::system::checkSoftwareInstalled("astap");
}

std::string AstapClient::getDefaultPath() {
#ifdef _WIN32
    return "C:\\Program Files\\astap\\astap.exe";
#else
    return "/usr/bin/astap";
#endif
}

// Register with client registry
LITHIUM_REGISTER_CLIENT(AstapClient, "astap", "ASTAP Plate Solver",
                        ClientType::Solver, "1.0.0", "astap")

}  // namespace lithium::client
