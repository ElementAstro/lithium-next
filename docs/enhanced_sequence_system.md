# Enhanced Sequence System Documentation

## Overview

The Enhanced Sequence System provides a comprehensive framework for astronomical observation automation with advanced task management, templating, and execution strategies. This system is designed to handle complex observation workflows with robust error handling, dependency management, and optimization capabilities.

## Key Components

### 1. Enhanced Sequencer (`enhanced_sequencer.hpp/cpp`)

The core orchestration engine that manages task execution with multiple strategies:

#### Execution Strategies
- **Sequential**: Tasks execute one after another in order
- **Parallel**: Independent tasks execute simultaneously with configurable concurrency limits
- **Adaptive**: Dynamic strategy selection based on system resources and task characteristics
- **Priority**: Tasks execute based on priority levels with preemption support

#### Key Features
- **Dependency Management**: Automatic resolution of task dependencies
- **Resource Monitoring**: CPU, memory, and device usage tracking
- **Script Integration**: Support for Python, JavaScript, and shell script execution
- **Performance Optimization**: Automatic load balancing and resource allocation
- **Real-time Monitoring**: Live metrics and progress tracking

#### Usage Example
```cpp
#include "task/custom/enhanced_sequencer.hpp"

auto sequencer = std::make_unique<EnhancedSequencer>(taskManager);
sequencer->setExecutionStrategy(ExecutionStrategy::Parallel);
sequencer->setConcurrencyLimit(4);
sequencer->enableMonitoring(true);

json sequence = json::array();
sequence.push_back({{"task_id", "imaging_task_1"}});
sequence.push_back({{"task_id", "calibration_task_1"}});

sequencer->executeSequence(sequence);
```

### 2. Task Manager (`task_manager.hpp/cpp`)

Advanced task lifecycle management with parallel execution support:

#### Features
- **Parallel Task Execution**: Concurrent execution with dependency resolution
- **Task Status Tracking**: Real-time status monitoring and history
- **Cancellation Support**: Graceful task cancellation with cleanup
- **Batch Operations**: Efficient batch task management
- **Error Recovery**: Automatic retry and error handling strategies

#### Usage Example
```cpp
#include "task/custom/task_manager.hpp"

auto manager = std::make_unique<TaskManager>();

// Add tasks with dependencies
auto taskId1 = manager->addTask(std::move(initTask));
auto taskId2 = manager->addTask(std::move(captureTask));
manager->addDependency(taskId2, taskId1);

// Execute tasks
manager->executeTasksParallel({taskId1, taskId2});
```

### 3. Task Templates (`task_templates.hpp/cpp`)

Pre-configured task templates for common astronomical operations:

#### Available Templates
- **Imaging**: Target imaging with configurable parameters
- **Calibration**: Dark, flat, and bias frame acquisition
- **Focus**: Automatic focusing with various algorithms
- **Plate Solving**: Astrometric calibration and pointing correction
- **Device Setup**: Equipment initialization and configuration
- **Safety Checks**: System safety validation
- **Script Execution**: Custom script integration
- **Filter Changes**: Automated filter wheel operations
- **Guiding Setup**: Autoguiding configuration
- **Complete Observation**: End-to-end observation workflows

#### Usage Example
```cpp
#include "task/custom/task_templates.hpp"

auto templates = std::make_unique<TaskTemplateManager>();

json params = {
    {"target", "M31"},
    {"exposure_time", 300},
    {"filter", "Ha"},
    {"count", 10}
};

auto imagingTask = templates->createTask("imaging", "m31_imaging", params);
```

### 4. Task Factory System (`factory.hpp`)

Dynamic task creation with registration system:

#### Registered Task Types
- **script_task**: Python, JavaScript, and shell script execution
- **device_task**: Astronomical device control and management
- **config_task**: Configuration management and persistence

#### Usage Example
```cpp
#include "task/custom/factory.hpp"

auto& factory = TaskFactory::getInstance();

auto scriptTask = factory.createTask("script_task", "my_script", {
    {"script_path", "/path/to/script.py"},
    {"script_type", "python"},
    {"timeout", 5000}
});
```

## Task Types

### Script Task (`script_task.hpp/cpp`)

Executes external scripts with full parameter passing and output capture:

#### Supported Script Types
- **Python**: `.py` files with virtual environment support
- **JavaScript**: `.js` files with Node.js execution
- **Shell**: Shell scripts with environment variable support

