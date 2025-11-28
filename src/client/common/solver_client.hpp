/*
 * solver_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Base class for plate solver clients

*************************************************/

#ifndef LITHIUM_CLIENT_SOLVER_CLIENT_HPP
#define LITHIUM_CLIENT_SOLVER_CLIENT_HPP

#include "client_base.hpp"

#include <cmath>
#include <future>
#include <optional>

namespace lithium::client {

/**
 * @brief Celestial coordinates structure
 */
struct Coordinates {
    double ra{0.0};   // Right Ascension in degrees
    double dec{0.0};  // Declination in degrees

    bool isValid() const {
        return ra >= 0.0 && ra < 360.0 && dec >= -90.0 && dec <= 90.0;
    }
} __attribute__((aligned(16)));

/**
 * @brief Plate solve result structure
 */
struct PlateSolveResult {
    bool success{false};
    Coordinates coordinates;
    double pixelScale{0.0};      // arcsec/pixel
    double positionAngle{0.0};   // degrees
    std::optional<bool> flipped;
    double radius{0.0};          // search radius used
    double solveTime{0.0};       // seconds
    std::string errorMessage;

    void clear() {
        success = false;
        coordinates = {};
        pixelScale = 0.0;
        positionAngle = 0.0;
        flipped.reset();
        radius = 0.0;
        solveTime = 0.0;
        errorMessage.clear();
    }
} __attribute__((aligned(64)));

/**
 * @brief Solver configuration options
 */
struct SolverOptions {
    // Scale hints
    std::optional<double> scaleLow;   // arcsec/pixel
    std::optional<double> scaleHigh;  // arcsec/pixel

    // Position hints
    std::optional<Coordinates> searchCenter;
    std::optional<double> searchRadius;  // degrees

    // Processing options
    std::optional<int> downsample;
    std::optional<int> depth;
    int timeout{120};  // seconds

    // Output options
    bool generatePlots{false};
    bool overwrite{true};
    std::string outputDir;
};

/**
 * @brief Base class for plate solver clients
 *
 * Provides common interface for ASTAP, Astrometry.net, StellarSolver, etc.
 */
class SolverClient : public ClientBase {
public:
    /**
     * @brief Construct a new SolverClient
     * @param name Solver name
     */
    explicit SolverClient(std::string name);

    /**
     * @brief Virtual destructor
     */
    ~SolverClient() override;

    // ==================== Solver Interface ====================

    /**
     * @brief Solve an image to determine celestial coordinates
     * @param imageFilePath Path to the image file
     * @param initialCoordinates Optional hint coordinates
     * @param fovW Field of view width in degrees
     * @param fovH Field of view height in degrees
     * @param imageWidth Image width in pixels
     * @param imageHeight Image height in pixels
     * @return Plate solve result
     */
    virtual PlateSolveResult solve(
        const std::string& imageFilePath,
        const std::optional<Coordinates>& initialCoordinates,
        double fovW, double fovH,
        int imageWidth, int imageHeight) = 0;

    /**
     * @brief Asynchronously solve an image
     * @param imageFilePath Path to the image file
     * @param initialCoordinates Optional hint coordinates
     * @param fovW Field of view width in degrees
     * @param fovH Field of view height in degrees
     * @param imageWidth Image width in pixels
     * @param imageHeight Image height in pixels
     * @return Future containing plate solve result
     */
    virtual std::future<PlateSolveResult> solveAsync(
        const std::string& imageFilePath,
        const std::optional<Coordinates>& initialCoordinates,
        double fovW, double fovH,
        int imageWidth, int imageHeight);

    /**
     * @brief Abort current solve operation
     */
    virtual void abort();

    /**
     * @brief Check if solver is currently running
     * @return true if solving in progress
     */
    bool isSolving() const { return solving_.load(); }

    // ==================== Configuration ====================

    /**
     * @brief Set solver options
     * @param options Solver options
     */
    void setOptions(const SolverOptions& options) { options_ = options; }

    /**
     * @brief Get current solver options
     * @return Current options
     */
    const SolverOptions& getOptions() const { return options_; }

    /**
     * @brief Get last solve result
     * @return Last result
     */
    const PlateSolveResult& getLastResult() const { return lastResult_; }

protected:
    // ==================== Utility Methods ====================

    /**
     * @brief Convert degrees to radians
     */
    static double toRadians(double degrees) {
        return degrees * M_PI / 180.0;
    }

    /**
     * @brief Convert radians to degrees
     */
    static double toDegrees(double radians) {
        return radians * 180.0 / M_PI;
    }

    /**
     * @brief Convert arcseconds to degrees
     */
    static double arcsecToDegree(double arcsec) {
        return arcsec / 3600.0;
    }

    /**
     * @brief Convert degrees to arcseconds
     */
    static double degreeToArcsec(double degrees) {
        return degrees * 3600.0;
    }

    /**
     * @brief Get output path for solved file
     * @param imageFilePath Source image path
     * @return Output path
     */
    virtual std::string getOutputPath(const std::string& imageFilePath) const;

    // Member variables
    SolverOptions options_;
    PlateSolveResult lastResult_;
    std::atomic<bool> solving_{false};
    std::atomic<bool> abortRequested_{false};
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_SOLVER_CLIENT_HPP
