# Enhanced Focus Task System Documentation

## Overview

The focus task system has been significantly enhanced to better utilize the latest Task definition features and provide a comprehensive suite of focus-related operations for astronomical imaging.

## Architecture Changes

### Enhanced Task Base Class Integration

All focus tasks now fully utilize the enhanced Task class features:

- **Error Management**: Proper error type classification (InvalidParameter, DeviceError, SystemError, etc.)
- **History Tracking**: Detailed execution history with milestone logging
- **Parameter Validation**: Built-in parameter validation with detailed error reporting
- **Performance Metrics**: Execution time and memory usage tracking
- **Dependency Management**: Task dependency chains and pre/post task execution
- **Exception Handling**: Comprehensive exception callbacks and error recovery

### Task Hierarchy

```
Focus Task Suite
├── Core Focus Tasks
│   ├── AutoFocusTask - Enhanced automatic focusing with HFR measurement
│   ├── FocusSeriesTask - Multi-position focus analysis 
│   └── TemperatureFocusTask - Temperature-based focus compensation
└── Specialized Tasks
    ├── FocusValidationTask - Focus quality validation and analysis
    ├── BacklashCompensationTask - Mechanical backlash elimination
    ├── FocusCalibrationTask - Focus curve calibration and mapping
    ├── StarDetectionTask - Star analysis for focus optimization
    └── FocusMonitoringTask - Continuous focus drift monitoring
```

## Enhanced Task Features

### 1. AutoFocusTask v2.0

**Enhancements:**
- Comprehensive parameter validation using Task base class
- Detailed execution history tracking
- Error type classification and recovery
- Performance metrics collection
- Exception callback integration

**New Capabilities:**
- Progress tracking throughout focus sweep
- Dependency management for camera calibration tasks
- Memory and CPU usage monitoring
- Detailed error reporting with context

**Example Usage:**
```cpp
auto autoFocus = AutoFocusTask::createEnhancedTask();
autoFocus->addDependency("camera_calibration_task_id");
autoFocus->setExceptionCallback([](const std::exception& e) {
    spdlog::error("AutoFocus exception: {}", e.what());
});

json params = {
    {"exposure", 1.5},
    {"step_size", 100},
    {"max_steps", 50},
    {"tolerance", 0.1}
};

autoFocus->execute(params);
```

### 2. FocusValidationTask (New)

**Purpose:** Validates focus quality by analyzing star characteristics

**Features:**
- Star count validation
- HFR (Half Flux Radius) threshold checking
- FWHM (Full Width Half Maximum) analysis
- Focus quality scoring

**Parameters:**
- `exposure_time`: Validation exposure duration
- `min_stars`: Minimum required star count
- `max_hfr`: Maximum acceptable HFR value

### 3. BacklashCompensationTask (New)

**Purpose:** Eliminates mechanical backlash in focuser systems

**Features:**
- Configurable compensation direction
- Variable backlash step amounts
- Pre-movement positioning
- Movement verification

**Parameters:**
- `backlash_steps`: Number of compensation steps
- `compensation_direction`: Direction for backlash elimination

### 4. FocusCalibrationTask (New)

**Purpose:** Calibrates focuser with known reference points

**Features:**
- Multi-point focus curve generation
- Temperature correlation mapping
- Reference position establishment
- Calibration data persistence

**Parameters:**
- `calibration_points`: Number of calibration samples

### 5. StarDetectionTask (New)

**Purpose:** Detects and analyzes stars for focus optimization

**Features:**
- Automated star detection algorithms
- Star profile analysis (HFR, FWHM, peak intensity)
- Focus quality metrics calculation
- Star field evaluation

**Parameters:**
- `detection_threshold`: Star detection sensitivity

### 6. FocusMonitoringTask (New)

**Purpose:** Continuously monitors focus quality and drift

**Features:**
- Periodic focus quality assessment
- Drift detection and alerting
- Automatic refocus triggering
- Long-term focus stability tracking

**Parameters:**
- `monitoring_interval`: Time between monitoring checks

## Workflow Examples

### 1. Comprehensive Focus Workflow

```cpp
// Create workflow with full dependency chain
auto workflow = FocusWorkflowExample::createComprehensiveFocusWorkflow();

// Execution order:
// 1. StarDetectionTask (parallel start)
// 2. FocusCalibrationTask (depends on star detection)
//    BacklashCompensationTask (parallel with calibration)
// 3. AutoFocusTask (depends on calibration + backlash)
// 4. FocusValidationTask (depends on autofocus)
// 5. FocusMonitoringTask (depends on validation)
```

### 2. Simple AutoFocus Workflow

```cpp
// Basic focusing sequence
auto workflow = FocusWorkflowExample::createSimpleAutoFocusWorkflow();

// Execution order:
// 1. BacklashCompensationTask
// 2. AutoFocusTask (depends on backlash compensation)
// 3. FocusValidationTask (depends on autofocus)
```

### 3. Temperature Compensated Workflow

```cpp
// Temperature-aware focusing
auto workflow = FocusWorkflowExample::createTemperatureCompensatedWorkflow();

// Execution order:
// 1. AutoFocusTask (initial focus)
// 2. TemperatureFocusTask (temperature compensation)
// 3. FocusMonitoringTask (continuous monitoring)
```

## Task Dependencies and Pre/Post Tasks

### Dependency Management

Tasks can now declare dependencies on other tasks:

```cpp
auto autoFocus = AutoFocusTask::createEnhancedTask();
auto validation = FocusValidationTask::createEnhancedTask();

// Validation depends on autofocus completion
validation->addDependency(autoFocus->getUUID());

// Check if dependencies are satisfied
if (validation->isDependencySatisfied()) {
    validation->execute(params);
}
```

### Pre/Post Task Execution

```cpp
auto mainTask = AutoFocusTask::createEnhancedTask();

// Add pre-task (backlash compensation)
auto preTask = std::make_unique<BacklashCompensationTask>();
mainTask->addPreTask(std::move(preTask));

// Add post-task (validation)
auto postTask = std::make_unique<FocusValidationTask>();
mainTask->addPostTask(std::move(postTask));

// Pre-tasks execute before main task
// Post-tasks execute after main task completion
```

## Error Handling and Recovery

### Error Type Classification

```cpp
task->setErrorType(TaskErrorType::InvalidParameter); // Parameter validation failed
task->setErrorType(TaskErrorType::DeviceError);      // Hardware communication error
task->setErrorType(TaskErrorType::SystemError);      // General system error
task->setErrorType(TaskErrorType::Timeout);          // Task execution timeout
```

### Exception Callbacks

```cpp
task->setExceptionCallback([](const std::exception& e) {
    // Custom error handling
    spdlog::error("Task failed: {}", e.what());
    
    // Trigger recovery procedures
    // Send notifications
    // Update system state
});
```

## Performance Monitoring

### Execution Metrics

```cpp
// After task execution
auto executionTime = task->getExecutionTime();
auto memoryUsage = task->getMemoryUsage();
auto cpuUsage = task->getCPUUsage();

spdlog::info("Task completed in {} ms, used {} bytes, {}% CPU", 
             executionTime.count(), memoryUsage, cpuUsage);
```

### History Tracking

```cpp
// During task execution
task->addHistoryEntry("Starting coarse focus sweep");
task->addHistoryEntry("Best position found: " + std::to_string(position));

// Retrieve history
auto history = task->getTaskHistory();
for (const auto& entry : history) {
    spdlog::info("History: {}", entry);
}
```

## Parameter Validation

### Built-in Validation

```cpp
// Tasks now use the base class parameter validation
if (!task->validateParams(params)) {
    auto errors = task->getParamErrors();
    for (const auto& error : errors) {
        spdlog::error("Parameter error: {}", error);
    }
}
```

### Custom Validation

Each task implements specific parameter validation:

```cpp
void AutoFocusTask::validateAutoFocusParameters(const json& params) {
    if (params.contains("exposure")) {
        double exposure = params["exposure"].get<double>();
        if (exposure <= 0 || exposure > 60) {
            THROW_INVALID_ARGUMENT("Exposure time must be between 0 and 60 seconds");
        }
    }
    // Additional validations...
}
```

## Migration from Previous Version

### Key Changes

1. **Enhanced Error Handling**: All tasks now use proper error type classification
2. **History Tracking**: Execution milestones are automatically logged
3. **Parameter Validation**: Built-in validation with detailed error reporting
4. **Dependency Management**: Tasks can declare dependencies on other tasks
5. **Performance Monitoring**: Automatic execution metrics collection

### Breaking Changes

- Task constructors now require initialization calls
- Exception handling behavior has changed
- Parameter validation is more strict
- Error reporting format has been enhanced

### Migration Steps

1. Update task instantiation to use `createEnhancedTask()` factory methods
2. Add proper error handling with exception callbacks
3. Update parameter validation to use new validation system
4. Add dependency declarations where appropriate
5. Update error handling code to use new error types

## Best Practices

### 1. Task Creation

Always use the enhanced factory methods:

```cpp
// Preferred
auto task = AutoFocusTask::createEnhancedTask();

// Avoid direct instantiation for production use
auto task = std::make_unique<AutoFocusTask>(); // Limited features
```

### 2. Error Handling

Implement comprehensive error handling:

```cpp
task->setExceptionCallback([](const std::exception& e) {
    // Log the error
    spdlog::error("Task failed: {}", e.what());
    
    // Implement recovery logic
    // Notify operators
    // Update system state
});
```

### 3. Dependency Management

Use dependencies to ensure proper execution order:

```cpp
// Ensure backlash compensation before focusing
autoFocus->addDependency(backlashTask->getUUID());

// Validate focus after completion
validation->addDependency(autoFocus->getUUID());
```

### 4. Parameter Validation

Always validate parameters before execution:

```cpp
if (!task->validateParams(params)) {
    auto errors = task->getParamErrors();
    // Handle validation errors
    return false;
}
```

## Future Enhancements

### Planned Features

1. **Machine Learning Integration**: AI-powered focus prediction
2. **Adaptive Algorithms**: Self-tuning focus parameters
3. **Multi-Camera Support**: Synchronized focusing across multiple cameras
4. **Cloud Integration**: Remote focus monitoring and control
5. **Advanced Analytics**: Focus performance trend analysis

### Extensibility

The system is designed for easy extension:

1. **Custom Focus Algorithms**: Implement new focusing methods
2. **Hardware Adapters**: Support for additional focuser types
3. **Analysis Plugins**: Custom star analysis algorithms
4. **Workflow Templates**: Pre-defined focus sequences

This enhanced focus task system provides a robust, scalable, and maintainable foundation for astronomical focusing operations with comprehensive error handling, performance monitoring, and dependency management capabilities.