#### Parameters
- `script_path`: Path to the script file
- `script_type`: Type of script (python/javascript/shell)
- `timeout`: Execution timeout in milliseconds
- `capture_output`: Whether to capture script output
- `environment`: Environment variables to pass
- `arguments`: Command-line arguments

### Device Task (`device_task.hpp/cpp`)

Controls astronomical devices with comprehensive device management:

#### Operations
- **connect**: Establish device connection
- **scan**: Discover available devices
- **initialize**: Initialize device with configuration
- **configure**: Apply device-specific settings
- **test**: Run device diagnostics

#### Parameters
- `operation`: Device operation to perform
- `deviceName`: Target device name
- `deviceType`: Device type (camera, mount, filterwheel, etc.)
- `timeout`: Operation timeout
- `retryCount`: Number of retry attempts
- `config`: Device-specific configuration

### Config Task (`config_task.hpp/cpp`)

Manages system configuration with validation and backup:

#### Operations
- **set**: Set configuration values
- **get**: Retrieve configuration values
- **delete**: Remove configuration keys
- **load**: Load configuration from file
- **save**: Save configuration to file
- **merge**: Merge configuration data
- **list**: List configuration keys

#### Parameters
- `operation`: Configuration operation
- `key_path`: Configuration key path (dot notation)
- `value`: Configuration value to set
- `file_path`: File path for load/save operations
- `merge_data`: Data to merge
- `backup`: Create backup before changes
- `validate`: Validate configuration after changes

## Utility Functions

### Common Tasks (`TaskUtils::CommonTasks`)

Pre-configured parameter generators for common operations:

```cpp
// Generate imaging parameters
auto imagingParams = CommonTasks::generateImagingParameters(
    "M31", "Ha", 300, 10, 1, 1.0, true, -10.0
);

// Generate calibration parameters
auto calibrationParams = CommonTasks::generateCalibrationParameters(
    "dark", 300, 10, 1, -10.0
);

// Generate focus parameters
auto focusParams = CommonTasks::generateFocusParameters(
    "star", 5.0, 50, 5, 2.0
);
```

### Sequence Patterns (`SequencePatterns`)

Optimization and pattern generation for complex sequences:

```cpp
// Optimize sequence for minimal execution time
auto optimized = SequencePatterns::optimizeSequence(tasks, {
    .minimizeTime = true,
    .balanceLoad = true,
    .respectPriority = true,
    .maxParallelTasks = 4
});

// Create optimal execution pattern
auto pattern = SequencePatterns::createOptimalPattern(tasks, "imaging");
```

### Task Validation (`TaskValidation`)

Comprehensive parameter validation:

```cpp
// Validate task parameters
bool isValid = TaskValidation::validateTaskParameters(task, params);

// Validate sequence integrity
bool sequenceValid = TaskValidation::validateSequence(sequence);

// Check resource requirements
bool resourcesOk = TaskValidation::checkResourceRequirements(tasks);
```

## Configuration

### Sequencer Configuration

```json
{
    "execution_strategy": "parallel",
    "concurrency_limit": 4,
    "monitoring_enabled": true,
    "optimization": {
        "minimize_time": true,
        "balance_load": true,
        "respect_priority": true
    },
    "retry_policy": {
        "max_retries": 3,
        "retry_delay": 1000,
        "exponential_backoff": true
    }
}
```

### Task Manager Configuration

```json
{
    "max_parallel_tasks": 8,
    "task_timeout": 300000,
    "cleanup_interval": 60000,
    "history_retention": 1000,
    "error_handling": {
        "auto_retry": true,
        "max_retries": 2,
        "isolation": true
    }
}
```

## Error Handling

### Error Types
- **TaskError**: Task execution failures
- **ValidationError**: Parameter validation failures
- **ResourceError**: Resource allocation failures
- **TimeoutError**: Task timeout exceeded
- **DependencyError**: Dependency resolution failures

### Recovery Strategies
- **Automatic Retry**: Configurable retry attempts with exponential backoff
- **Graceful Degradation**: Continue execution with failed tasks isolated
- **Rollback**: Revert changes on critical failures
- **Alternative Execution**: Switch to alternative execution paths

## Performance Optimization

### Execution Strategies
- **Load Balancing**: Distribute tasks across available resources
- **Dependency Optimization**: Minimize wait times through intelligent scheduling
- **Resource Pooling**: Efficient resource allocation and reuse
- **Adaptive Scheduling**: Dynamic strategy adjustment based on performance metrics

