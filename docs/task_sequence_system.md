# Lithium Task Sequence System

## Overview

The Lithium Task Sequence System is a comprehensive framework for managing complex astrophotography operations through an intelligent task control system. It allows users to define, schedule, and execute sequences of tasks with dependencies, retries, timeouts, and extensive error handling.

## Key Components

### SequenceManager

The `SequenceManager` class serves as the central integration point for the task sequence system. It provides a high-level interface for:

- Creating, loading, and saving sequences
- Validating sequence files and JSON
- Executing sequences with proper error handling
- Managing templates for common sequence patterns
- Monitoring execution and collecting results

### TaskGenerator

The `TaskGenerator` class processes macros and templates to generate task sequences. Features include:

- Macro expansion within sequence definitions
- Script template management
- JSON schema validation
- Format conversion between different serialization formats

### ExposureSequence

The `ExposureSequence` class manages and executes a sequence of targets with tasks:

- Target dependency management
- Concurrent execution control
- Scheduling strategies
- Recovery strategies for error handling
- Progress tracking and callbacks

### Target

The `Target` class represents a unit of work with a collection of tasks:

- Task grouping and dependency management
- Retry logic with cooldown periods
- Callbacks for monitoring execution
- Parameter customization for tasks

### Task

The `Task` class represents an individual operation:

- Timeout management
- Priority settings
- Error handling and classification
- Resource usage tracking
- Parameter validation

## Exception Handling

The system includes a comprehensive exception hierarchy:

- `TaskException`: Base class for all task-related exceptions
- `TaskTimeoutException`: For task timeout errors
- `TaskParameterException`: For invalid parameters
- `TaskDependencyException`: For dependency resolution errors
- `TaskExecutionException`: For runtime execution errors

## Usage Examples

### Basic Sequence Creation

```cpp
// Initialize sequence manager
auto manager = SequenceManager::createShared();

// Create a sequence
auto sequence = manager->createSequence("SimpleSequence");

// Create and add a target
auto target = std::make_unique<Target>("MyTarget", std::chrono::seconds(5), 2);

// Add a task to the target
auto task = std::make_unique<Task>(
    "Exposure",
    "TakeExposure",
    [](const json& params) {
        // Task implementation
    });

// Set task parameters
task->setTimeout(std::chrono::seconds(30));
target->addTask(std::move(task));

// Add target to sequence
sequence->addTarget(std::move(target));

// Execute the sequence
auto result = manager->executeSequence(sequence, false);
```

### Using Templates

```cpp
// Create parameters for template
json params = {
    {"targetName", "M42"},
    {"exposureTime", 30.0},
    {"frameType", "light"},
    {"binning", 1},
    {"gain", 100},
    {"offset", 10}
};

// Create sequence from template
auto sequence = manager->createSequenceFromTemplate("BasicExposure", params);

// Execute sequence
manager->executeSequence(sequence, true);
```

### Error Handling

```cpp
try {
    auto sequence = manager->loadSequenceFromFile("my_sequence.json");
    manager->executeSequence(sequence, false);
} catch (const SequenceException& e) {
    spdlog::error("Sequence error: {}", e.what());
} catch (const TaskException& e) {
    spdlog::error("Task error: {} ({})", e.what(), e.severityToString());
} catch (const std::exception& e) {
    spdlog::error("General error: {}", e.what());
}
```

## Integration with Other Systems

The task sequence system integrates with:

- Database for sequence persistence
- File system for template and sequence storage
- Camera control modules
- Image processing pipeline
- Telescope control system

## Performance Considerations

- Configurable concurrency for parallel task execution
- Resource monitoring for memory and CPU usage
- Optimized task scheduling based on dependencies and priorities
- Efficient error recovery with multiple strategies

## Best Practices

- Define clear dependencies between tasks
- Use templates for common operations
- Set appropriate timeouts for all tasks
- Implement robust error handling with retries
- Monitor resource usage for long-running sequences
- Use appropriate scheduling strategies based on workload

## Future Enhancements

- Distributed task execution across multiple nodes
- Real-time monitoring and visualization
- Machine learning for optimizing task scheduling
- Extended template library for common astrophotography scenarios
