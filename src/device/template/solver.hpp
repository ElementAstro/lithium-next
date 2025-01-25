/*
 * solver.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomSolver Simulator and Basic Definition

*************************************************/

#pragma once

#include "device.hpp"

#include <future>
#include <optional>

#include "atom/macro.hpp"

template <typename T>
concept CoordinateType = requires(T t) {
    { t.ra } -> std::convertible_to<double>;
    { t.dec } -> std::convertible_to<double>;
};

struct Coordinates {
    double ra;   // in degrees
    double dec;  // in degrees
} ATOM_ALIGNAS(16);

struct PlateSolveResult {
    bool success{};
    Coordinates coordinates{};
    double pixscale{};
    double positionAngle{};
    std::optional<bool> flipped;
    double radius{};
} ATOM_ALIGNAS(64);

class AtomSolver : public AtomDriver {
public:
    explicit AtomSolver(std::string name) : AtomDriver(std::move(name)) {}

    virtual ~AtomSolver() = default;

    virtual PlateSolveResult solve(
        const std::string& imageFilePath,
        const std::optional<Coordinates>& initialCoordinates, double fovW,
        double fovH, int imageWidth, int imageHeight) = 0;

    virtual std::future<PlateSolveResult> async_solve(
        const std::string& imageFilePath,
        const std::optional<Coordinates>& initialCoordinates, double fovW,
        double fovH, int imageWidth, int imageHeight) = 0;

protected:
    virtual double toRadians(double degrees) = 0;
    virtual double toDegrees(double radians) = 0;
    virtual double arcsecToDegree(double arcsec) = 0;

    virtual std::string getOutputPath(
        const std::string& imageFilePath) const = 0;
};
