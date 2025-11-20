#ifndef LITHIUM_CLIENT_ASTAP_HPP
#define LITHIUM_CLIENT_ASTAP_HPP

#include <spdlog/spdlog.h>
#include <cmath>
#include <future>
#include <optional>
#include <string>
#include <vector>
#include "device/template/solver.hpp"

/**
 * @brief Implementation of a plate solver using ASTAP software
 *
 * Provides plate-solving functionality through the ASTAP (Astrometric STAcking
 * Program) external software package.
 */
class AstapSolver : public AtomSolver {
public:
    /**
     * @brief Constructs a new AstapSolver instance
     * @param name Name identifier for this solver instance
     */
    explicit AstapSolver(std::string name);

    /**
     * @brief Destructor
     */
    ~AstapSolver() override;

    /**
     * @brief Initializes the solver by locating the ASTAP executable
     * @return true if initialization successful, false otherwise
     */
    auto initialize() -> bool override;

    /**
     * @brief Releases resources used by the solver
     * @return true if cleanup successful, false otherwise
     */
    auto destroy() -> bool override;

    /**
     * @brief Connects to the ASTAP solver at specified path
     * @param name Path to ASTAP executable
     * @param timeout Connection timeout in seconds
     * @param maxRetry Maximum number of connection attempts
     * @return true if connection successful, false otherwise
     */
    auto connect(const std::string& name, int timeout = 30, int maxRetry = 3)
        -> bool override;

    /**
     * @brief Disconnects from ASTAP solver
     * @return true if disconnection successful, false otherwise
     */
    auto disconnect() -> bool override;

    /**
     * @brief Scans for available ASTAP installations
     * @return Vector containing paths to detected ASTAP executables
     */
    auto scan() -> std::vector<std::string> override;

    /**
     * @brief Checks if solver is connected
     * @return true if connected, false otherwise
     */
    auto isConnected() const -> bool override;

    /**
     * @brief Solves an astronomical image to determine coordinates
     * @param imageFilePath Path to image file
     * @param initialCoordinates Optional starting point for plate solving
     * @param fovW Field of view width in degrees
     * @param fovH Field of view height in degrees
     * @param imageWidth Image width in pixels
     * @param imageHeight Image height in pixels
     * @return Solution results including coordinates and scale
     */
    PlateSolveResult solve(const std::string& imageFilePath,
                           const std::optional<Coordinates>& initialCoordinates,
                           double fovW, double fovH, int imageWidth,
                           int imageHeight) override;

    /**
     * @brief Asynchronously solves an astronomical image
     * @param imageFilePath Path to image file
     * @param initialCoordinates Optional starting point for plate solving
     * @param fovW Field of view width in degrees
     * @param fovH Field of view height in degrees
     * @param imageWidth Image width in pixels
     * @param imageHeight Image height in pixels
     * @return Future containing solution results
     */
    std::future<PlateSolveResult> async_solve(
        const std::string& imageFilePath,
        const std::optional<Coordinates>& initialCoordinates, double fovW,
        double fovH, int imageWidth, int imageHeight) override;

protected:
    /**
     * @brief Converts degrees to radians
     * @param degrees Angle in degrees
     * @return Angle in radians
     */
    auto toRadians(double degrees) -> double override {
        return degrees * M_PI / 180.0;
    }

    /**
     * @brief Converts radians to degrees
     * @param radians Angle in radians
     * @return Angle in degrees
     */
    auto toDegrees(double radians) -> double override {
        return radians * 180.0 / M_PI;
    }

    /**
     * @brief Converts arcseconds to degrees
     * @param arcsec Angle in arcseconds
     * @return Angle in degrees
     */
    auto arcsecToDegree(double arcsec) -> double override {
        return arcsec / 3600.0;
    }

    /**
     * @brief Gets the output path for solved file
     * @param imageFilePath Source image path
     * @return Path to output solution file
     */
    auto getOutputPath(const std::string& imageFilePath) const
        -> std::string override;

private:
    /**
     * @brief Locates ASTAP executable on the system
     * @return true if ASTAP found, false otherwise
     */
    auto scanSolver() -> bool;

    /**
     * @brief Reads WCS data from a solution file
     * @param filename Path to file with WCS headers
     * @return Plate solving results
     */
    auto readWCS(const std::string& filename) -> PlateSolveResult;

    /**
     * @brief Executes ASTAP plate solving process
     * @param imageFilePath Path to image file
     * @param initialCoordinates Optional starting point
     * @param fovW Field of view width in degrees
     * @param fovH Field of view height in degrees
     * @return true if solving succeeded, false otherwise
     */
    auto executeSolve(const std::string& imageFilePath,
                      const std::optional<Coordinates>& initialCoordinates,
                      double fovW, double fovH) -> bool;

    std::string solverPath_;                  ///< Path to ASTAP executable
    std::string solverVersion_;               ///< ASTAP version string
    std::shared_ptr<spdlog::logger> logger_;  ///< Class logger
};

#endif
