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

AstapClient::AstapClient(std::string name)
    : SolverClient(std::move(name)) {
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
    searchPaths = {
        "C:\\Program Files\\astap\\astap.exe",
        "C:\\Program Files (x86)\\astap\\astap.exe",
        "C:\\astap\\astap.exe"
    };
#else
    searchPaths = {
        "/usr/bin/astap",
        "/usr/local/bin/astap",
        "/opt/astap/astap"
    };
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
            if (std::find(results.begin(), results.end(), pathStr) == results.end()) {
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
    const std::optional<Coordinates>& initialCoordinates,
    double fovW, double fovH,
    int /*imageWidth*/, int /*imageHeight*/) {

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
            lastResult_ = parseIniFile(iniFile);
        } else {
            // Fall back to reading WCS from FITS
            lastResult_ = readWCS(imageFilePath);
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    lastResult_.solveTime = std::chrono::duration<double>(endTime - startTime).count();

    solving_.store(false);

    if (lastResult_.success) {
        logger_->info("Solve successful: RA={:.4f}, Dec={:.4f}, Scale={:.2f}\"/px",
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

    // Kill the ASTAP process
    if (currentProcessId_.load() > 0) {
        try {
            atom::system::killProcessByName("astap", 15);
        } catch (const std::exception& ex) {
            logger_->warn("Failed to kill ASTAP process: {}", ex.what());
        }
    }

    SolverClient::abort();
}

std::string AstapClient::buildCommand(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates,
    double fovW, double fovH) {

    std::ostringstream cmd;
    cmd << "\"" << solverPath_ << "\"";
    cmd << " -f \"" << imageFilePath << "\"";

    // Field of view (use average or height)
    double fov = (fovW > 0 && fovH > 0) ? (fovW + fovH) / 2.0 : std::max(fovW, fovH);
    if (fov > 0) {
        cmd << " -fov " << fov;
    }

    // Initial coordinates hint
    if (initialCoordinates && initialCoordinates->isValid()) {
        cmd << " -ra " << initialCoordinates->ra;
        cmd << " -spd " << (90.0 - initialCoordinates->dec);  // ASTAP uses SPD (South Pole Distance)
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
    const std::optional<Coordinates>& initialCoordinates,
    double fovW, double fovH) {

    std::string command = buildCommand(imageFilePath, initialCoordinates, fovW, fovH);
    logger_->debug("Executing: {}", command);

    try {
        auto result = atom::system::executeCommand(command, false);

        // Check for success indicators
        if (result.find("Solution found") != std::string::npos ||
            result.find("Solved") != std::string::npos) {
            return true;
        }

        // Check for failure indicators
        if (result.find("No solution") != std::string::npos ||
            result.find("Failed") != std::string::npos) {
            lastResult_.errorMessage = "ASTAP could not solve the image";
            return false;
        }

        // Check if INI file was created (indicates success)
        std::string iniFile = imageFilePath;
        auto dotPos = iniFile.rfind('.');
        if (dotPos != std::string::npos) {
            iniFile = iniFile.substr(0, dotPos) + ".ini";
        }

        return atom::io::isFileExists(iniFile);

    } catch (const std::exception& ex) {
        logger_->error("Execute error: {}", ex.what());
        lastResult_.errorMessage = ex.what();
        return false;
    }
}

PlateSolveResult AstapClient::readWCS(const std::string& filename) {
    logger_->debug("Reading WCS from FITS: {}", filename);

    PlateSolveResult result;
    fitsfile* fptr = nullptr;
    int status = 0;

    if (fits_open_file(&fptr, filename.c_str(), READONLY, &status)) {
        result.errorMessage = "Failed to open FITS file";
        return result;
    }

    double ra = 0, dec = 0, cdelt1 = 0, cdelt2 = 0, crota2 = 0;
    char comment[FLEN_COMMENT];

    fits_read_key(fptr, TDOUBLE, "CRVAL1", &ra, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CRVAL2", &dec, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CDELT1", &cdelt1, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CDELT2", &cdelt2, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CROTA2", &crota2, comment, &status);

    fits_close_file(fptr, &status);

    if (status != 0) {
        result.errorMessage = "Failed to read WCS keywords";
        return result;
    }

    result.success = true;
    result.coordinates.ra = ra;
    result.coordinates.dec = dec;
    result.pixelScale = std::abs(cdelt2) * 3600.0;
    result.positionAngle = crota2;

    return result;
}

PlateSolveResult AstapClient::parseIniFile(const std::string& filename) {
    logger_->debug("Parsing INI file: {}", filename);

    PlateSolveResult result;
    std::ifstream file(filename);

    if (!file.is_open()) {
        result.errorMessage = "Cannot open INI file";
        return result;
    }

    std::string line;
    std::unordered_map<std::string, std::string> values;

    while (std::getline(file, line)) {
        auto eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

            values[key] = value;
        }
    }

    // Check for solution
    auto it = values.find("PLTSOLVD");
    if (it == values.end() || it->second != "T") {
        result.errorMessage = "No solution found in INI file";
        return result;
    }

    result.success = true;

    // Parse coordinates
    if (values.count("CRVAL1")) {
        result.coordinates.ra = std::stod(values["CRVAL1"]);
    }
    if (values.count("CRVAL2")) {
        result.coordinates.dec = std::stod(values["CRVAL2"]);
    }

    // Parse pixel scale
    if (values.count("CDELT2")) {
        result.pixelScale = std::abs(std::stod(values["CDELT2"])) * 3600.0;
    }

    // Parse rotation
    if (values.count("CROTA2")) {
        result.positionAngle = std::stod(values["CROTA2"]);
    }

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
