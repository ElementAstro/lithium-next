# Camera Task System Documentation

## Overview

The optimized camera task system provides comprehensive control over astrophotography cameras through a modular, well-structured task framework. This system aligns perfectly with the AtomCamera interface and provides modern C++ implementations for all camera functionality.

## Architecture

### Task Categories

#### 1. Core Camera Tasks

- **Basic Exposure Tasks** (`basic_exposure.hpp/.cpp`)
  - `TakeExposureTask` - Single exposure with full parameter control
  - `TakeManyExposureTask` - Sequence of exposures with delay support
  - `SubframeExposureTask` - ROI exposures for targeted imaging
  - `CameraSettingsTask` - General camera configuration
  - `CameraPreviewTask` - Quick preview exposures

- **Calibration Tasks** (`calibration_tasks.hpp/.cpp`)
  - `AutoCalibrationTask` - Automated calibration frame acquisition
  - `ThermalCycleTask` - Temperature-dependent dark frame acquisition
  - `FlatFieldSequenceTask` - Automated flat field acquisition

#### 2. Advanced Camera Control

- **Video Tasks** (`video_tasks.hpp/.cpp`)
  - `StartVideoTask` - Initialize video streaming
  - `StopVideoTask` - Terminate video streaming
  - `GetVideoFrameTask` - Retrieve individual frames
  - `RecordVideoTask` - Record video sessions with timing control
  - `VideoStreamMonitorTask` - Monitor stream performance metrics

- **Temperature Tasks** (`temperature_tasks.hpp/.cpp`)
  - `CoolingControlTask` - Manage camera cooling system
  - `TemperatureMonitorTask` - Continuous temperature monitoring
  - `TemperatureStabilizationTask` - Wait for thermal equilibrium
  - `CoolingOptimizationTask` - Optimize cooling efficiency
  - `TemperatureAlertTask` - Temperature threshold monitoring

- **Frame Management Tasks** (`frame_tasks.hpp/.cpp`)
  - `FrameConfigTask` - Configure resolution, binning, file formats
  - `ROIConfigTask` - Set up Region of Interest for subframe imaging
  - `BinningConfigTask` - Control pixel binning for speed/sensitivity
  - `FrameInfoTask` - Query current frame configuration
  - `UploadModeTask` - Configure image upload destinations
  - `FrameStatsTask` - Analyze captured frame statistics

- **Parameter Control Tasks** (`parameter_tasks.hpp/.cpp`)
  - `GainControlTask` - Control camera gain/sensitivity
  - `OffsetControlTask` - Control offset/pedestal levels
  - `ISOControlTask` - Control ISO sensitivity (DSLR cameras)
  - `AutoParameterTask` - Automatic parameter optimization
  - `ParameterProfileTask` - Save/load parameter profiles
  - `ParameterStatusTask` - Query current parameter values

## Design Principles

### Modern C++ Features

- **C++20 Standard**: Utilizes latest language features
- **Concepts**: Type safety and template constraints
- **Smart Pointers**: Automatic memory management
- **RAII**: Resource acquisition is initialization
- **Move Semantics**: Efficient object handling

### Error Handling

- **Exception Safety**: Strong exception safety guarantees
- **Comprehensive Validation**: Parameter validation with detailed error messages
- **Error Propagation**: Proper error context preservation
- **Graceful Degradation**: Fallback mechanisms where appropriate

### Logging and Monitoring

- **Structured Logging**: JSON-formatted logs for easy parsing
- **Multiple Log Levels**: DEBUG, INFO, WARN, ERROR categories
- **Performance Metrics**: Execution time and memory usage tracking
- **Real-time Status**: Live status updates during task execution

## Usage Examples

### Complete Imaging Session

```cpp
// Create a complete deep-sky imaging session
auto session = lithium::task::examples::ImagingSessionExample::createFullImagingSequence();

// Execute the sequence
bool success = lithium::task::examples::executeTaskSequence(session);
```

### Video Streaming Session

```cpp
// Set up video streaming for planetary observation
auto videoSession = lithium::task::examples::VideoStreamingExample::createVideoStreamingSequence();

// Execute video tasks
bool success = lithium::task::examples::executeTaskSequence(videoSession);
```

### ROI Imaging for Planets

```cpp
// Configure high-speed ROI imaging
auto roiSession = lithium::task::examples::ROIImagingExample::createROIImagingSequence();

// Execute ROI imaging sequence
bool success = lithium::task::examples::executeTaskSequence(roiSession);
```

