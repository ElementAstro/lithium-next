#include "astap.hpp"

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "atom/io/io.hpp"
#include "atom/system/command.hpp"
#include "atom/system/process.hpp"
#include "atom/system/software.hpp"
#include "device/template/solver.hpp"

#include <fitsio.h>
#include <spdlog/sinks/stdout_color_sinks-inl.h>
#include <cmath>
#include <sstream>
#include <string_view>
#include <utility>

AstapSolver::AstapSolver(std::string name) : AtomSolver(std::move(name)) {
    logger_ = spdlog::get("astap_solver")
                  ? spdlog::get("astap_solver")
                  : spdlog::stdout_color_mt<spdlog::synchronous_factory>(
                        "astap_solver", spdlog::color_mode::automatic);

    logger_->info("Initializing Astap Solver");
    if (!scanSolver()) {
        logger_->error("Failed to initialize: ASTAP not installed");
    }
}

AstapSolver::~AstapSolver() { logger_->debug("Destroying Astap Solver"); }

auto AstapSolver::initialize() -> bool {
    logger_->debug("Initializing Astap Solver");
    return scanSolver();
}

auto AstapSolver::destroy() -> bool {
    logger_->debug("Destroying Astap Solver");
    return true;
}

auto AstapSolver::connect(const std::string& name, int /*timeout*/,
                          int /*maxRetry*/) -> bool {
    logger_->debug("Connecting to Astap Solver with path: {}", name);

    if (name.empty() || !atom::io::isFileNameValid(name) ||
        !atom::io::isFileExists(name)) {
        logger_->error("Connection failed: Invalid parameters");
        return false;
    }

    solverPath_ = name;
    logger_->debug("Connected to Astap Solver at: {}", solverPath_);
    return true;
}

auto AstapSolver::disconnect() -> bool {
    logger_->debug("Disconnecting from Astap Solver");
    solverPath_.clear();

    if (atom::system::isProcessRunning("astap")) {
        logger_->info("Terminating running Astap process");
        try {
            atom::system::killProcessByName("astap", 15);
        } catch (const std::exception& ex) {
            logger_->error("Failed to terminate Astap process: {}", ex.what());
            return false;
        }
    }

    return true;
}

auto AstapSolver::scan() -> std::vector<std::string> {
    logger_->debug("Scanning for available Astap solvers");
    std::vector<std::string> result;

    if (scanSolver() && !solverPath_.empty()) {
        result.push_back(solverPath_);
    }

    return result;
}

auto AstapSolver::isConnected() const -> bool { return !solverPath_.empty(); }

auto AstapSolver::scanSolver() -> bool {
    logger_->debug("Scanning for Astap executable");

    if (isConnected()) {
        logger_->warn("Solver is already connected");
        return true;
    }

    if (!atom::system::checkSoftwareInstalled("astap")) {
        logger_->error("ASTAP not installed on system");
        return false;
    }

    auto astapCliPath = atom::system::getAppPath("astap");
    if (!atom::io::isExecutableFile(astapCliPath.string(), "astap")) {
        logger_->error("Found ASTAP path is not executable");
        return false;
    }

    solverPath_ = astapCliPath.string();
    solverVersion_ = atom::system::getAppVersion(astapCliPath.string());

    if (solverVersion_.empty()) {
        logger_->error("Failed to retrieve ASTAP version");
        return false;
    }

    logger_->info("Found ASTAP version: {}", solverVersion_);
    return true;
}

PlateSolveResult AstapSolver::solve(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH, int /*imageWidth*/, int /*imageHeight*/) {
    logger_->debug("Starting plate solve for image: {}", imageFilePath);

    if (!isConnected()) {
        logger_->error("Cannot solve: Solver not connected");
        return PlateSolveResult{.success = false};
    }

    if (!atom::io::isFileExists(imageFilePath)) {
        logger_->error("Cannot solve: Image file does not exist: {}",
                       imageFilePath);
        return PlateSolveResult{.success = false};
    }

    if (executeSolve(imageFilePath, initialCoordinates, fovW, fovH)) {
        return readWCS(imageFilePath);
    }

    logger_->error("Plate solving failed for image: {}", imageFilePath);
    return PlateSolveResult{.success = false};
}