### Monitoring and Metrics
- **Real-time Metrics**: Task execution statistics and performance data
- **Resource Usage**: CPU, memory, and device utilization tracking
- **Bottleneck Detection**: Automatic identification of performance bottlenecks
- **Optimization Suggestions**: Recommendations for performance improvements

## Integration Examples

### Complete Observation Workflow

```cpp
#include "task/custom/enhanced_sequencer.hpp"
#include "task/custom/task_templates.hpp"

// Initialize system
auto manager = std::make_unique<TaskManager>();
auto sequencer = std::make_unique<EnhancedSequencer>(manager.get());
auto templates = std::make_unique<TaskTemplateManager>();

// Configure sequencer
sequencer->setExecutionStrategy(ExecutionStrategy::Adaptive);
sequencer->enableMonitoring(true);

// Create observation sequence
json sequence = json::array();

// 1. Safety checks
auto safetyTask = templates->createTask("safety_check", "initial_safety", {});
auto safetyId = manager->addTask(std::move(safetyTask));
sequence.push_back({{"task_id", safetyId}});

// 2. Device setup
auto setupTask = templates->createTask("device_setup", "equipment_init", {
    {"devices", json::array({"camera", "mount", "filterwheel"})}
});
auto setupId = manager->addTask(std::move(setupTask));
manager->addDependency(setupId, safetyId);
sequence.push_back({{"task_id", setupId}});

// 3. Plate solving
auto platesolveTask = templates->createTask("platesolve", "initial_solve", {
    {"target", "M31"},
    {"exposure_time", 5}
});
auto platesolveId = manager->addTask(std::move(platesolveTask));
manager->addDependency(platesolveId, setupId);
sequence.push_back({{"task_id", platesolveId}});

// 4. Focus
auto focusTask = templates->createTask("focus", "auto_focus", {
    {"focus_method", "star"},
    {"step_size", 5.0}
});
auto focusId = manager->addTask(std::move(focusTask));
manager->addDependency(focusId, platesolveId);
sequence.push_back({{"task_id", focusId}});

// 5. Imaging
for (const auto& filter : {"Ha", "OIII", "SII"}) {
    auto imagingTask = templates->createTask("imaging",
        std::string("imaging_") + filter, {
        {"target", "M31"},
        {"filter", filter},
        {"exposure_time", 300},
        {"count", 10}
    });
    auto imagingId = manager->addTask(std::move(imagingTask));
    manager->addDependency(imagingId, focusId);
    sequence.push_back({{"task_id", imagingId}});
}

// Execute sequence
sequencer->executeSequence(sequence);

// Monitor progress
while (sequencer->isRunning()) {
    auto metrics = sequencer->getMetrics();
    std::cout << "Progress: " << metrics["progress_percentage"] << "%" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
}
```

## Best Practices

1. **Task Design**: Keep tasks focused and atomic for better error handling and reusability
2. **Dependency Management**: Minimize dependencies to maximize parallelization opportunities
3. **Resource Management**: Monitor resource usage and set appropriate concurrency limits
4. **Error Handling**: Implement comprehensive error handling with meaningful error messages
5. **Testing**: Use the provided test framework to validate sequences before production use
6. **Performance**: Profile sequences and optimize based on actual performance metrics
7. **Documentation**: Document custom tasks and sequences for team collaboration

## Troubleshooting

### Common Issues

1. **Task Timeouts**: Increase timeout values or optimize task implementation
2. **Dependency Deadlocks**: Review dependency chains for circular dependencies
3. **Resource Exhaustion**: Reduce concurrency limits or optimize resource usage
4. **Script Failures**: Check script paths, permissions, and environment setup
5. **Device Connection Issues**: Verify device availability and connection parameters

### Debug Features

- **Verbose Logging**: Enable detailed logging for troubleshooting
- **Task Profiling**: Monitor individual task performance
- **Dependency Visualization**: Generate dependency graphs for complex sequences
- **Resource Monitoring**: Track resource usage patterns
- **Error Analysis**: Detailed error reporting with stack traces

## Future Enhancements

- **Machine Learning**: Predictive scheduling and optimization
- **Cloud Integration**: Distributed task execution across multiple systems
- **Advanced Scripting**: Support for additional scripting languages
- **Visual Editor**: Graphical sequence design interface
- **API Extensions**: REST API for remote task management
- **Workflow Templates**: More sophisticated workflow patterns
