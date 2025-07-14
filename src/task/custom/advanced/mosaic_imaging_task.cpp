#include "mosaic_imaging_task.hpp"
#include <chrono>
#include <memory>
#include <thread>
#include <cmath>
#include "deep_sky_sequence_task.hpp"

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace lithium::task::task {

auto MosaicImagingTask::taskName() -> std::string { return "MosaicImaging"; }

void MosaicImagingTask::execute(const json& params) { executeImpl(params); }

void MosaicImagingTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing MosaicImaging task '{}' with params: {}",
          getName(), params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::string targetName = params.value("target_name", "Mosaic");
        double centerRA = params["center_ra"].get<double>();
        double centerDec = params["center_dec"].get<double>();
        double mosaicWidth = params.value("mosaic_width_degrees", 2.0);
        double mosaicHeight = params.value("mosaic_height_degrees", 2.0);
        int tilesX = params.value("tiles_x", 2);
        int tilesY = params.value("tiles_y", 2);
        double overlapPercent = params.value("overlap_percent", 20.0);

        // Exposure parameters
        int exposuresPerTile = params.value("exposures_per_tile", 10);
        double exposureTime = params.value("exposure_time", 300.0);
        std::vector<std::string> filters =
            params.value("filters", std::vector<std::string>{"L"});
        bool dithering = params.value("dithering", true);
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);

        LOG_F(INFO, "Starting mosaic '{}' - Center: {:.3f}h, {:.3f}° - Size: {:.1f}°×{:.1f}° - Grid: {}×{}",
              targetName, centerRA, centerDec, mosaicWidth, mosaicHeight, tilesX, tilesY);

        // Calculate mosaic tile positions
        std::vector<json> mosaicTiles = calculateMosaicTiles(params);
        int totalTiles = mosaicTiles.size();

        LOG_F(INFO, "Mosaic will capture {} tiles with {:.1f}% overlap",
              totalTiles, overlapPercent);

        // Capture each tile
        for (size_t tileIndex = 0; tileIndex < mosaicTiles.size(); ++tileIndex) {
            const json& tile = mosaicTiles[tileIndex];

            LOG_F(INFO, "Starting tile {} of {} - Position: {:.3f}h, {:.3f}°",
                  tileIndex + 1, totalTiles,
                  tile["ra"].get<double>(), tile["dec"].get<double>());

            try {
                captureMosaicTile(tile, tileIndex + 1, totalTiles);

                LOG_F(INFO, "Tile {} completed successfully", tileIndex + 1);

            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to capture tile {}: {}", tileIndex + 1, e.what());

                // Ask user if they want to continue with remaining tiles
                LOG_F(WARNING, "Continuing with remaining tiles...");
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::hours>(
            endTime - startTime);
        LOG_F(INFO, "MosaicImaging task '{}' completed {} tiles in {} hours",
              getName(), totalTiles, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(ERROR, "MosaicImaging task '{}' failed after {} minutes: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

std::vector<json> MosaicImagingTask::calculateMosaicTiles(const json& params) {
    double centerRA = params["center_ra"].get<double>();
    double centerDec = params["center_dec"].get<double>();
    double mosaicWidth = params.value("mosaic_width_degrees", 2.0);
    double mosaicHeight = params.value("mosaic_height_degrees", 2.0);
    int tilesX = params.value("tiles_x", 2);
    int tilesY = params.value("tiles_y", 2);
    double overlapPercent = params.value("overlap_percent", 20.0);

    std::vector<json> tiles;

    // Calculate tile size with overlap
    double tileWidth = mosaicWidth / tilesX;
    double tileHeight = mosaicHeight / tilesY;

    // Calculate step size (accounting for overlap)
    double stepX = tileWidth * (1.0 - overlapPercent / 100.0);
    double stepY = tileHeight * (1.0 - overlapPercent / 100.0);

    // Calculate starting position (top-left of mosaic)
    double startRA = centerRA - (mosaicWidth / 2.0) / 15.0; // Convert degrees to hours
    double startDec = centerDec + (mosaicHeight / 2.0);

    LOG_F(INFO, "Calculating {} tiles - Tile size: {:.3f}°×{:.3f}°, Step: {:.3f}°×{:.3f}°",
          tilesX * tilesY, tileWidth, tileHeight, stepX, stepY);

    for (int y = 0; y < tilesY; ++y) {
        for (int x = 0; x < tilesX; ++x) {
            // Calculate tile center position
            double tileRA = startRA + (x * stepX + tileWidth / 2.0) / 15.0;
            double tileDec = startDec - (y * stepY + tileHeight / 2.0);

            // Ensure RA is in valid range [0, 24)
            while (tileRA < 0) tileRA += 24.0;
            while (tileRA >= 24.0) tileRA -= 24.0;

            json tile = {
                {"tile_x", x},
                {"tile_y", y},
                {"ra", tileRA},
                {"dec", tileDec},
                {"width", tileWidth},
                {"height", tileHeight}
            };

            tiles.push_back(tile);

            LOG_F(INFO, "Tile {},{}: RA={:.3f}h, Dec={:.3f}°",
                  x, y, tileRA, tileDec);
        }
    }

    return tiles;
}

void MosaicImagingTask::captureMosaicTile(const json& tile, int tileNumber, int totalTiles) {
    double tileRA = tile["ra"].get<double>();
    double tileDec = tile["dec"].get<double>();
    int tileX = tile["tile_x"].get<int>();
    int tileY = tile["tile_y"].get<int>();

    LOG_F(INFO, "Capturing mosaic tile {}/{} at position ({},{}) - {:.3f}h, {:.3f}°",
          tileNumber, totalTiles, tileX, tileY, tileRA, tileDec);

    // Slew to tile position
    LOG_F(INFO, "Slewing to tile position");
    std::this_thread::sleep_for(std::chrono::seconds(10)); // Simulate slewing

    // Plate solve and center
    LOG_F(INFO, "Plate solving and centering tile");
    std::this_thread::sleep_for(std::chrono::seconds(15)); // Simulate plate solving

    // Create target name for this tile
    std::string tileName = "Tile_" + std::to_string(tileX) + "_" + std::to_string(tileY);

    // Prepare deep sky sequence parameters for this tile
    json tileParams = {
        {"target_name", tileName},
        {"total_exposures", 10}, // Default, should come from parent params
        {"exposure_time", 300.0}, // Default, should come from parent params
        {"filters", json::array({"L"})}, // Default, should come from parent params
        {"dithering", true},
        {"binning", 1},
        {"gain", 100},
        {"offset", 10}
    };

    // Execute imaging sequence for this tile
    auto deepSkyTask = DeepSkySequenceTask::createEnhancedTask();
    deepSkyTask->execute(tileParams);

    LOG_F(INFO, "Tile {}/{} capture completed", tileNumber, totalTiles);
}

json MosaicImagingTask::calculateTileCoordinates(double centerRA, double centerDec,
                                                double width, double height,
                                                int tilesX, int tilesY,
                                                double overlapPercent) {
    // This is a helper function for more complex coordinate calculations
    // For now, delegate to the main calculation method
    json params = {
        {"center_ra", centerRA},
        {"center_dec", centerDec},
        {"mosaic_width_degrees", width},
        {"mosaic_height_degrees", height},
        {"tiles_x", tilesX},
        {"tiles_y", tilesY},
        {"overlap_percent", overlapPercent}
    };

    std::vector<json> tiles = calculateMosaicTiles(params);
    return json{{"tiles", tiles}};
}

void MosaicImagingTask::validateMosaicImagingParameters(const json& params) {
    if (!params.contains("center_ra") || !params["center_ra"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid center_ra parameter");
    }

    if (!params.contains("center_dec") || !params["center_dec"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid center_dec parameter");
    }

    double centerRA = params["center_ra"].get<double>();
    if (centerRA < 0 || centerRA >= 24) {
        THROW_INVALID_ARGUMENT("Center RA must be between 0 and 24 hours");
    }

    double centerDec = params["center_dec"].get<double>();
    if (centerDec < -90 || centerDec > 90) {
        THROW_INVALID_ARGUMENT("Center Dec must be between -90 and 90 degrees");
    }

    if (params.contains("tiles_x")) {
        int tilesX = params["tiles_x"].get<int>();
        if (tilesX < 1 || tilesX > 10) {
            THROW_INVALID_ARGUMENT("Tiles X must be between 1 and 10");
        }
    }

    if (params.contains("tiles_y")) {
        int tilesY = params["tiles_y"].get<int>();
        if (tilesY < 1 || tilesY > 10) {
            THROW_INVALID_ARGUMENT("Tiles Y must be between 1 and 10");
        }
    }

    if (params.contains("overlap_percent")) {
        double overlap = params["overlap_percent"].get<double>();
        if (overlap < 0 || overlap > 50) {
            THROW_INVALID_ARGUMENT("Overlap percent must be between 0 and 50");
        }
    }
}

auto MosaicImagingTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<MosaicImagingTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced MosaicImaging task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(5);
    task->setTimeout(std::chrono::seconds(43200));  // 12 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void MosaicImagingTask::defineParameters(Task& task) {
    task.addParamDefinition("target_name", "string", false, "Mosaic",
                            "Name of the mosaic target");
    task.addParamDefinition("center_ra", "double", true, 0.0,
                            "Center right ascension in hours");
    task.addParamDefinition("center_dec", "double", true, 0.0,
                            "Center declination in degrees");
    task.addParamDefinition("mosaic_width_degrees", "double", false, 2.0,
                            "Total mosaic width in degrees");
    task.addParamDefinition("mosaic_height_degrees", "double", false, 2.0,
                            "Total mosaic height in degrees");
    task.addParamDefinition("tiles_x", "int", false, 2,
                            "Number of tiles in X direction");
    task.addParamDefinition("tiles_y", "int", false, 2,
                            "Number of tiles in Y direction");
    task.addParamDefinition("overlap_percent", "double", false, 20.0,
                            "Overlap percentage between tiles");
    task.addParamDefinition("exposures_per_tile", "int", false, 10,
                            "Number of exposures per tile");
    task.addParamDefinition("exposure_time", "double", false, 300.0,
                            "Exposure time in seconds");
    task.addParamDefinition("filters", "array", false, json::array({"L"}),
                            "List of filters to use");
    task.addParamDefinition("dithering", "bool", false, true,
                            "Enable dithering between exposures");
    task.addParamDefinition("binning", "int", false, 1, "Camera binning");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

}  // namespace lithium::task::task
