#ifndef LITHIUM_CLIENT_ASTAP_HPP
#define LITHIUM_CLIENT_ASTAP_HPP

#include "device/template/solver.hpp"
#include <cmath>
#include <string>
#include <vector>

class AstapSolver : public AtomSolver {
public:
    explicit AstapSolver(std::string name);
    ~AstapSolver() override;

    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& name, int timeout = 30, int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    PlateSolveResult solve(const std::string& imageFilePath,
                          const std::optional<Coordinates>& initialCoordinates,
                          double fovW, double fovH,
                          int imageWidth, int imageHeight) override;

    std::future<PlateSolveResult> async_solve(const std::string& imageFilePath,
                                            const std::optional<Coordinates>& initialCoordinates,
                                            double fovW, double fovH,
                                            int imageWidth, int imageHeight) override;

protected:
    double toRadians(double degrees) override { return degrees * M_PI / 180.0; }
    double toDegrees(double radians) override { return radians * 180.0 / M_PI; }
    double arcsecToDegree(double arcsec) override { return arcsec / 3600.0; }
    std::string getOutputPath(const std::string& imageFilePath) const override;

private:
    auto scanSolver() -> bool;
    auto readWCS(const std::string& filename) -> PlateSolveResult;
    auto executeSolve(const std::string& imageFilePath,
                     const std::optional<Coordinates>& initialCoordinates,
                     double fovW, double fovH) -> bool;

    std::string solverPath_;
    std::string solverVersion_;
};

#endif