std::future<PlateSolveResult> AstapSolver::async_solve(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH, int imageWidth, int imageHeight) {
    return std::async(std::launch::async,
                      [this, imageFilePath, initialCoordinates, fovW, fovH,
                       imageWidth, imageHeight]() {
                          return solve(imageFilePath, initialCoordinates, fovW,
                                       fovH, imageWidth, imageHeight);
                      });
}

auto AstapSolver::executeSolve(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH) -> bool {
    logger_->debug("Executing solve command for image: {}", imageFilePath);

    std::ostringstream command;
    command << solverPath_ << " -f " << imageFilePath;

    if (initialCoordinates) {
        command << " -ra " << initialCoordinates->ra;
        command << " -spd " << (90.0 - initialCoordinates->dec);
    }

    // ASTAP uses height for FOV parameter
    command << " -fov " << fovH;

    try {
        auto result = atom::system::executeCommand(command.str(), false);
        return result.find("Solution found:") != std::string::npos;
    } catch (const std::exception& ex) {
        logger_->error("Solve execution error: {}", ex.what());
        return false;
    }
}

auto AstapSolver::readWCS(const std::string& filename) -> PlateSolveResult {
    logger_->debug("Reading WCS data from: {}", filename);

    fitsfile* fptr;
    int status = 0;
    PlateSolveResult result;

    if (fits_open_file(&fptr, filename.c_str(), READONLY, &status)) {
        logger_->error("Failed to open FITS file: {}", filename);
        return PlateSolveResult{.success = false};
    }

    double ra = 0.0, dec = 0.0, cdelt1 = 0.0, cdelt2 = 0.0, crota2 = 0.0;
    char comment[FLEN_COMMENT];

    // Extract WCS keywords
    fits_read_key(fptr, TDOUBLE, "CRVAL1", &ra, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CRVAL2", &dec, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CDELT1", &cdelt1, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CDELT2", &cdelt2, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CROTA2", &crota2, comment, &status);

    fits_close_file(fptr, &status);

    if (status != 0) {
        logger_->error("Failed to read WCS keywords from: {}", filename);
        return PlateSolveResult{.success = false};
    }

    // Populate solution result
    result.success = true;
    result.coordinates = Coordinates{ra, dec};
    result.pixscale = std::abs(cdelt2) * 3600.0;  // Convert degrees to arcsec
    result.positionAngle = crota2;
    result.radius = 0.5 * std::abs(cdelt2) * 3600.0;

    logger_->debug("Successfully parsed WCS data");
    return result;
}

auto AstapSolver::getOutputPath(const std::string& imageFilePath) const
    -> std::string {
    return imageFilePath + ".wcs";
}

// Module registration
ATOM_MODULE(solver_astap, [](Component& component) {
    auto logger = spdlog::get("module_registry")
                      ? spdlog::get("module_registry")
                      : spdlog::stdout_color_mt<spdlog::synchronous_factory>(
                            "module_registry", spdlog::color_mode::automatic);

    logger->info("Registering solver_astap module");

    // Register public methods
    component.def("connect", &AstapSolver::connect, "main",
                  "Connect to astap solver");
    component.def("disconnect", &AstapSolver::disconnect, "main",
                  "Disconnect from astap solver");
    component.def("isConnected", &AstapSolver::isConnected, "main",
                  "Check if astap solver is connected");
    component.def("scanSolver", &AstapSolver::scan, "main",
                  "Scan for astap solver");
    component.def("solveImage", &AstapSolver::solve, "main",
                  "Solve image with various options");
    component.def("analyseImage", &AstapSolver::async_solve, "main",
                  "Analyse image and report HFD");

    component.addVariable("astap.instance", "Astap solver instance");
    component.defType<AstapSolver>("astap");

    // Factory method for creating solver instances
    component.def(
        "create_instance",
        [](const std::string& name) {
            return std::static_pointer_cast<AtomSolver>(
                std::make_shared<AstapSolver>(name));
        },
        "device", "Create a new solver instance");

    component.defType<AstapSolver>("solver.astap", "device",
                                   "Define a new solver instance");

    logger->info("solver_astap module registered successfully");
});