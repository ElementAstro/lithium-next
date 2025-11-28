/*
 * stellarsolver_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: StellarSolver plate solver client implementation

*************************************************/

#include "stellarsolver_client.hpp"
#include "stellarsolver.hpp"

#include "atom/io/io.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <QCoreApplication>
#include <chrono>
#include <filesystem>

namespace lithium::client {

StellarSolverClient::StellarSolverClient(std::string name)
    : SolverClient(std::move(name)) {
    logger_ = spdlog::get("stellarsolver_client");
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("stellarsolver_client");
    }
    logger_->info("StellarSolverClient created: {}", getName());
}

StellarSolverClient::~StellarSolverClient() {
    if (isConnected()) {
        disconnect();
    }
    logger_->debug("StellarSolverClient destroyed: {}", getName());
}

bool StellarSolverClient::initialize() {
    logger_->debug("Initializing StellarSolverClient");

    // Initialize Qt application context if needed
    if (!QCoreApplication::instance()) {
        static int argc = 1;
        static char* argv[] = {const_cast<char*>("stellarsolver")};
        qtApp_ = std::make_unique<QCoreApplication>(argc, argv);
    }

    setState(ClientState::Initialized);
    emitEvent("initialized", "");
    return true;
}

bool StellarSolverClient::destroy() {
    logger_->debug("Destroying StellarSolverClient");

    if (solving_.load()) {
        abort();
    }

    if (isConnected()) {
        disconnect();
    }

    solver_.reset();
    qtApp_.reset();

    setState(ClientState::Uninitialized);
    emitEvent("destroyed", "");
    return true;
}

bool StellarSolverClient::connect(const std::string& /*target*/, int /*timeout*/,
                                  int /*maxRetry*/) {
    logger_->debug("Connecting StellarSolver");
    setState(ClientState::Connecting);

    // StellarSolver is a library, no actual connection needed
    // Just verify it's available
    if (!isStellarSolverAvailable()) {
        setError(1, "StellarSolver library not available");
        return false;
    }

    setState(ClientState::Connected);
    emitEvent("connected", "");
    return true;
}

bool StellarSolverClient::disconnect() {
    logger_->debug("Disconnecting StellarSolver");
    setState(ClientState::Disconnecting);

    if (solving_.load()) {
        abort();
    }

    solver_.reset();
    imageLoaded_ = false;

    setState(ClientState::Disconnected);
    emitEvent("disconnected", "");
    return true;
}

bool StellarSolverClient::isConnected() const {
    return getState() == ClientState::Connected;
}

std::vector<std::string> StellarSolverClient::scan() {
    // StellarSolver is a library, return empty
    return {};
}

PlateSolveResult StellarSolverClient::solve(
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

    try {
        // Create solver instance
        solver_ = std::make_unique<SS>();

        // Load image
        if (!loadImage(imageFilePath)) {
            lastResult_.errorMessage = "Failed to load image";
            solving_.store(false);
            return lastResult_;
        }

        // Set search scale
        if (fovW > 0 && fovH > 0) {
            double scaleLow = std::min(fovW, fovH) * 0.8;
            double scaleHigh = std::max(fovW, fovH) * 1.2;
            solver_->setSearchScale(scaleLow, scaleHigh, "degwidth");
        } else if (ssOptions_.scaleLow && ssOptions_.scaleHigh) {
            QString units;
            switch (ssOptions_.scaleUnits) {
                case ScaleUnits::DegWidth: units = "degwidth"; break;
                case ScaleUnits::ArcMinWidth: units = "arcminwidth"; break;
                case ScaleUnits::ArcSecPerPix: units = "arcsecperpix"; break;
                case ScaleUnits::FocalMM: units = "focalmm"; break;
            }
            solver_->setSearchScale(*ssOptions_.scaleLow, *ssOptions_.scaleHigh, units);
        }

        // Set search position
        if (initialCoordinates && initialCoordinates->isValid()) {
            solver_->setSearchPositionRaDec(initialCoordinates->ra, initialCoordinates->dec);
        } else if (ssOptions_.searchRA && ssOptions_.searchDec) {
            solver_->setSearchPositionRaDec(*ssOptions_.searchRA, *ssOptions_.searchDec);
        }

        // Apply profile
        applyOptions();

        // Run solver
        auto solveStart = std::chrono::steady_clock::now();
        bool success = solver_->solve();
        auto solveEnd = std::chrono::steady_clock::now();
        solvingTime_ = std::chrono::duration<double>(solveEnd - solveStart).count();

        if (abortRequested_.load()) {
            lastResult_.errorMessage = "Solve aborted by user";
            solving_.store(false);
            emitEvent("solve_aborted", imageFilePath);
            return lastResult_;
        }

        if (success) {
            // Get solution from solver
            // Note: Actual implementation depends on SS class interface
            lastResult_.success = true;
            // TODO: Extract coordinates from solver_->getSolution()
        } else {
            lastResult_.errorMessage = solver_->getLastError().toStdString();
        }

    } catch (const std::exception& ex) {
        logger_->error("Solve error: {}", ex.what());
        lastResult_.errorMessage = ex.what();
    }

    auto endTime = std::chrono::steady_clock::now();
    lastResult_.solveTime = std::chrono::duration<double>(endTime - startTime).count();

    solving_.store(false);

    if (lastResult_.success) {
        logger_->info("Solve successful in {:.2f}s", lastResult_.solveTime);
        emitEvent("solve_completed", imageFilePath);
    } else {
        logger_->error("Solve failed: {}", lastResult_.errorMessage);
        emitEvent("solve_failed", lastResult_.errorMessage);
    }

    return lastResult_;
}

