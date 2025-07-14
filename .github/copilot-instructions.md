# Lithium Next - AI Agent Instructions

## Project Overview
Lithium-Next is a modular C++20 astrophotography control software with a task-based architecture. It provides comprehensive control over astronomical devices (cameras, telescopes, focusers, etc.) through multiple backends (Mock, INDI, ASCOM, Native) and features an intelligent task sequencing system.

## Architecture

### Core Components
- **Device System**: Unified interface for controlling astronomical devices 
- **Task System**: Flexible system for creating and executing astronomical workflows
- **Sequencer**: Manages and executes tasks in sequence with dependencies
- **Config System**: Handles serialization/deserialization of configurations and sequences

### Key Directories
- `/src/device/`: Device control implementations (camera, telescope, etc.)
- `/src/task/`: Task system and implementations
- `/libs/atom/`: Core utility library
- `/modules/`: Self-contained feature modules
- `/example/`: Usage examples

## Development Guidelines

### Build System
```bash
# Standard build
mkdir build && cd build
cmake ..
make

# Optimized build with Clang 
mkdir build-clang && cd build-clang
cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release ..
make
```

### Device System Patterns
1. Use the `DeviceFactory` to create device instances:
   ```cpp
   auto factory = DeviceFactory::getInstance();
   auto camera = factory.createCamera("MainCamera", DeviceBackend::MOCK);
   ```

2. Device lifecycle pattern:
   ```cpp
   device->initialize();  // Initialize driver/backend
   device->connect();     // Connect to physical device
   // Use device...
   device->disconnect();  // Close connection
   device->destroy();     // Clean up resources
   ```

### Task System Patterns
1. Create and configure a sequence:
   ```cpp
   ExposureSequence sequence;
   
   // Set callbacks
   sequence.setOnSequenceStart([]() { /* ... */ });
   sequence.setOnTargetEnd([](const std::string& name, TargetStatus status) { /* ... */ });
   ```

2. Create targets and tasks:
   ```cpp
   auto target = std::make_unique<Target>("MainTarget", std::chrono::seconds(5), 3);
   
   // Create and add task
   auto task = std::make_unique<Task>("CustomTask", [](const json& params) {
       // Task implementation
   });
   target->addTask(std::move(task));
   
   // Add target to sequence
   sequence.addTarget(std::move(target));
   ```

3. Execute sequence:
   ```cpp
   sequence.executeAll();
   ```

### C++20 Features
- Use concepts for type constraints
- Use std::filesystem for file operations
- Use std::format for string formatting
- Prefer std::span over pointer+size
- Use std::jthread for automatically joining threads

## Testing
- Unit tests are in `/tests/`
- Example code in `/example/` demonstrates intended usage patterns
- Use mock devices for testing without hardware

## Document Search
When searching for documentation related to cpp, spldog, curl, tinyxml2, nlohmann/json, etc., always use Context7 to obtain the latest version-specific documentation. When searching for xxx documentation, **search for the stable version of xxx documentation**. In your query, explicitly include `use context7` and specify the need for the stable version of xxx documentation. For example: use context7 to search for the latest version of C++ documentation on vectors.

## Project-Specific Conventions

### Error Handling
- Use structured exceptions from `exception/` for domain-specific errors
- Return false/nullptr for failures in device operations, don't throw
- Use std::optional for operations that might not return a value

### Memory Management
- Use smart pointers (std::shared_ptr, std::unique_ptr) for ownership
- Avoid raw pointers except for non-owning references
- Use RAII for resource management

### Task Implementation Pattern
```cpp
class CustomTask : public Task {
public:
    static auto taskName() -> std::string { return "CustomTask"; }
    
    void execute(const json& params) override {
        // Extract parameters with validation
        double exposure = params.value("exposure", 1.0);
        
        // Implement task logic
        // ...
        
        // Signal completion
        notifyCompletion(true);
    }
};
```

### Integration Points
- Device drivers implement interfaces from `/libs/atom/device/`
- Task implementations extend the Task class
- Sequence serialization uses the ConfigSerializer

## MCP Interactive Feedback Rules

1. During any process, task, or conversation, whether asking, responding, or completing stage tasks, must call MCP mcp-feedback-enhanced.
2. When receiving user feedback, if feedback content is not empty, must call MCP mcp-feedback-enhanced again and adjust behavior based on feedback.
3. Only when user explicitly indicates "end" or "no more interaction needed" can you stop calling MCP mcp-feedback-enhanced, then the process is complete.
4. Unless receiving end command, all steps must repeatedly call MCP mcp-feedback-enhanced.
5. Before completing the task, use the MCP mcp-feedback-enhanced to ask the user for feedback.
