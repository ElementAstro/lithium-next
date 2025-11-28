/*
 * stellarsolver_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: StellarSolver plate solver client implementation

*************************************************/

#ifndef LITHIUM_CLIENT_STELLARSOLVER_CLIENT_HPP
#define LITHIUM_CLIENT_STELLARSOLVER_CLIENT_HPP

#include "../common/solver_client.hpp"

#include <spdlog/spdlog.h>
#include <memory>
#include <vector>

// Forward declarations
class SS;
class QCoreApplication;

namespace lithium::client {

/**
 * @brief StellarSolver parameter profile
 */
enum class StellarSolverProfile {
    Default = 0,
    SingleThreadSolving,
    ParallelLargeScale,
    ParallelSmallScale,
    SmallScaleStars,
    Custom
};

/**
 * @brief StellarSolver scale units
 */
enum class ScaleUnits {
    DegWidth,
    ArcMinWidth,
    ArcSecPerPix,
    FocalMM
};

/**
 * @brief StellarSolver-specific options
 */
struct StellarSolverOptions {
    // Profile settings
    StellarSolverProfile profile{StellarSolverProfile::Default};
    
    // Scale settings
    std::optional<double> scaleLow;
    std::optional<double> scaleHigh;
    ScaleUnits scaleUnits{ScaleUnits::ArcSecPerPix};
    
    // Position hints
    std::optional<double> searchRA;
    std::optional<double> searchDec;
    std::optional<double> searchRadius;
    
    // Processing options
    bool calculateHFR{false};
    bool extractOnly{false};
    int downsample{0};
    
    // Index file settings
    std::vector<std::string> indexFolders;
    int indexToUse{-1};
    int healpixToUse{-1};
    
    // External program paths (for astrometry.net backend)
    std::string sextractorPath;
    std::string solverPath;
    std::string configFilePath;
    std::string wcsPath;
    
    // Convolution filter
    int convFilterType{0};  // 0=default, 1=gaussian, etc.
    double convFilterFWHM{3.5};
    
    // Star extraction parameters
    int minArea{5};
    double deblendNThresh{32};
    double deblendMinCont{0.005};
    bool cleanResults{true};
    double cleanParam{1.0};
};

/**
 * @brief Star detection result
 */
struct StarResult {
    double x{0};
    double y{0};
    double flux{0};
    double peak{0};
    double hfr{0};
    double fwhm{0};
    double background{0};
    int numPixels{0};
};

/**
 * @brief StellarSolver plate solver client
 *
 * Provides plate-solving and star extraction through StellarSolver library
 */
class StellarSolverClient : public SolverClient {
public:
    /**
     * @brief Construct a new StellarSolverClient
     * @param name Instance name
     */
    explicit StellarSolverClient(std::string name = "stellarsolver");

    /**
     * @brief Destructor
     */
    ~StellarSolverClient() override;

    // ==================== Lifecycle ====================

    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& target, int timeout = 5000,
                 int maxRetry = 3) override;
    bool disconnect() override;
    bool isConnected() const override;
    std::vector<std::string> scan() override;

    // ==================== Solver Interface ====================

    PlateSolveResult solve(
        const std::string& imageFilePath,
        const std::optional<Coordinates>& initialCoordinates,
        double fovW, double fovH,
        int imageWidth, int imageHeight) override;

    void abort() override;

    // ==================== Star Extraction ====================

    /**
     * @brief Extract stars from image
     * @param imageFilePath Image file path
     * @param calculateHFR Calculate HFR for each star
     * @return Vector of detected stars
     */
    std::vector<StarResult> extractStars(const std::string& imageFilePath,
                                         bool calculateHFR = false);

    /**
     * @brief Get last extracted stars
     * @return Last extraction results
     */
    const std::vector<StarResult>& getLastStars() const { return lastStars_; }

    // ==================== Configuration ====================

    /**
     * @brief Set StellarSolver-specific options
     * @param options StellarSolver options
     */
    void setStellarSolverOptions(const StellarSolverOptions& options) {
        ssOptions_ = options;
    }

    /**
     * @brief Get StellarSolver-specific options
     * @return Current options
     */
    const StellarSolverOptions& getStellarSolverOptions() const {
        return ssOptions_;
    }

    /**
     * @brief Set parameter profile
     * @param profile Profile to use
     */
    void setProfile(StellarSolverProfile profile);

    /**
     * @brief Get available index files
     * @param directories Directories to search
     * @return List of index file paths
     */
    std::vector<std::string> getIndexFiles(
        const std::vector<std::string>& directories = {}) const;

    /**
     * @brief Get default index folder paths
     * @return Default paths
     */
    static std::vector<std::string> getDefaultIndexFolderPaths();

    /**
     * @brief Check if StellarSolver library is available
     * @return true if available
     */
    static bool isStellarSolverAvailable();

    // ==================== WCS Utilities ====================

    /**
     * @brief Convert pixel to WCS coordinates
     * @param x Pixel X
     * @param y Pixel Y
     * @param ra Output RA
     * @param dec Output Dec
     * @return true if successful
     */
    bool pixelToWCS(double x, double y, double& ra, double& dec) const;

    /**
     * @brief Convert WCS to pixel coordinates
     * @param ra RA in degrees
     * @param dec Dec in degrees
     * @param x Output pixel X
     * @param y Output pixel Y
     * @return true if successful
     */
    bool wcsToPixel(double ra, double dec, double& x, double& y) const;

    // ==================== Performance ====================

    /**
     * @brief Get extraction time from last operation
     * @return Time in seconds
     */
    double getExtractionTime() const { return extractionTime_; }

    /**
     * @brief Get solving time from last operation
     * @return Time in seconds
     */
    double getSolvingTime() const { return solvingTime_; }

protected:
    std::string getOutputPath(const std::string& imageFilePath) const override;

private:
    /**
     * @brief Load image into solver
     */
    bool loadImage(const std::string& imageFilePath);

    /**
     * @brief Apply current options to solver
     */
    void applyOptions();

    StellarSolverOptions ssOptions_;
    std::vector<StarResult> lastStars_;
    std::shared_ptr<spdlog::logger> logger_;

    // Qt application context (required for StellarSolver)
    std::unique_ptr<QCoreApplication> qtApp_;
    std::unique_ptr<SS> solver_;

    // Performance tracking
    double extractionTime_{0};
    double solvingTime_{0};

    // State
    bool imageLoaded_{false};
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_STELLARSOLVER_CLIENT_HPP
