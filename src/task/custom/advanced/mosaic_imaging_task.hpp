#ifndef LITHIUM_TASK_ADVANCED_MOSAIC_IMAGING_TASK_HPP
#define LITHIUM_TASK_ADVANCED_MOSAIC_IMAGING_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Automated Mosaic Imaging Task
 * 
 * Creates large field-of-view mosaics by automatically capturing
 * multiple overlapping frames across a defined area of sky.
 */
class MosaicImagingTask : public Task {
public:
    MosaicImagingTask()
        : Task("MosaicImaging",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "MosaicImaging"; }

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateMosaicImagingParameters(const json& params);

private:
    void executeImpl(const json& params);
    std::vector<json> calculateMosaicTiles(const json& params);
    void captureMosaicTile(const json& tile, int tileNumber, int totalTiles);
    json calculateTileCoordinates(double centerRA, double centerDec, 
                                 double width, double height, 
                                 int tilesX, int tilesY, 
                                 double overlapPercent);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_MOSAIC_IMAGING_TASK_HPP
