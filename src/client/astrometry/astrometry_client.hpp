/*
 * astrometry_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Astrometry.net plate solver client implementation

*************************************************/

#ifndef LITHIUM_CLIENT_ASTROMETRY_CLIENT_HPP
#define LITHIUM_CLIENT_ASTROMETRY_CLIENT_HPP

#include "../common/process_runner.hpp"
#include "../common/result_parser.hpp"
#include "../common/solver_client.hpp"
#include "options.hpp"

#include <spdlog/spdlog.h>
#include <memory>

namespace lithium::client {

// AstrometryOptions is now defined in options.hpp as astrometry::Options
using AstrometryOptions = astrometry::Options;

/**
 * @brief Astrometry.net plate solver client
 *
 * Provides plate-solving functionality through Astrometry.net's solve-field
 */
class AstrometryClient : public SolverClient {
public:
    /**
     * @brief Construct a new AstrometryClient
     * @param name Instance name
     */
    explicit AstrometryClient(std::string name = "astrometry");

    /**
     * @brief Destructor
     */
    ~AstrometryClient() override;

    // ==================== Lifecycle ====================

    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& target, int timeout = 5000,
                 int maxRetry = 3) override;
    bool disconnect() override;
    bool isConnected() const override;
    std::vector<std::string> scan() override;

    // ==================== Solver Interface ====================

    PlateSolveResult solve(const std::string& imageFilePath,
                           const std::optional<Coordinates>& initialCoordinates,
                           double fovW, double fovH, int imageWidth,
                           int imageHeight) override;

    void abort() override;

    // ==================== Astrometry-Specific ====================

    /**
     * @brief Set Astrometry-specific options
     * @param options Astrometry options
     */
    void setAstrometryOptions(const AstrometryOptions& options) {
        astrometryOptions_ = options;
    }

    /**
     * @brief Get Astrometry-specific options
     * @return Current options
     */
    const AstrometryOptions& getAstrometryOptions() const {
        return astrometryOptions_;
    }

    /**
     * @brief Check if Astrometry.net is installed
     * @return true if installed
     */
    static bool isAstrometryInstalled();

    /**
     * @brief Get default solve-field path
     * @return Path to solve-field
     */
    static std::string getDefaultPath();

    /**
     * @brief Get available index files
     * @param directories Directories to search
     * @return List of index file paths
     */
    std::vector<std::string> getIndexFiles(
        const std::vector<std::string>& directories = {}) const;

protected:
    std::string getOutputPath(const std::string& imageFilePath) const override;

private:
    /**
     * @brief Scan for solve-field executable
     * @return true if found
     */
    bool scanSolver();

    /**
     * @brief Execute solve using ProcessRunner
     */
    bool executeSolve(const std::string& imageFilePath,
                      const std::optional<Coordinates>& initialCoordinates,
                      double fovW, double fovH);

    /**
     * @brief Convert WCSData to PlateSolveResult
     */
    static PlateSolveResult wcsToResult(const WCSData& wcs);

    std::string solverPath_;
    std::string solverVersion_;
    astrometry::Options astrometryOptions_;
    std::shared_ptr<spdlog::logger> logger_;
    ProcessRunner processRunner_;
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_ASTROMETRY_CLIENT_HPP
