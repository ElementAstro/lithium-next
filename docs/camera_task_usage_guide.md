# Camera Task System Usage Guide

## 🚀 Quick Start Guide

### Basic Single Exposure

```cpp
#include "camera_tasks.hpp"
using namespace lithium::task::task;

// Create and execute a single exposure
auto task = std::make_unique<TakeExposureTask>("TakeExposure", nullptr);
json params = {
    {"exposure_time", 10.0},
    {"save_path", "/data/images/"},
    {"file_format", "FITS"}
};
task->execute(params);
```

### Multi-Exposure Sequence

```cpp
auto task = std::make_unique<TakeManyExposureTask>("TakeManyExposure", nullptr);
json params = {
    {"exposure_time", 300.0},
    {"count", 20},
    {"save_path", "/data/images/"},
    {"sequence_name", "M31_luminance"},
    {"delay_between", 5.0}
};
task->execute(params);
```

## 🔬 Advanced Workflows

### Complete Calibration Session

```cpp
// Dark frames
auto darkTask = std::make_unique<DarkFrameTask>("DarkFrame", nullptr);
json darkParams = {
    {"exposure_time", 300.0},
    {"count", 20},
    {"target_temperature", -10.0},
    {"save_path", "/data/calibration/darks/"}
};
darkTask->execute(darkParams);

// Bias frames
auto biasTask = std::make_unique<BiasFrameTask>("BiasFrame", nullptr);
json biasParams = {
    {"count", 50},
    {"save_path", "/data/calibration/bias/"}
};
biasTask->execute(biasParams);

// Flat frames
auto flatTask = std::make_unique<FlatFrameTask>("FlatFrame", nullptr);
json flatParams = {
    {"exposure_time", 1.0},
    {"count", 20},
    {"target_adu", 30000},
    {"save_path", "/data/calibration/flats/"}
};
flatTask->execute(flatParams);
```

### Professional Filter Sequence

```cpp
auto filterTask = std::make_unique<AutoFilterSequenceTask>("AutoFilterSequence", nullptr);
json filterParams = {
    {"filter_sequence", json::array({
        {{"filter", "Luminance"}, {"count", 30}, {"exposure", 300}},
        {{"filter", "Red"}, {"count", 15}, {"exposure", 240}},
        {{"filter", "Green"}, {"count", 15}, {"exposure", 240}},
        {{"filter", "Blue"}, {"count", 15}, {"exposure", 240}},
        {{"filter", "Ha"}, {"count", 20}, {"exposure", 900}},
        {{"filter", "OIII"}, {"count", 20}, {"exposure", 900}},
        {{"filter", "SII"}, {"count", 20}, {"exposure", 900}}
    })},
    {"auto_focus_per_filter", true},
    {"repetitions", 2}
};
filterTask->execute(filterParams);
```

## 🔭 Observatory Automation

### Complete Observatory Session

```cpp
// 1. Connect all devices
auto scanTask = std::make_unique<DeviceScanConnectTask>("DeviceScanConnect", nullptr);
json scanParams = {
    {"auto_connect", true},
    {"device_types", json::array({"Camera", "Telescope", "Focuser", "FilterWheel", "Guider"})}
};
scanTask->execute(scanParams);

// 2. Goto target
auto gotoTask = std::make_unique<TelescopeGotoImagingTask>("TelescopeGotoImaging", nullptr);
json gotoParams = {
    {"target_ra", 0.712},   // M31
    {"target_dec", 41.269},
    {"enable_tracking", true},
    {"wait_for_slew", true}
};
gotoTask->execute(gotoParams);

// 3. Intelligent autofocus
auto focusTask = std::make_unique<IntelligentAutoFocusTask>("IntelligentAutoFocus", nullptr);
json focusParams = {
    {"temperature_compensation", true},
    {"filter_offsets", true},
    {"current_filter", "Luminance"}
};
focusTask->execute(focusParams);

// 4. Advanced imaging sequence
auto sequenceTask = std::make_unique<AdvancedImagingSequenceTask>("AdvancedImagingSequence", nullptr);
json sequenceParams = {
    {"targets", json::array({
        {{"name", "M31"}, {"ra", 0.712}, {"dec", 41.269}, {"exposure_count", 50}, {"exposure_time", 300}},
        {{"name", "M42"}, {"ra", 5.588}, {"dec", -5.389}, {"exposure_count", 30}, {"exposure_time", 180}}
    })},
    {"adaptive_scheduling", true},
    {"quality_optimization", true},
    {"max_session_time", 480}
};
sequenceTask->execute(sequenceParams);

// 5. Safe shutdown
auto shutdownTask = std::make_unique<CoordinatedShutdownTask>("CoordinatedShutdown", nullptr);
json shutdownParams = {
    {"park_telescope", true},
    {"stop_cooling", true},
    {"disconnect_devices", true}
};
shutdownTask->execute(shutdownParams);
```

## 🌡️ Temperature Management

### Cooling Control

