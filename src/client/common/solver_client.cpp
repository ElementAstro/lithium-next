/*
 * solver_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Implementation of solver client base class

*************************************************/

#include "solver_client.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>

namespace lithium::client {

SolverClient::SolverClient(std::string name)
    : ClientBase(std::move(name), ClientType::Solver) {
    setCapabilities(ClientCapability::Connect | ClientCapability::Scan |
                    ClientCapability::Configure |
                    ClientCapability::AsyncOperation |
                    ClientCapability::StatusQuery);
    spdlog::debug("SolverClient created: {}", getName());
}

SolverClient::~SolverClient() {
    if (solving_.load()) {
        abort();
    }
    spdlog::debug("SolverClient destroyed: {}", getName());
}

std::future<PlateSolveResult> SolverClient::solveAsync(
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

void SolverClient::abort() {
    if (solving_.load()) {
        abortRequested_.store(true);
        spdlog::info("Solver {} abort requested", getName());
        emitEvent("abort_requested", "");
    }
}

std::string SolverClient::getOutputPath(
    const std::string& imageFilePath) const {
    std::filesystem::path imagePath(imageFilePath);
    std::filesystem::path outputDir =
        options_.outputDir.empty() ? imagePath.parent_path()
                                   : std::filesystem::path(options_.outputDir);

    std::string baseName = imagePath.stem().string();
    return (outputDir / (baseName + "_solved.wcs")).string();
}

}  // namespace lithium::client