### Parameter Profile Management

```cpp
// Manage different camera profiles
auto profileSession = lithium::task::examples::ProfileManagementExample::createProfileManagementSequence();

// Execute profile management
bool success = lithium::task::examples::executeTaskSequence(profileSession);
```

## Integration with AtomCamera Interface

### Direct Mapping

Each task category directly maps to AtomCamera interface methods:

| AtomCamera Method  | Corresponding Tasks                        |
| ------------------ | ------------------------------------------ |
| `startExposure()`  | `TakeExposureTask`, `TakeManyExposureTask` |
| `startVideo()`     | `StartVideoTask`                           |
| `stopVideo()`      | `StopVideoTask`                            |
| `getVideoFrame()`  | `GetVideoFrameTask`                        |
| `startCooling()`   | `CoolingControlTask`                       |
| `getTemperature()` | `TemperatureMonitorTask`                   |
| `setGain()`        | `GainControlTask`                          |
| `setOffset()`      | `OffsetControlTask`                        |
| `setISO()`         | `ISOControlTask`                           |
| `setResolution()`  | `FrameConfigTask`, `ROIConfigTask`         |
| `setBinning()`     | `BinningConfigTask`                        |
| `setFrameType()`   | `FrameConfigTask`                          |
| `setUploadMode()`  | `UploadModeTask`                           |

### Enhanced Functionality

The task system provides enhanced functionality beyond the basic interface:

- **Automated Sequences**: Complex multi-step operations
- **Parameter Optimization**: Automatic parameter tuning
- **Profile Management**: Save/load different configurations
- **Monitoring and Alerts**: Real-time system monitoring
- **Statistics and Analysis**: Frame quality analysis

## Configuration and Customization

### Parameter Schemas

Each task includes comprehensive JSON schemas for parameter validation:

```json
{
  "type": "object",
  "properties": {
    "exposure": {
      "type": "number",
      "minimum": 0,
      "maximum": 7200,
      "description": "Exposure time in seconds"
    },
    "gain": {
      "type": "integer",
      "minimum": 0,
      "maximum": 1000,
      "description": "Camera gain value"
    }
  },
  "required": ["exposure"]
}
```

### Task Dependencies

Tasks can declare dependencies to ensure proper execution order:

```cpp
.dependencies = {"CoolingControl", "ParameterStatus"}
```

### Custom Task Creation

Create custom tasks by inheriting from the base Task class:

```cpp
class CustomImagingTask : public Task {
public:
    static auto taskName() -> std::string { return "CustomImaging"; }
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
};
```

## Performance Considerations

### Memory Management

- Smart pointers for automatic cleanup
- RAII for resource management
- Move semantics for efficient transfers
- Minimal copying of large objects

### Execution Efficiency

- Lazy initialization of resources
- Background monitoring tasks
- Efficient parameter validation
- Optimized mock implementations for testing

### Scalability

- Modular task design
- Thread-safe implementations
- Configurable timeout handling
- Resource pooling where appropriate

## Testing and Validation

### Mock Implementations

Complete mock camera implementations for testing:

- **MockCameraDevice**: Video streaming simulation
- **MockTemperatureController**: Thermal system simulation
- **MockFrameController**: Frame management simulation
- **MockParameterController**: Parameter control simulation

### Parameter Validation

Comprehensive validation for all parameters:

- Range checking for numeric values
- Enum validation for string parameters
- Cross-parameter validation
- Required parameter enforcement

### Error Scenarios

Testing of various error conditions:

- Hardware communication failures
- Parameter out-of-range errors
- Timeout conditions
- Resource unavailability

## Future Enhancements

### Planned Features

- **AI-Powered Optimization**: Machine learning for parameter tuning
- **Advanced Scheduling**: Complex time-based task scheduling
- **Cloud Integration**: Remote monitoring and control
- **Advanced Analytics**: Deep frame analysis and quality metrics

### Extension Points

- **Custom Parameter Types**: Support for complex parameter structures
- **Plugin Architecture**: Third-party task extensions
- **Hardware Abstraction**: Support for multiple camera types
- **Protocol Support**: INDI, ASCOM, and other protocols

## Conclusion

The optimized camera task system provides a comprehensive, modern, and extensible framework for astrophotography camera control. It combines the power of modern C++ with practical astrophotography requirements to create a professional-grade control system.

The system's modular design, comprehensive error handling, and extensive testing capabilities make it suitable for both amateur and professional astrophotography applications.
