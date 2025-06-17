#ifndef LITHIUM_TASK_CAMERA_ALL_TASKS_HPP
#define LITHIUM_TASK_CAMERA_ALL_TASKS_HPP

/**
 * @file camera_tasks.hpp
 * @brief Comprehensive camera task system for astrophotography
 * 
 * This header aggregates all camera-related tasks providing complete functionality
 * for professional astrophotography control including:
 * 
 * - Basic exposure control and calibration
 * - Video streaming and recording
 * - Temperature management and cooling
 * - Frame configuration and analysis
 * - Parameter control and profiles
 * - Telescope integration and coordination
 * - Device management and health monitoring
 * - Advanced filter and focus control
 * - Intelligent sequences and analysis
 * - Environmental monitoring and safety
 * 
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

// ==================== Core Camera Tasks ====================
#include "common.hpp"              // Common enums and utilities
#include "basic_exposure.hpp"      // Basic exposure tasks (single, multiple, subframe)
#include "calibration_tasks.hpp"   // Calibration frame tasks (darks, bias, flats)

// ==================== Advanced Camera Control ====================
#include "video_tasks.hpp"         // Video streaming and recording tasks
#include "temperature_tasks.hpp"   // Temperature control and monitoring
#include "frame_tasks.hpp"         // Frame configuration and management
#include "parameter_tasks.hpp"     // Gain, offset, ISO parameter control

// ==================== System Integration ====================
#include "telescope_tasks.hpp"     // Telescope integration and coordination
#include "device_coordination_tasks.hpp"  // Device management and coordination
#include "sequence_analysis_tasks.hpp"    // Advanced sequences and analysis

namespace lithium::task::task {

/**
 * @brief Camera task category enumeration
 * Defines all available camera task categories
 */
enum class CameraTaskCategory {
    EXPOSURE,           ///< Basic exposure control
    CALIBRATION,        ///< Calibration frame management
    VIDEO,              ///< Video streaming and recording
    TEMPERATURE,        ///< Temperature management
    FRAME,              ///< Frame configuration
    PARAMETER,          ///< Parameter control
    TELESCOPE,          ///< Telescope integration
    DEVICE,             ///< Device coordination
    FILTER,             ///< Filter wheel control
    FOCUS,              ///< Focus control and optimization
    SEQUENCE,           ///< Advanced sequences
    ANALYSIS,           ///< Image analysis
    SAFETY,             ///< Safety and monitoring
    SYSTEM              ///< System management
};

/**
 * @brief Camera task system information
 * Comprehensive system providing 48+ tasks for complete astrophotography control
 */
struct CameraTaskSystemInfo {
    static constexpr const char* VERSION = "2.0.0";
    static constexpr const char* BUILD_DATE = __DATE__;
    static constexpr int TOTAL_TASKS = 48;  // Updated total count
    
    struct Categories {
        static constexpr int EXPOSURE = 4;       // Basic exposure tasks
        static constexpr int CALIBRATION = 4;    // Calibration tasks
        static constexpr int VIDEO = 5;          // Video tasks
        static constexpr int TEMPERATURE = 5;    // Temperature tasks
        static constexpr int FRAME = 6;          // Frame tasks
        static constexpr int PARAMETER = 6;      // Parameter tasks
        static constexpr int TELESCOPE = 6;      // Telescope integration
        static constexpr int DEVICE = 7;         // Device coordination
        static constexpr int SEQUENCE = 7;       // Advanced sequences
        static constexpr int ANALYSIS = 4;       // Analysis tasks
    };
};

/**
 * @brief Namespace documentation for camera tasks
 * 
 * This namespace contains all camera-related task implementations that provide
 * comprehensive control over camera functionality including:
 * 
 * CORE FUNCTIONALITY:
 * - Basic exposures (single, multiple, subframe)
 * - Video streaming and recording 
 * - Temperature control and monitoring
 * - Frame configuration and management
 * - Parameter control (gain, offset, ISO)
 * - Calibration frame acquisition
 * 
 * ADVANCED INTEGRATION:
 * - Telescope slewing and tracking
 * - Device scanning and coordination  
 * - Filter wheel automation
 * - Intelligent autofocus
 * - Multi-target sequences
 * - Image quality analysis
 * - Environmental monitoring
 * - Safety systems
 * 
 * All tasks follow modern C++ design principles with proper error handling,
 * parameter validation, comprehensive logging, and professional documentation.
 * The system provides complete coverage of the AtomCamera interface and beyond
 * for professional astrophotography applications.
 */

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_ALL_TASKS_HPP
