#include "astap.hpp"

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"
#include "atom/macro.hpp"
#include "atom/system/command.hpp"
#include "atom/system/process.hpp"
#include "atom/system/software.hpp"
#include "device/template/solver.hpp"

#include <fitsio.h>
#include <cmath>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

AstapSolver::AstapSolver(std::string name) : AtomSolver(std::move(name)) {
    DLOG_F(INFO, "Initializing Astap Solver...");
    if (!scanSolver()) {
        LOG_F(ERROR, "Failed to execute {}: Astap not installed",
              ATOM_FUNC_NAME);
        return;
    }
}

AstapSolver::~AstapSolver() { DLOG_F(INFO, "Destroying Astap Solver..."); }

auto AstapSolver::initialize() -> bool {
    DLOG_F(INFO, "Initializing Astap Solver...");
    return scanSolver();
}

auto AstapSolver::destroy() -> bool {
    DLOG_F(INFO, "Destroying Astap Solver...");
    return true;
}

auto AstapSolver::connect(const std::string& name, int /*timeout*/,
                          int /*maxRetry*/) -> bool {
    DLOG_F(INFO, "Attempting to connect to Astap Solver with name: {}", name);
    if (name.empty() || !atom::io::isFileNameValid(name) ||
        !atom::io::isFileExists(name)) {
        LOG_F(ERROR, "Failed to execute {}: Invalid Parameters",
              ATOM_FUNC_NAME);
        return false;
    }

    DLOG_F(INFO, "Connecting to Astap Solver...");
    solverPath_ = name;
    DLOG_F(INFO, "Connected to Astap Solver at path: {}", solverPath_);
    return true;
}

auto AstapSolver::disconnect() -> bool {
    DLOG_F(INFO, "Disconnecting from Astap Solver...");
    solverPath_.clear();
    LOG_F(INFO, "Checking if Astap Solver is running...");
    if (atom::system::isProcessRunning("astap")) {
        LOG_F(INFO, "Killing Astap Solver process...");
        try {
            atom::system::killProcessByName("astap", 15);
        } catch (const std::exception& ex) {
            LOG_F(ERROR, "Failed to kill Astap Solver process: {}", ex.what());
            return false;
        }
        LOG_F(INFO, "Astap Solver process killed successfully");
    }
    DLOG_F(INFO, "Disconnected from Astap Solver");
    return true;
}

auto AstapSolver::isConnected() const -> bool {
    DLOG_F(INFO, "Checking if Astap Solver is connected...");
    return !solverPath_.empty();
}

auto AstapSolver::scanSolver() -> bool {
    DLOG_F(INFO, "Scanning for Astap Solver...");
    if (isConnected()) {
        LOG_F(WARNING, "Solver is already connected");
        return true;
    }

    auto isAstapCli = atom::system::checkSoftwareInstalled("astap");
    if (!isAstapCli) {
        LOG_F(ERROR, "Failed to execute {}: Astap not installed",
              ATOM_FUNC_NAME);
        return false;
    }

    auto astapCliPath = atom::system::getAppPath("astap");
    if (!atom::io::isExecutableFile(astapCliPath.string(), "astap")) {
        LOG_F(ERROR, "Failed to execute {}: Astap not installed",
              ATOM_FUNC_NAME);
        return false;
    }

    solverPath_ = astapCliPath.string();
    solverVersion_ = atom::system::getAppVersion(astapCliPath.string());
    if (solverVersion_.empty()) {
        LOG_F(ERROR, "Failed to execute {}: Astap version not retrieved",
              ATOM_FUNC_NAME);
        return false;
    }

    LOG_F(INFO, "Current Astap version: {}", solverVersion_);
    return true;
}