```cpp
auto coolingTask = std::make_unique<CoolingControlTask>("CoolingControl", nullptr);
json coolingParams = {
    {"enable", true},
    {"target_temperature", -10.0},
    {"cooling_power", 80.0},
    {"auto_regulate", true}
};
coolingTask->execute(coolingParams);

// Monitor temperature
auto monitorTask = std::make_unique<TemperatureMonitorTask>("TemperatureMonitor", nullptr);
json monitorParams = {
    {"duration", 300},      // 5 minutes
    {"interval", 10},       // Every 10 seconds
    {"log_to_file", true},
    {"alert_threshold", 2.0}
};
monitorTask->execute(monitorParams);
```

## 🎥 Video Streaming

### Live Streaming Setup

```cpp
auto videoTask = std::make_unique<StartVideoTask>("StartVideo", nullptr);
json videoParams = {
    {"format", "H.264"},
    {"resolution", "1920x1080"},
    {"fps", 30},
    {"quality", "high"},
    {"enable_audio", false}
};
videoTask->execute(videoParams);

// Record video session
auto recordTask = std::make_unique<RecordVideoTask>("RecordVideo", nullptr);
json recordParams = {
    {"duration", 300},      // 5 minutes
    {"save_path", "/data/videos/"},
    {"filename", "planetary_session"},
    {"compression", "medium"}
};
recordTask->execute(recordParams);
```

## 🔍 Image Analysis

### Quality Analysis

```cpp
auto analysisTask = std::make_unique<ImageQualityAnalysisTask>("ImageQualityAnalysis", nullptr);
json analysisParams = {
    {"images", json::array({
        "/data/images/M31_001.fits",
        "/data/images/M31_002.fits",
        "/data/images/M31_003.fits"
    })},
    {"detailed_analysis", true},
    {"generate_report", true}
};
analysisTask->execute(analysisParams);
```

### Adaptive Parameter Optimization

```cpp
auto optimizeTask = std::make_unique<AdaptiveExposureOptimizationTask>("AdaptiveExposureOptimization", nullptr);
json optimizeParams = {
    {"target_type", "deepsky"},
    {"current_seeing", 2.8},
    {"adapt_to_conditions", true}
};
optimizeTask->execute(optimizeParams);
```

## 🛡️ Safety and Monitoring

### Environment Monitoring

```cpp
auto envTask = std::make_unique<EnvironmentMonitorTask>("EnvironmentMonitor", nullptr);
json envParams = {
    {"duration", 3600},     // 1 hour
    {"interval", 60},       // Every minute
    {"max_wind_speed", 8.0},
    {"max_humidity", 85.0}
};
envTask->execute(envParams);
```

### Device Health Monitoring

```cpp
auto healthTask = std::make_unique<DeviceHealthMonitorTask>("DeviceHealthMonitor", nullptr);
json healthParams = {
    {"duration", 7200},     // 2 hours
    {"interval", 30},       // Every 30 seconds
    {"alert_on_failure", true}
};
healthTask->execute(healthParams);
```

## ⚙️ Parameter Control

### Comprehensive Parameter Setup

```cpp
// Gain control
auto gainTask = std::make_unique<GainControlTask>("GainControl", nullptr);
json gainParams = {
    {"gain", 100},
    {"auto_gain", false},
    {"save_profile", true},
    {"profile_name", "deepsky_standard"}
};
gainTask->execute(gainParams);

// Offset control
auto offsetTask = std::make_unique<OffsetControlTask>("OffsetControl", nullptr);
json offsetParams = {
    {"offset", 10},
    {"auto_adjust", true}
};
offsetTask->execute(offsetParams);
```

## 🎯 Error Handling Best Practices

```cpp
try {
    auto task = std::make_unique<TakeExposureTask>("TakeExposure", nullptr);
    json params = {{"exposure_time", 10.0}};
    task->execute(params);

} catch (const atom::error::InvalidArgument& e) {
    std::cerr << "Parameter error: " << e.what() << std::endl;

} catch (const atom::error::RuntimeError& e) {
    std::cerr << "Runtime error: " << e.what() << std::endl;

} catch (const std::exception& e) {
    std::cerr << "Unexpected error: " << e.what() << std::endl;
}
```

## 📊 Task Status and Monitoring

```cpp
// Check task status
if (task->getStatus() == TaskStatus::COMPLETED) {
    std::cout << "Task completed successfully!" << std::endl;
} else if (task->getStatus() == TaskStatus::FAILED) {
    std::cout << "Task failed: " << task->getErrorMessage() << std::endl;
}

// Get task progress
auto progress = task->getProgress();
std::cout << "Progress: " << progress << "%" << std::endl;
```

## 🔧 Custom Task Configuration

```cpp
// Create enhanced task with validation
auto enhancedTask = TakeExposureTask::createEnhancedTask();
enhancedTask->setRetryCount(3);
enhancedTask->setTimeout(std::chrono::minutes(5));
enhancedTask->setErrorType(TaskErrorType::CameraError);
```

This guide provides comprehensive examples for using all major camera task functionality. The system is designed to be both simple for basic operations and powerful for complex professional workflows.
