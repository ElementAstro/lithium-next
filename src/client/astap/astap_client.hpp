/*
 * astap_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: ASTAP plate solver client implementation

*************************************************/

#ifndef LITHIUM_CLIENT_ASTAP_CLIENT_HPP
#define LITHIUM_CLIENT_ASTAP_CLIENT_HPP

#include "../common/process_runner.hpp"
#include "../common/result_parser.hpp"
#include "../common/solver_client.hpp"
#include "options.hpp"

#include <spdlog/spdlog.h>
#include <memory>

namespace lithium::client {

// AstapOptions is now defined in options.hpp as astap::Options
using AstapOptions = astap::Options;

/**
 * @brief ASTAP plate solver client
 *
 * Provides plate-solving functionality through the ASTAP (Astrometric STAcking
 * Program) external software package.
 */
class AstapClient : public SolverClient {
public:
    /**
     * @brief Construct a new AstapClient
     * @param name Instance name
     */
    explicit AstapClient(std::string name = "astap");

    /**
     * @brief Destructor
     */
    ~AstapClient() override;

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

    // ==================== ASTAP-Specific ====================

    /**
     * @brief Set ASTAP-specific options
     * @param options ASTAP options
     */
    void setAstapOptions(const AstapOptions& options) {
        astapOptions_ = options;
    }

    /**
     * @brief Get ASTAP-specific options
     * @return Current ASTAP options
     */
    const AstapOptions& getAstapOptions() const { return astapOptions_; }

    /**
     * @brief Get ASTAP version
     * @return Version string
     */
    std::string getAstapVersion() const { return solverVersion_; }

    /**
     * @brief Check if ASTAP is installed on the system
     * @return true if installed
     */
    static bool isAstapInstalled();

    /**
     * @brief Get default ASTAP executable path
     * @return Path to ASTAP executable
     */
    static std::string getDefaultPath();

protected:
    std::string getOutputPath(const std::string& imageFilePath) const override;

private:
    /**
     * @brief Scan for ASTAP executable
     * @return true if found
     */
    bool scanSolver();

    /**
     * @brief Execute ASTAP solve process
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
    astap::Options astapOptions_;
    std::shared_ptr<spdlog::logger> logger_;
    ProcessRunner processRunner_;
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_ASTAP_CLIENT_HPP
