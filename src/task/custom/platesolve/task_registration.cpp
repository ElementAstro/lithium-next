#include "platesolve_exposure.hpp"
#include "centering_task.hpp"
#include "mosaic_task.hpp"
#include "platesolve_tasks.hpp"
#include "../factory.hpp"

namespace lithium::task::platesolve {

// ==================== Task Registration ====================

namespace {
using namespace lithium::task;

// Register PlateSolveExposureTask
AUTO_REGISTER_TASK(
    PlateSolveExposureTask, "PlateSolveExposure",
    (TaskInfo{
        "PlateSolveExposure",
        "Take an exposure and perform plate solving for astrometry",
        "Astrometry",
        {},  // No required parameters
        json{
            {"type", "object"},
            {"properties", json{
                {"exposure", json{
                    {"type", "number"},
                    {"minimum", 0.1},
                    {"maximum", 120.0},
                    {"description", "Exposure time in seconds"}
                }},
                {"binning", json{
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 4},
                    {"description", "Camera binning factor"}
                }},
                {"max_attempts", json{
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 10},
                    {"description", "Maximum solve attempts"}
                }},
                {"timeout", json{
                    {"type", "number"},
                    {"minimum", 10.0},
                    {"maximum", 600.0},
                    {"description", "Solve timeout in seconds"}
                }},
                {"gain", json{
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "Camera gain"}
                }},
                {"offset", json{
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "Camera offset"}
                }},
                {"solver_type", json{
                    {"type", "string"},
                    {"enum", json::array({"astrometry", "astap"})},
                    {"description", "Plate solver type"}
                }},
                {"use_initial_coordinates", json{
                    {"type", "boolean"},
                    {"description", "Use initial coordinates hint"}
                }},
                {"fov_width", json{
                    {"type", "number"},
                    {"minimum", 0.1},
                    {"maximum", 10.0},
                    {"description", "Field of view width in degrees"}
                }},
                {"fov_height", json{
                    {"type", "number"},
                    {"minimum", 0.1},
                    {"maximum", 10.0},
                    {"description", "Field of view height in degrees"}
                }}
            }}
        },
        "2.0.0",
        {}
    }));

// Register CenteringTask
AUTO_REGISTER_TASK(
    CenteringTask, "Centering",
    (TaskInfo{
        "Centering",
        "Center the telescope on a target using iterative plate solving",
        "Astrometry",
        {"target_ra", "target_dec"},  // Required parameters
        json{
            {"type", "object"},
            {"properties", json{
                {"target_ra", json{
                    {"type", "number"},
                    {"minimum", 0.0},
                    {"maximum", 24.0},
                    {"description", "Target Right Ascension in hours"}
                }},
                {"target_dec", json{
                    {"type", "number"},
                    {"minimum", -90.0},
                    {"maximum", 90.0},
                    {"description", "Target Declination in degrees"}
                }},
                {"tolerance", json{
                    {"type", "number"},
                    {"minimum", 1.0},
                    {"maximum", 300.0},
                    {"description", "Centering tolerance in arcseconds"}
                }},
                {"max_iterations", json{
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 10},
                    {"description", "Maximum centering iterations"}
                }},
                {"exposure", json{
                    {"type", "number"},
                    {"minimum", 0.1},
                    {"maximum", 120.0},
                    {"description", "Plate solve exposure time"}
                }},
                {"binning", json{
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 4},
                    {"description", "Camera binning factor"}
                }},
                {"gain", json{
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "Camera gain"}
                }},
                {"offset", json{
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "Camera offset"}
                }},
                {"solver_type", json{
                    {"type", "string"},
                    {"enum", json::array({"astrometry", "astap"})},
                    {"description", "Plate solver type"}
                }},
                {"fov_width", json{
                    {"type", "number"},
                    {"minimum", 0.1},
                    {"maximum", 10.0},
                    {"description", "Field of view width in degrees"}
                }},
                {"fov_height", json{
                    {"type", "number"},
                    {"minimum", 0.1},
                    {"maximum", 10.0},
                    {"description", "Field of view height in degrees"}
                }}
            }},
            {"required", json::array({"target_ra", "target_dec"})}
        },
        "2.0.0",
        {}
    }));

// Register MosaicTask
AUTO_REGISTER_TASK(
    MosaicTask, "Mosaic",
    (TaskInfo{
        "Mosaic",
        "Perform automated mosaic imaging with plate solving and centering",
        "Astrometry",
        {"center_ra", "center_dec", "grid_width", "grid_height"},  // Required parameters
        json{
            {"type", "object"},
            {"properties", json{
                {"center_ra", json{
                    {"type", "number"},
                    {"minimum", 0.0},
                    {"maximum", 24.0},
                    {"description", "Mosaic center RA in hours"}
                }},
                {"center_dec", json{
                    {"type", "number"},
                    {"minimum", -90.0},
                    {"maximum", 90.0},
                    {"description", "Mosaic center Dec in degrees"}
                }},
                {"grid_width", json{
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 10},
                    {"description", "Number of columns in mosaic grid"}
                }},
                {"grid_height", json{
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 10},
                    {"description", "Number of rows in mosaic grid"}
                }},
                {"overlap", json{
                    {"type", "number"},
                    {"minimum", 0.0},
                    {"maximum", 50.0},
                    {"description", "Frame overlap percentage"}
                }},
                {"frame_exposure", json{
                    {"type", "number"},
                    {"minimum", 0.1},
                    {"maximum", 3600.0},
                    {"description", "Exposure time per frame in seconds"}
                }},
                {"frames_per_position", json{
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 10},
                    {"description", "Number of frames per mosaic position"}
                }},
                {"auto_center", json{
                    {"type", "boolean"},
                    {"description", "Auto-center each position"}
                }},
                {"gain", json{
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "Camera gain"}
                }},
                {"offset", json{
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "Camera offset"}
                }},
                {"centering_tolerance", json{
                    {"type", "number"},
                    {"minimum", 1.0},
                    {"maximum", 300.0},
                    {"description", "Centering tolerance in arcseconds"}
                }},
                {"centering_max_iterations", json{
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 10},
                    {"description", "Maximum centering iterations"}
                }},
                {"centering_exposure", json{
                    {"type", "number"},
                    {"minimum", 0.1},
                    {"maximum", 120.0},
                    {"description", "Centering exposure time"}
                }},
                {"centering_binning", json{
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 4},
                    {"description", "Centering binning factor"}
                }},
                {"solver_type", json{
                    {"type", "string"},
                    {"enum", json::array({"astrometry", "astap"})},
                    {"description", "Plate solver type"}
                }}
            }},
            {"required", json::array({"center_ra", "center_dec", "grid_width", "grid_height"})}
        },
        "2.0.0",
        {}
    }));

}  // namespace

}  // namespace lithium::task::platesolve
