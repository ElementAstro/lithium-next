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

#include "../common/solver_client.hpp"

#include <spdlog/spdlog.h>
#include <memory>

namespace lithium::client {

/**
 * @brief Astrometry.net-specific solver options
 * Based on solve-field command line interface
 */
struct AstrometryOptions {
    // Basic options
    std::optional<std::string> backendConfig;  // --backend-config
    std::optional<std::string> config;          // --config
    bool batch{false};                          // --batch
    bool noPlots{true};                         // --no-plots
    bool overwrite{true};                       // --overwrite
    bool skipSolved{false};                     // --skip-solved
    bool continueRun{false};                    // --continue
    bool timestamp{false};                      // --timestamp
    bool noDeleteTemp{false};                   // --no-delete-temp

    // Scale options
    std::optional<double> scaleLow;             // --scale-low
    std::optional<double> scaleHigh;            // --scale-high
    std::optional<std::string> scaleUnits;      // --scale-units: degwidth, arcminwidth, arcsecperpix, focalmm
    bool guessScale{false};                     // --guess-scale

    // Position options
    std::optional<std::string> ra;              // --ra
    std::optional<std::string> dec;             // --dec
    std::optional<double> radius;               // --radius

    // Processing options
    std::optional<int> depth;                   // --depth
    std::optional<int> objs;                    // --objs
    std::optional<int> cpuLimit;                // --cpulimit
    std::optional<int> downsample;              // --downsample
    bool invert{false};                         // --invert
    bool noBackgroundSubtraction{false};        // --no-background-subtraction
    std::optional<float> sigma;                 // --sigma
    std::optional<float> nsigma;                // --nsigma
    bool noRemoveLines{false};                  // --no-remove-lines
    std::optional<int> uniformize;              // --uniformize
    bool noVerifyUniformize{false};             // --no-verify-uniformize
    bool noVerifyDedup{false};                  // --no-verify-dedup
    bool resort{false};                         // --resort

    // Parity and tolerance
    std::optional<std::string> parity;          // --parity: pos, neg
    std::optional<double> codeTolerance;        // --code-tolerance
    std::optional<int> pixelError;              // --pixel-error

    // Quad size
    std::optional<double> quadSizeMin;          // --quad-size-min
    std::optional<double> quadSizeMax;          // --quad-size-max

    // Odds
    std::optional<double> oddsTuneUp;           // --odds-to-tune-up
    std::optional<double> oddsSolve;            // --odds-to-solve
    std::optional<double> oddsReject;           // --odds-to-reject
    std::optional<double> oddsStopLooking;      // --odds-to-stop-looking

    // Output options
    std::optional<std::string> newFits;         // --new-fits
    std::optional<std::string> wcs;             // --wcs
    std::optional<std::string> corr;            // --corr
    std::optional<std::string> match;           // --match
    std::optional<std::string> rdls;            // --rdls
    std::optional<std::string> sortRdls;        // --sort-rdls
    std::optional<std::string> tag;             // --tag
    bool tagAll{false};                         // --tag-all
    std::optional<std::string> indexXyls;       // --index-xyls
    bool justAugment{false};                    // --just-augment
    std::optional<std::string> axy;             // --axy
    bool tempAxy{false};                        // --temp-axy
    std::optional<std::string> pnm;             // --pnm
    std::optional<std::string> keepXylist;      // --keep-xylist
    bool dontAugment{false};                    // --dont-augment

    // WCS options
    bool crpixCenter{false};                    // --crpix-center
    std::optional<int> crpixX;                  // --crpix-x
    std::optional<int> crpixY;                  // --crpix-y
    bool noTweak{false};                        // --no-tweak
    std::optional<int> tweakOrder;              // --tweak-order
    std::optional<std::string> predistort;      // --predistort
    std::optional<double> xscale;               // --xscale

    // Verification
    std::optional<std::string> verify;          // --verify
    std::optional<std::string> verifyExt;       // --verify-ext
    bool noVerify{false};                       // --no-verify

    // Source extractor
    bool useSourceExtractor{false};             // --use-source-extractor
    std::optional<std::string> sourceExtractorConfig;  // --source-extractor-config
    std::optional<std::string> sourceExtractorPath;    // --source-extractor-path

    // SCAMP
    std::optional<std::string> scamp;           // --scamp
    std::optional<std::string> scampConfig;     // --scamp-config
    std::optional<std::string> scampRef;        // --scamp-ref

    // KMZ output
    std::optional<std::string> kmz;             // --kmz

    // FITS extension
    std::optional<int> extension;               // --extension
    bool fitsImage{false};                      // --fits-image

    // Temp directory
    std::optional<std::string> tempDir;         // --temp-dir

    // Cancel/solved files
    std::optional<std::string> cancel;          // --cancel
    std::optional<std::string> solved;          // --solved
    std::optional<std::string> solvedIn;        // --solved-in
};

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

    PlateSolveResult solve(
        const std::string& imageFilePath,
        const std::optional<Coordinates>& initialCoordinates,
        double fovW, double fovH,
        int imageWidth, int imageHeight) override;

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
     * @brief Build command line
     */
    std::string buildCommand(const std::string& imageFilePath,
                             const std::optional<Coordinates>& initialCoordinates,
                             double fovW, double fovH);

    /**
     * @brief Execute solve process
     */
    bool executeSolve(const std::string& imageFilePath,
                      const std::optional<Coordinates>& initialCoordinates,
                      double fovW, double fovH);

    /**
     * @brief Parse solve output
     */
    PlateSolveResult parseSolveOutput(const std::string& output);

    /**
     * @brief Read WCS from solved file
     */
    PlateSolveResult readWCS(const std::string& filename);

    std::string solverPath_;
    std::string solverVersion_;
    AstrometryOptions astrometryOptions_;
    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<int> currentProcessId_{0};
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_ASTROMETRY_CLIENT_HPP
