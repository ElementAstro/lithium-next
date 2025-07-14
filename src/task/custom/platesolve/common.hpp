#ifndef LITHIUM_TASK_PLATESOLVE_PLATESOLVE_COMMON_HPP
#define LITHIUM_TASK_PLATESOLVE_PLATESOLVE_COMMON_HPP

#include <memory>
#include <string>
#include "../../task.hpp"
#include "client/astrometry/remote/client.hpp"
#include "device/template/solver.hpp"

namespace lithium::task::platesolve {

// ==================== Enhanced Configuration Structures ====================

/**
 * @brief Plate solve task configuration with support for online/offline modes
 */
struct PlateSolveConfig {
    double exposure{5.0};  // Exposure time for plate solving
    int binning{2};        // Camera binning
    int maxAttempts{3};    // Maximum solve attempts
    double timeout{60.0};  // Solve timeout in seconds
    int gain{100};         // Camera gain
    int offset{10};        // Camera offset
    std::string solverType{
        "astrometry"};                  // Solver type (astrometry/astap/remote)
    bool useInitialCoordinates{false};  // Use initial coordinates hint
    double fovWidth{1.0};               // Field of view width in degrees
    double fovHeight{1.0};              // Field of view height in degrees

    // Online/Remote solving configuration
    bool useRemoteSolver{false};  // Use remote astrometry.net service
    std::string apiKey;           // API key for remote service
    astrometry::License license{astrometry::License::Default};  // Image license
    bool publiclyVisible{false};  // Make submission publicly visible
    std::string sessionId;        // Session ID for remote service

    // Advanced solver options
    double scaleEstimate{1.0};      // Pixel scale estimate (arcsec/pixel)
    double scaleError{0.1};         // Scale estimate error tolerance
    std::optional<double> raHint;   // RA hint in degrees
    std::optional<double> decHint;  // Dec hint in degrees
    double searchRadius{2.0};       // Search radius around hint in degrees
};

/**
 * @brief Centering task configuration
 */
struct CenteringConfig {
    double targetRA{0.0};         // Target RA in hours
    double targetDec{0.0};        // Target Dec in degrees
    double tolerance{30.0};       // Centering tolerance in arcseconds
    int maxIterations{5};         // Maximum centering iterations
    PlateSolveConfig platesolve;  // Plate solve configuration
};

/**
 * @brief Mosaic task configuration
 */
struct MosaicConfig {
    double centerRA{0.0};         // Mosaic center RA in hours
    double centerDec{0.0};        // Mosaic center Dec in degrees
    int gridWidth{2};             // Number of columns
    int gridHeight{2};            // Number of rows
    double overlap{20.0};         // Frame overlap percentage
    double frameExposure{300.0};  // Exposure time per frame
    int framesPerPosition{1};     // Frames per position
    bool autoCenter{true};        // Auto-center each position
    int gain{100};                // Camera gain
    int offset{10};               // Camera offset
    CenteringConfig centering;    // Centering configuration
};

/**
 * @brief Enhanced result structure for plate solving operations
 */
struct PlatesolveResult {
    bool success{false};
    Coordinates coordinates{0.0, 0.0};
    double pixelScale{0.0};
    double rotation{0.0};
    double fovWidth{0.0};
    double fovHeight{0.0};
    std::string errorMessage;
    std::chrono::milliseconds solveTime{0};

    // Additional solver information
    std::string solverUsed;                // Which solver was used
    bool usedRemote{false};                // Whether remote solver was used
    int starsFound{0};                     // Number of stars detected
    double matchQuality{0.0};              // Quality of the plate solve match
    std::optional<std::string> wcsHeader;  // WCS header information
};

/**
 * @brief Result structure for centering operations
 */
struct CenteringResult {
    bool success{false};
    Coordinates finalPosition{0.0, 0.0};
    Coordinates targetPosition{0.0, 0.0};
    double finalOffset{0.0};  // Final offset in arcseconds
    int iterations{0};
    std::vector<PlatesolveResult> solveResults;
};

/**
 * @brief Result structure for mosaic operations
 */
struct MosaicResult {
    bool success{false};
    int totalPositions{0};
    int completedPositions{0};
    int totalFrames{0};
    int completedFrames{0};
    std::vector<CenteringResult> centeringResults;
    std::chrono::milliseconds totalTime{0};
};

/**
 * @brief Base class for all plate solve related tasks
 */
class PlateSolveTaskBase : public lithium::task::Task {
public:
    explicit PlateSolveTaskBase(const std::string& name);
    virtual ~PlateSolveTaskBase() = default;

protected:
    /**
     * @brief Get local solver instance from global manager
     * @param solverType Type of solver to retrieve
     * @return Shared pointer to solver instance
     */
    auto getLocalSolverInstance(const std::string& solverType)
        -> std::shared_ptr<AtomSolver>;

    /**
     * @brief Get remote astrometry client instance from global manager
     * @return Shared pointer to remote client instance
     */
    auto getRemoteAstrometryClient()
        -> std::shared_ptr<astrometry::AstrometryClient>;

    /**
     * @brief Get mount instance from global manager
     * @return Shared pointer to mount instance
     */
    auto getMountInstance() -> std::shared_ptr<void>;

    /**
     * @brief Perform plate solving using appropriate solver (local or remote)
     * @param imagePath Path to image file
     * @param config Plate solve configuration
     * @return Plate solve result
     */
    auto performPlateSolve(const std::string& imagePath,
                           const PlateSolveConfig& config) -> PlatesolveResult;

private:
    /**
     * @brief Perform local plate solving using installed solvers
     * @param imagePath Path to image file
     * @param config Plate solve configuration
     * @return Plate solve result
     */
    auto performLocalPlateSolve(const std::string& imagePath,
                                const PlateSolveConfig& config)
        -> PlatesolveResult;

    /**
     * @brief Perform remote plate solving using astrometry.net service
     * @param imagePath Path to image file
     * @param config Plate solve configuration
     * @return Plate solve result
     */
    auto performRemotePlateSolve(const std::string& imagePath,
                                 const PlateSolveConfig& config)
        -> PlatesolveResult;

    /**
     * @brief Get camera instance from global manager
     * @return Shared pointer to camera instance
     */
    auto getCameraInstance() -> std::shared_ptr<void>;

    /**
     * @brief Get mount instance from global manager
     * @return Shared pointer to mount instance
     */
    // auto getMountInstance() -> std::shared_ptr<void>; // 移到 protected 区域，删除此重复声明

    /**
     * @brief Convert RA from hours to degrees
     */
    static auto hoursTodegrees(double hours) -> double;

    // 其它静态成员同理，全部移到 protected 区域，避免子类访问报错
    static auto degreesToHours(double degrees) -> double;
    static auto calculateAngularDistance(const Coordinates& pos1,
                                         const Coordinates& pos2) -> double;
    static auto degreesToArcsec(double degrees) -> double;
    static auto arcsecToDegrees(double arcsec) -> double;

    // 删除 private 区域的重复静态成员声明
};

}  // namespace lithium::task::platesolve

#endif  // LITHIUM_TASK_PLATESOLVE_PLATESOLVE_COMMON_HPP
