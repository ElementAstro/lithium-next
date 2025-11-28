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

#include "../common/solver_client.hpp"

#include <spdlog/spdlog.h>
#include <memory>

namespace lithium::client {

/**
 * @brief ASTAP-specific solver options
 * Based on ASTAP v2025.11.23 command line interface
 */
struct AstapOptions {
    // Basic solving options
    int searchRadius{180};       // -r: Search radius in degrees (default: all-sky)
    int maxStars{500};           // -s: Maximum stars to use for solving
    double tolerance{0.007};     // -t: Hash code tolerance for quad matching
    int speed{0};                // -speed: Speed mode (0=auto, 1-4, higher=faster but less accurate)
    
    // Database options
    std::string database;        // -d: Star database path (D05, D20, D50, D80, G05, V50, W08)
    
    // Output options
    bool update{false};          // -update: Update FITS header with WCS
    bool analyse{false};         // -analyse: Analyse image (HFD, background, etc.)
    bool annotate{false};        // -annotate: Annotate solved image
    std::string wcsFile;         // -wcs: Output WCS file path
    
    // Image processing options
    int downsample{0};           // -z: Downsample factor (0=auto, 2, 3, 4)
    double minStarSize{1.5};     // Minimum star HFD in arcsec to filter hot pixels
    bool useSIP{false};          // -sip: Add SIP polynomial coefficients for distortion
    bool useTriples{false};      // -triples: Use triples instead of quads (for sparse fields)
    bool slow{false};            // -slow: Force 50% overlap between search fields
    
    // Photometry options (requires V50 database)
    bool photometry{false};      // -photometry: Perform photometry calibration
    std::string photometryFile;  // Output photometry results file
    
    // Stacking options (for batch processing)
    bool stack{false};           // -stack: Stack multiple images
    std::string stackMethod;     // -stackmethod: average, sigma_clip
    double sigmaFactor{2.5};     // -sigma: Sigma factor for sigma clip stacking
    
    // Alignment options
    std::string alignmentMethod{"star"};  // star, astrometric, manual, ephemeris
    
    // Log and debug
    bool verbose{false};         // -log: Enable verbose logging
    std::string logFile;         // -logfile: Log file path
};

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

    PlateSolveResult solve(
        const std::string& imageFilePath,
        const std::optional<Coordinates>& initialCoordinates,
        double fovW, double fovH,
        int imageWidth, int imageHeight) override;

    void abort() override;

    // ==================== ASTAP-Specific ====================

    /**
     * @brief Set ASTAP-specific options
     * @param options ASTAP options
     */
    void setAstapOptions(const AstapOptions& options) { astapOptions_ = options; }

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
     * @brief Build command line for ASTAP
     * @param imageFilePath Image file path
     * @param initialCoordinates Optional hint coordinates
     * @param fovW Field of view width
     * @param fovH Field of view height
     * @return Command string
     */
    std::string buildCommand(const std::string& imageFilePath,
                             const std::optional<Coordinates>& initialCoordinates,
                             double fovW, double fovH);

    /**
     * @brief Execute ASTAP solve process
     * @param imageFilePath Image file path
     * @param initialCoordinates Optional hint coordinates
     * @param fovW Field of view width
     * @param fovH Field of view height
     * @return true if successful
     */
    bool executeSolve(const std::string& imageFilePath,
                      const std::optional<Coordinates>& initialCoordinates,
                      double fovW, double fovH);

    /**
     * @brief Read WCS data from solution file
     * @param filename Solution file path
     * @return Plate solve result
     */
    PlateSolveResult readWCS(const std::string& filename);

    /**
     * @brief Parse INI file for solve results
     * @param filename INI file path
     * @return Plate solve result
     */
    PlateSolveResult parseIniFile(const std::string& filename);

    std::string solverPath_;
    std::string solverVersion_;
    AstapOptions astapOptions_;
    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<int> currentProcessId_{0};
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_ASTAP_CLIENT_HPP