void StellarSolverClient::abort() {
    if (!solving_.load()) {
        return;
    }

    logger_->info("Aborting StellarSolver");
    abortRequested_.store(true);

    if (solver_) {
        solver_->abort();
    }

    SolverClient::abort();
}

std::vector<StarResult> StellarSolverClient::extractStars(
    const std::string& imageFilePath, bool calculateHFR) {

    lastStars_.clear();

    if (!isConnected()) {
        logger_->error("Cannot extract: not connected");
        return lastStars_;
    }

    if (!atom::io::isFileExists(imageFilePath)) {
        logger_->error("Image file not found: {}", imageFilePath);
        return lastStars_;
    }

    try {
        solver_ = std::make_unique<SS>();

        if (!loadImage(imageFilePath)) {
            return lastStars_;
        }

        applyOptions();

        auto extractStart = std::chrono::steady_clock::now();
        bool success = solver_->extract(calculateHFR);
        auto extractEnd = std::chrono::steady_clock::now();
        extractionTime_ = std::chrono::duration<double>(extractEnd - extractStart).count();

        if (success) {
            auto stars = solver_->findStarsByStellarSolver(true, calculateHFR);
            for (const auto& star : stars) {
                StarResult result;
                result.x = star.x;
                result.y = star.y;
                result.flux = star.flux;
                result.peak = star.peak;
                result.hfr = star.HFR;
                result.fwhm = star.fwhm;
                result.background = star.background;
                result.numPixels = star.numPixels;
                lastStars_.push_back(result);
            }
            logger_->info("Extracted {} stars in {:.2f}s", lastStars_.size(), extractionTime_);
        }

    } catch (const std::exception& ex) {
        logger_->error("Extraction error: {}", ex.what());
    }

    return lastStars_;
}

void StellarSolverClient::setProfile(StellarSolverProfile profile) {
    ssOptions_.profile = profile;

    if (solver_) {
        switch (profile) {
            case StellarSolverProfile::Default:
                solver_->setParameterProfile(SSolver::Parameters::DEFAULT);
                break;
            case StellarSolverProfile::SingleThreadSolving:
                solver_->setParameterProfile(SSolver::Parameters::SINGLE_THREAD_SOLVING);
                break;
            case StellarSolverProfile::ParallelLargeScale:
                solver_->setParameterProfile(SSolver::Parameters::PARALLEL_LARGESCALE);
                break;
            case StellarSolverProfile::ParallelSmallScale:
                solver_->setParameterProfile(SSolver::Parameters::PARALLEL_SMALLSCALE);
                break;
            case StellarSolverProfile::SmallScaleStars:
                solver_->setParameterProfile(SSolver::Parameters::SMALL_SCALE_STARS);
                break;
            default:
                break;
        }
    }
}

std::vector<std::string> StellarSolverClient::getIndexFiles(
    const std::vector<std::string>& directories) const {

    std::vector<std::string> results;

    QStringList dirs;
    if (directories.empty()) {
        dirs = SS::getDefaultIndexFolderPaths();
    } else {
        for (const auto& dir : directories) {
            dirs.append(QString::fromStdString(dir));
        }
    }

    auto files = SS::getIndexFiles(dirs, ssOptions_.indexToUse, ssOptions_.healpixToUse);
    for (const auto& file : files) {
        results.push_back(file.toStdString());
    }

    return results;
}

std::vector<std::string> StellarSolverClient::getDefaultIndexFolderPaths() {
    std::vector<std::string> paths;
    auto qPaths = SS::getDefaultIndexFolderPaths();
    for (const auto& path : qPaths) {
        paths.push_back(path.toStdString());
    }
    return paths;
}

bool StellarSolverClient::isStellarSolverAvailable() {
    // Check if StellarSolver library is linked
    return true;  // Always available if compiled with StellarSolver
}

bool StellarSolverClient::pixelToWCS(double x, double y, double& ra, double& dec) const {
    if (!solver_ || !lastResult_.success) {
        return false;
    }

    QPointF pixel(x, y);
    FITSImage::wcs_point sky;

    if (solver_->pixelToWCS(pixel, sky)) {
        ra = sky.ra;
        dec = sky.dec;
        return true;
    }

    return false;
}

bool StellarSolverClient::wcsToPixel(double ra, double dec, double& x, double& y) const {
    if (!solver_ || !lastResult_.success) {
        return false;
    }

    FITSImage::wcs_point sky{ra, dec};
    QPointF pixel;

    if (solver_->wcsToPixel(sky, pixel)) {
        x = pixel.x();
        y = pixel.y();
        return true;
    }

    return false;
}

std::string StellarSolverClient::getOutputPath(const std::string& imageFilePath) const {
    std::filesystem::path path(imageFilePath);
    return (path.parent_path() / (path.stem().string() + ".wcs")).string();
}

bool StellarSolverClient::loadImage(const std::string& imageFilePath) {
    // Implementation depends on SS class interface
    // This is a placeholder
    imageLoaded_ = true;
    return true;
}

void StellarSolverClient::applyOptions() {
    if (!solver_) return;

    // Apply profile
    setProfile(ssOptions_.profile);

    // Apply other options as needed
    // This depends on the SS class interface
}

// Register with client registry
LITHIUM_REGISTER_CLIENT(StellarSolverClient, "stellarsolver", "StellarSolver Library",
                        ClientType::Solver, "1.0.0")

}  // namespace lithium::client