PlateSolveResult AstapSolver::solve(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH, int /*imageWidth*/, int /*imageHeight*/) {
    DLOG_F(INFO, "Starting solve for image: {}", imageFilePath);
    if (!isConnected() || !atom::io::isFileExists(imageFilePath)) {
        LOG_F(ERROR, "Solver not connected or image file does not exist: {}",
              imageFilePath);
        return PlateSolveResult{.success = false};
    }

    if (executeSolve(imageFilePath, initialCoordinates, fovW, fovH)) {
        DLOG_F(INFO, "Solve executed successfully for image: {}",
               imageFilePath);
        return readWCS(imageFilePath);
    }
    LOG_F(ERROR, "Solve execution failed for image: {}", imageFilePath);
    return PlateSolveResult{.success = false};
}

std::future<PlateSolveResult> AstapSolver::async_solve(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH, int imageWidth, int imageHeight) {
    DLOG_F(INFO, "Starting async solve for image: {}", imageFilePath);
    return std::async(std::launch::async, [=]() {
        return solve(imageFilePath, initialCoordinates, fovW, fovH, imageWidth,
                     imageHeight);
    });
}

auto AstapSolver::executeSolve(
    const std::string& imageFilePath,
    const std::optional<Coordinates>& initialCoordinates, double fovW,
    double fovH) -> bool {
    DLOG_F(INFO, "Executing solve for image: {}", imageFilePath);
    std::ostringstream command;
    command << solverPath_ << " -f " << imageFilePath;

    if (initialCoordinates) {
        command << " -ra " << initialCoordinates->ra;
        command << " -spd " << (90.0 - initialCoordinates->dec);
    }

    command << " -fov " << fovH;  // ASTAP uses height for FOV

    try {
        auto ret = atom::system::executeCommand(command.str(), false);
        DLOG_F(INFO, "Solve command executed: {}", command.str());
        return ret.find("Solution found:") != std::string::npos;
    } catch (const std::exception& ex) {
        LOG_F(ERROR, "Solve execution failed: {}", ex.what());
        return false;
    }
}

auto AstapSolver::readWCS(const std::string& filename) -> PlateSolveResult {
    DLOG_F(INFO, "Reading WCS from file: {}", filename);
    fitsfile* fptr;
    int status = 0;
    PlateSolveResult result;

    if (fits_open_file(&fptr, filename.c_str(), READONLY, &status)) {
        LOG_F(ERROR, "Failed to open FITS file: {}", filename);
        return PlateSolveResult{.success = false};
    }

    double ra, dec, cdelt1, cdelt2, crota2;
    char comment[FLEN_COMMENT];

    // Read WCS keywords
    fits_read_key(fptr, TDOUBLE, "CRVAL1", &ra, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CRVAL2", &dec, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CDELT1", &cdelt1, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CDELT2", &cdelt2, comment, &status);
    fits_read_key(fptr, TDOUBLE, "CROTA2", &crota2, comment, &status);

    fits_close_file(fptr, &status);

    if (status != 0) {
        LOG_F(ERROR, "Failed to read WCS from FITS file: {}", filename);
        return PlateSolveResult{.success = false};
    }

    result.success = true;
    result.coordinates = Coordinates{ra, dec};
    result.pixscale = std::abs(cdelt2) * 3600.0;  // Convert degrees to arcsec
    result.positionAngle = crota2;
    result.radius = 0.5 * std::abs(cdelt2) * 3600.0;  // Estimate search radius

    DLOG_F(INFO, "WCS read successfully from file: {}", filename);
    return result;
}

std::string AstapSolver::getOutputPath(const std::string& imageFilePath) const {
    DLOG_F(INFO, "Generating output path for image: {}", imageFilePath);
    return imageFilePath + ".wcs";
}

ATOM_MODULE(solver_astap, [](Component& component) {
    LOG_F(INFO, "Registering solver_astap module...");

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

    component.def(
        "create_instance",
        [](const std::string& name) {
            std::shared_ptr<AtomSolver> instance =
                std::make_shared<AstapSolver>(name);
            return instance;
        },
        "device", "Create a new solver instance.");
    component.defType<AstapSolver>("solver.astap", "device",
                                   "Define a new solver instance.");

    LOG_F(INFO, "Registered solver_astap module.");
});
