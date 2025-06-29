# Lithium Device System - Complete Architecture Documentation

## Overview

The Lithium Device System is a comprehensive, INDI-compatible device control framework for astrophotography applications. It provides a unified interface for controlling various astronomical devices through multiple backends (Mock, INDI, ASCOM, Native).

## Architecture Components

### 1. Device Templates (`template/`)

#### Base Device (`device.hpp`)
- **Enhanced INDI-style architecture** with property management
- **State management** and device capabilities system
- **Configuration management** and device information structures
- **Thread-safe operations** with proper mutex protection

#### Specialized Device Types

1. **Camera (`camera.hpp`)**
   - Complete exposure control with progress tracking
   - Video streaming and live preview capabilities
   - Temperature control with cooling management
   - Gain/Offset/ISO parameter control
   - Binning and subframe support
   - Multiple frame formats (FITS, NATIVE, XISF, etc.)
   - Event callbacks for exposure completion

2. **Telescope (`telescope.hpp`)**
   - Comprehensive coordinate system support (RA/DEC, AZ/ALT)
   - Advanced tracking modes (sidereal, solar, lunar, custom)
   - Parking and home position management
   - Guiding pulse support
   - Pier side detection and management
   - Location and time synchronization
   - Multiple slew rates and motion control

3. **Focuser (`focuser.hpp`)**
   - Absolute and relative positioning
   - Temperature compensation with coefficients
   - Backlash compensation
   - Speed control and limits
   - Auto-focus support with progress tracking
   - Preset positions (10 slots)
   - Move statistics and history

4. **Filter Wheel (`filterwheel.hpp`)**
   - Advanced filter management with metadata
   - Filter information (name, type, wavelength, bandwidth)
   - Search and selection by name/type
   - Configuration presets and profiles
   - Temperature monitoring (if supported)
   - Move statistics and optimization

5. **Rotator (`rotator.hpp`)**
   - Precise angle control with normalization
   - Direction control and reversal
   - Speed management with limits
   - Backlash compensation
   - Preset angle positions
   - Shortest path calculation

6. **Dome (`dome.hpp`)**
   - Azimuth control with telescope following
   - Shutter control with safety checks
   - Weather monitoring integration
   - Parking and home position
   - Speed control and backlash compensation
   - Safety interlocks

7. **Additional Devices**
   - **Guider**: Complete guiding system with calibration
   - **Weather Station**: Comprehensive weather monitoring
   - **Safety Monitor**: Safety system integration
   - **Adaptive Optics**: Advanced optics control

### 2. Mock Device Implementations (`template/mock/`)

All mock devices provide realistic simulation with:
- **Threaded movement simulation** with progress updates
- **Random noise injection** for realistic behavior
- **Proper timing simulation** based on device characteristics
- **Event callbacks** for state changes
- **Statistics tracking** and configuration persistence

#### Available Mock Devices
- `MockCamera`: Complete camera simulation with exposure and cooling
- `MockTelescope`: Full mount simulation with tracking and slewing
- `MockFocuser`: Focuser with temperature compensation
- `MockFilterWheel`: 8-position filter wheel with preset filters
- `MockRotator`: Field rotator with angle management
- `MockDome`: Observatory dome with shutter control

### 3. Device Factory System (`device_factory.hpp`)

- **Unified device creation** interface
- **Multiple backend support** (Mock, INDI, ASCOM, Native)
- **Device discovery** and enumeration
- **Runtime backend detection**
- **Custom device registration** system
- **Type-safe device creation**

### 4. Configuration Management (`device_config.hpp`)

- **JSON-based configuration** with validation
- **Device profiles** for different setups
- **Global settings** management
- **Configuration templates** for common devices
- **Automatic configuration persistence**
- **Profile switching** for different observing scenarios

### 5. Integration and Testing

#### Device Integration Test (`device_integration_test.cpp`)
Comprehensive test demonstrating:
- Individual device operations
- Coordinated multi-device sequences
- Automated imaging workflows
- Error handling and recovery
- Performance monitoring

#### Build System (`CMakeLists.txt`)
- **Modular library structure**
- **Optional INDI integration**
- **Testing framework integration**
- **Header installation**
- **Cross-platform compatibility**

## Device Capabilities

### Camera Features
- ✅ Exposure control with sub-second precision
- ✅ Temperature control and cooling
- ✅ Gain/Offset/ISO adjustment
- ✅ Binning and subframe support
- ✅ Multiple image formats
- ✅ Video streaming
- ✅ Bayer pattern support
- ✅ Event-driven callbacks

### Telescope Features
- ✅ Multiple coordinate systems
- ✅ Precise tracking control
- ✅ Parking and home positions
- ✅ Guiding pulse support
- ✅ Pier side management
- ✅ Multiple slew rates
- ✅ Safety limits

### Focuser Features
- ✅ Absolute/relative positioning
- ✅ Temperature compensation
- ✅ Backlash compensation
- ✅ Auto-focus integration
- ✅ Preset positions
- ✅ Move optimization

### Filter Wheel Features
- ✅ Smart filter management
- ✅ Metadata support
- ✅ Search and selection
- ✅ Configuration profiles
- ✅ Move optimization

### Rotator Features
- ✅ Precise angle control
- ✅ Shortest path calculation
- ✅ Backlash compensation
- ✅ Preset positions

### Dome Features
- ✅ Telescope coordination
- ✅ Shutter control
- ✅ Weather integration
- ✅ Safety monitoring

## Usage Examples

### Basic Device Creation
```cpp
auto factory = DeviceFactory::getInstance();
auto camera = factory.createCamera("MainCamera", DeviceBackend::MOCK);
camera->setSimulated(true);
camera->connect();
```

### Coordinated Operations
```cpp
// Point telescope and follow with dome
telescope->slewToRADECJNow(20.0, 30.0);
auto coords = telescope->getRADECJNow();
dome->setTelescopePosition(coords->ra * 15.0, coords->dec);

// Change filter and rotate
filterwheel->selectFilterByName("Luminance");
rotator->moveToAngle(45.0);

// Focus and capture
focuser->moveToPosition(1500);
camera->startExposure(5.0);
```

### Configuration Management
```cpp
auto& config = DeviceConfigManager::getInstance();
config.loadProfile("DeepSky");
auto devices = config.createAllDevicesFromActiveProfile();
```

## Integration with INDI

The system is designed for seamless INDI integration:
- **Property-based architecture** matching INDI design
- **Device state management** following INDI patterns
- **Event-driven callbacks** for property updates
- **Standard device interfaces** compatible with INDI clients
- **Automatic device discovery** through INDI protocols

## Future Enhancements

1. **INDI Backend Implementation**
   - Complete INDI client integration
   - Device property synchronization
   - BLOB handling for images

2. **ASCOM Integration** (Windows)
   - ASCOM platform integration
   - Device enumeration and control

3. **Advanced Features**
   - Plate solving integration
   - Automated sequences
   - Equipment profiles
   - Cloud connectivity

4. **Performance Optimizations**
   - Parallel device operations
   - Caching and optimization
   - Memory management

## Conclusion

The Lithium Device System provides a robust, extensible foundation for astrophotography control software. With comprehensive device support, multiple backends, and realistic simulation capabilities, it enables development and testing of complex astronomical applications without requiring physical hardware.

The system's modular design allows for easy extension and customization while maintaining compatibility with industry-standard protocols like INDI and ASCOM.
