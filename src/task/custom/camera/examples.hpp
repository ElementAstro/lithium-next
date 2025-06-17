#ifndef LITHIUM_TASK_CAMERA_EXAMPLES_HPP
#define LITHIUM_TASK_CAMERA_EXAMPLES_HPP

/**
 * @file camera_examples.hpp
 * @brief Examples demonstrating the usage of the optimized camera task system
 * 
 * This file contains practical examples showing how to use the comprehensive
 * camera task system for various astrophotography scenarios.
 * 
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "camera_tasks.hpp"
#include "atom/type/json.hpp"

namespace lithium::task::examples {

using json = nlohmann::json;

/**
 * @brief Example: Complete imaging session setup
 * 
 * Demonstrates setting up a complete imaging session with:
 * - Temperature stabilization
 * - Parameter optimization
 * - Frame configuration
 * - Calibration frames
 * - Science exposures
 */
class ImagingSessionExample {
public:
    static auto createFullImagingSequence() -> json {
        return json{
            {"sequence_name", "Deep Sky Imaging Session"},
            {"description", "Complete imaging workflow with cooling, calibration, and science frames"},
            {"tasks", json::array({
                // 1. Temperature Control
                {
                    {"task", "CoolingControl"},
                    {"params", {
                        {"enable", true},
                        {"target_temperature", -15.0},
                        {"wait_for_stabilization", true},
                        {"max_wait_time", 600},
                        {"tolerance", 1.0}
                    }}
                },
                
                // 2. Parameter Optimization
                {
                    {"task", "AutoParameter"},
                    {"params", {
                        {"target", "snr"},
                        {"iterations", 5}
                    }}
                },
                
                // 3. Frame Configuration
                {
                    {"task", "FrameConfig"},
                    {"params", {
                        {"width", 4096},
                        {"height", 4096},
                        {"binning", {{"x", 1}, {"y", 1}}},
                        {"frame_type", "FITS"},
                        {"upload_mode", "LOCAL"}
                    }}
                },
                
                // 4. Calibration Frames
                {
                    {"task", "AutoCalibration"},
                    {"params", {
                        {"dark_count", 20},
                        {"bias_count", 50},
                        {"flat_count", 20},
                        {"dark_exposure", 300},
                        {"flat_exposure", 5}
                    }}
                },
                
                // 5. Science Exposures
                {
                    {"task", "TakeManyExposure"},
                    {"params", {
                        {"count", 50},
                        {"exposure", 300},
                        {"type", "light"},
                        {"delay", 2},
                        {"fileName", "NGC7000_L_{:03d}"},
                        {"path", "/data/imaging/NGC7000"}
                    }}
                }
            })}
        };
    }
};

/**
 * @brief Example: Video streaming and monitoring
 * 
 * Demonstrates video functionality for:
 * - Live view setup
 * - Recording sessions
 * - Performance monitoring
 */
class VideoStreamingExample {
public:
    static auto createVideoStreamingSequence() -> json {
        return json{
            {"sequence_name", "Video Streaming Session"},
            {"description", "Complete video streaming workflow"},
            {"tasks", json::array({
                // 1. Start Video Stream
                {
                    {"task", "StartVideo"},
                    {"params", {
                        {"stabilize_delay", 2000},
                        {"format", "RGB24"},
                        {"fps", 30.0}
                    }}
                },
                
                // 2. Monitor Stream Quality
                {
                    {"task", "VideoStreamMonitor"},
                    {"params", {
                        {"monitor_duration", 60},
                        {"report_interval", 10}
                    }}
                },
                
                // 3. Record Video
                {
                    {"task", "RecordVideo"},
                    {"params", {
                        {"duration", 120},
                        {"filename", "planetary_observation.mp4"},
                        {"quality", "high"},
                        {"fps", 30.0}
                    }}
                },
                
                // 4. Stop Video Stream
                {
                    {"task", "StopVideo"},
                    {"params", {}}
                }
            })}
        };
    }
};

/**
 * @brief Example: ROI (Region of Interest) imaging
 * 
 * Demonstrates subframe imaging for:
 * - Planetary imaging
 * - Variable star monitoring
 * - High-cadence observations
 */
