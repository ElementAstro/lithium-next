# Component-Based INDI Camera Architecture

## Overview

The INDI camera implementation has been refactored from a monolithic class into a modular, component-based architecture. This design improves code maintainability, testability, and extensibility while preserving the original public API for backward compatibility.

## Architecture

### Core Components

1. **INDICameraCore** (`core/`)
   - Central hub for INDI device communication
   - Inherits from INDI::BaseClient
   - Manages device connection and property distribution
   - Component registration and lifecycle management

2. **ExposureController** (`exposure/`)
   - Handles all exposure-related operations
   - Exposure timing and progress tracking
   - Image capture and download management
   - Exposure statistics and history

3. **VideoController** (`video/`)
   - Video streaming management
   - Video recording functionality
   - Frame rate monitoring and statistics
   - Video format handling

4. **TemperatureController** (`temperature/`)
   - Camera cooling system control
   - Temperature monitoring and regulation
   - Cooling power management
   - Temperature-related property handling

5. **HardwareController** (`hardware/`)
   - Gain and offset control
   - Frame settings (resolution, binning)
   - Shutter and fan control
   - Hardware property management

6. **ImageProcessor** (`image/`)
   - Image format handling and conversion
   - Image quality analysis
   - Image compression and processing
   - Image statistics calculation

7. **SequenceManager** (`sequence/`)
   - Automated image sequences
   - Multi-frame capture coordination
   - Sequence progress tracking
   - Inter-frame timing management

8. **PropertyHandler** (`properties/`)
   - Centralized INDI property management
   - Property routing to appropriate components
   - Property watching and monitoring
   - Property validation and utilities

### Main Camera Class

**INDICamera** (`indi_camera.hpp/cpp`)
- Aggregates all components
- Maintains the original AtomCamera API
- Delegates calls to appropriate components
- Provides component access for advanced usage

## Benefits

### 1. **Modularity**
- Each component has a single responsibility
- Components can be developed and tested independently
- Easier to understand and maintain individual features

### 2. **Extensibility**
- New components can be added easily
- Existing components can be enhanced without affecting others
- Plugin-like architecture for future features

### 3. **Testability**
- Components can be unit tested in isolation
- Mock components can be created for testing
- Better test coverage and reliability

### 4. **Maintainability**
- Smaller, focused code files
- Clear separation of concerns
- Easier debugging and troubleshooting

### 5. **Thread Safety**
- Each component manages its own synchronization
- Reduced shared state between components
- More predictable concurrent behavior

## API Compatibility

The refactored implementation maintains 100% backward compatibility:

```cpp
// Original API still works
auto camera = std::make_shared<INDICamera>("CCD Simulator");
camera->initialize();
camera->connect("CCD Simulator");
camera->startExposure(1.0);
```

## Advanced Usage

For advanced users, individual components can be accessed:

```cpp
auto camera = std::make_shared<INDICamera>("CCD Simulator");

// Access specific components
auto exposure = camera->getExposureController();
auto video = camera->getVideoController();
auto temperature = camera->getTemperatureController();

// Component-specific operations
exposure->setSequenceCallback([](int frame, auto image) {
    // Custom sequence handling
});
```

## Component Communication

Components communicate through:

1. **Core Hub**: All components have access to the core
2. **Property System**: Properties are routed to interested components  
3. **Callbacks**: Components can register callbacks for events
4. **Shared State**: Some state is managed by the core

## Implementation Details

### Component Base Class

All components inherit from `ComponentBase`:

```cpp
class ComponentBase {
public:
    virtual auto initialize() -> bool = 0;
    virtual auto destroy() -> bool = 0;
    virtual auto getComponentName() const -> std::string = 0;
    virtual auto handleProperty(INDI::Property property) -> bool = 0;
protected:
    auto getCore() -> INDICameraCore*;
};
```

### Property Handling

Properties are handled hierarchically:

1. Core receives all INDI properties
2. PropertyHandler validates and routes properties
3. Interested components handle relevant properties
4. Components can register for specific properties

### Error Handling

Each component handles its own errors:

- Local error recovery where possible
- Error propagation to core when necessary
- Graceful degradation of functionality
- Comprehensive logging at all levels

## Migration Guide

### For Library Users
No changes required - the API is identical.

### For Developers

When extending camera functionality:

1. Identify the appropriate component
2. Add functionality to that component
3. Update the main camera class delegation
4. Add tests for the specific component

### Adding New Components

1. Inherit from `ComponentBase`
2. Implement required virtual methods
3. Register with the core in `INDICamera` constructor
4. Add property handlers to `PropertyHandler`

## Performance

The component-based architecture provides:

- **Better Memory Usage**: Components only allocate what they need
- **Improved Cache Locality**: Related data is grouped together
- **Reduced Lock Contention**: Finer-grained synchronization
- **Faster Compilation**: Smaller compilation units

## Future Enhancements

The new architecture enables:

1. **Plugin System**: Dynamic component loading
2. **Remote Components**: Network-distributed camera control
3. **AI Integration**: Smart exposure and focusing components
4. **Custom Workflows**: User-defined component combinations
5. **Performance Monitoring**: Per-component metrics and profiling

## File Structure

```
src/device/indi/camera/
├── component_base.hpp              # Base component interface
├── indi_camera.hpp/.cpp           # Main camera class
├── CMakeLists.txt                  # Build configuration
├── module.cpp                      # Atom component registration
├── core/
│   ├── indi_camera_core.hpp/.cpp  # Core INDI functionality
├── exposure/
│   └── exposure_controller.hpp/.cpp
├── video/
│   └── video_controller.hpp/.cpp
├── temperature/
│   └── temperature_controller.hpp/.cpp
├── hardware/
│   └── hardware_controller.hpp/.cpp
├── image/
│   └── image_processor.hpp/.cpp
├── sequence/
│   └── sequence_manager.hpp/.cpp
└── properties/
    └── property_handler.hpp/.cpp
```

## Conclusion

The component-based architecture provides a solid foundation for future development while maintaining compatibility with existing code. The modular design makes the codebase more maintainable and extensible, enabling rapid development of new features and improvements.