class ROIImagingExample {
public:
    static auto createROIImagingSequence() -> json {
        return json{
            {"sequence_name", "ROI Planetary Imaging"},
            {"description", "High-cadence planetary imaging with ROI"},
            {"tasks", json::array({
                // 1. Configure ROI
                {
                    {"task", "ROIConfig"},
                    {"params", {
                        {"x", 1500},
                        {"y", 1500},
                        {"width", 1000},
                        {"height", 1000}
                    }}
                },
                
                // 2. Set High Speed Binning
                {
                    {"task", "BinningConfig"},
                    {"params", {
                        {"horizontal", 2},
                        {"vertical", 2}
                    }}
                },
                
                // 3. Optimize for Speed
                {
                    {"task", "AutoParameter"},
                    {"params", {
                        {"target", "speed"}
                    }}
                },
                
                // 4. High-Cadence Exposures
                {
                    {"task", "TakeManyExposure"},
                    {"params", {
                        {"count", 1000},
                        {"exposure", 0.1},
                        {"type", "light"},
                        {"delay", 0},
                        {"fileName", "Jupiter_{:04d}"},
                        {"path", "/data/planetary/jupiter"}
                    }}
                }
            })}
        };
    }
};

/**
 * @brief Example: Temperature monitoring session
 * 
 * Demonstrates thermal management for:
 * - Long exposure sessions
 * - Thermal noise characterization
 * - Cooling system optimization
 */
class ThermalManagementExample {
public:
    static auto createThermalMonitoringSequence() -> json {
        return json{
            {"sequence_name", "Thermal Management Session"},
            {"description", "Comprehensive thermal monitoring and optimization"},
            {"tasks", json::array({
                // 1. Temperature Alert Setup
                {
                    {"task", "TemperatureAlert"},
                    {"params", {
                        {"max_temperature", 35.0},
                        {"min_temperature", -25.0},
                        {"monitor_time", 3600},
                        {"check_interval", 60}
                    }}
                },
                
                // 2. Cooling Optimization
                {
                    {"task", "CoolingOptimization"},
                    {"params", {
                        {"target_temperature", -20.0},
                        {"optimization_time", 600}
                    }}
                },
                
                // 3. Temperature Stabilization
                {
                    {"task", "TemperatureStabilization"},
                    {"params", {
                        {"target_temperature", -20.0},
                        {"tolerance", 0.5},
                        {"max_wait_time", 900},
                        {"check_interval", 30}
                    }}
                },
                
                // 4. Continuous Monitoring
                {
                    {"task", "TemperatureMonitor"},
                    {"params", {
                        {"duration", 7200},
                        {"interval", 60}
                    }}
                }
            })}
        };
    }
};

/**
 * @brief Example: Parameter profile management
 * 
 * Demonstrates profile system for:
 * - Different target types (galaxies, nebulae, planets)
 * - Equipment configurations
 * - Quick setup switching
 */
class ProfileManagementExample {
public:
    static auto createProfileManagementSequence() -> json {
        return json{
            {"sequence_name", "Parameter Profile Management"},
            {"description", "Save and load parameter profiles for different scenarios"},
            {"tasks", json::array({
                // 1. Setup Deep Sky Profile
                {
                    {"task", "GainControl"},
                    {"params", {{"gain", 200}}}
                },
                {
                    {"task", "OffsetControl"},
                    {"params", {{"offset", 15}}}
                },
                {
                    {"task", "ParameterProfile"},
                    {"params", {
                        {"action", "save"},
                        {"name", "deep_sky_profile"}
                    }}
                },
                
                // 2. Setup Planetary Profile
                {
                    {"task", "GainControl"},
                    {"params", {{"gain", 50}}}
                },
                {
                    {"task", "OffsetControl"},
                    {"params", {{"offset", 8}}}
                },
                {
                    {"task", "ParameterProfile"},
                    {"params", {
                        {"action", "save"},
                        {"name", "planetary_profile"}
                    }}
                },
                
                // 3. List Available Profiles
                {
                    {"task", "ParameterProfile"},
                    {"params", {
                        {"action", "list"}
                    }}
                },
                
                // 4. Load Deep Sky Profile
                {
                    {"task", "ParameterProfile"},
                    {"params", {
                        {"action", "load"},
                        {"name", "deep_sky_profile"}
                    }}
                }
            })}
        };
    }
};

/**
 * @brief Helper function to execute a task sequence
 * 
 * This function demonstrates how to programmatically execute
 * the task sequences defined in the examples above.
 */
auto executeTaskSequence(const json& sequence) -> bool;

}  // namespace lithium::task::examples

#endif  // LITHIUM_TASK_CAMERA_EXAMPLES_HPP
