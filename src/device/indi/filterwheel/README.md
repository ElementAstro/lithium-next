# INDI FilterWheel Module - Modular Architecture

This directory contains a modular implementation of the INDI FilterWheel device driver, split into specialized components for better maintainability and extensibility.

## Architecture

The filterwheel module is split into the following components:

### Core Components

1. **base.hpp/cpp** - Base INDI client functionality
   - Device connection and communication
   - Property watching and message handling
   - Basic INDI protocol implementation

2. **control.hpp/cpp** - Movement and position control
   - Filter position management
   - Movement control (abort, home, calibrate)
   - Position validation and state management

3. **filter_manager.hpp/cpp** - Filter information management
   - Filter naming and metadata
   - Filter search and selection
   - Enhanced filter information handling

4. **statistics.hpp/cpp** - Statistics and monitoring
   - Move counting and timing
   - Temperature monitoring
   - Performance metrics

5. **configuration.hpp/cpp** - Configuration management
   - Save/load filter configurations
   - Import/export functionality
   - Persistent settings storage

6. **filterwheel.hpp/cpp** - Main composite class
   - Combines all components using multiple inheritance
   - Provides unified interface
   - Component registration and module export

## Features

### ✅ Complete INDI Integration
- Full INDI protocol support
- Property watching and callbacks
- Automatic device discovery
- Message handling and logging

### ✅ Advanced Filter Management
- Named filter slots
- Filter type categorization
- Wavelength and bandwidth information
- Filter search by name or type
- Batch filter operations

### ✅ Movement Control
- Position validation
- Abort motion capability
- Homing and calibration
- Movement state tracking
- Timeout handling

### ✅ Statistics & Monitoring
- Total move counter
- Average move time calculation
- Moves per hour metrics
- Temperature monitoring (if supported)
- Uptime tracking

### ✅ Configuration System
- Save/load named configurations
- Export/import to external files
- Simple text-based format
- Persistent settings storage

### ✅ Modern C++ Features
- C++20 standard compliance
- spdlog for structured logging
- RAII resource management
- std::optional for nullable returns
- Smart pointers for memory safety

## Usage Example

```cpp
#include "filterwheel/filterwheel.hpp"

// Create filterwheel instance
auto filterwheel = std::make_shared<INDIFilterwheel>("MyFilterWheel");

// Connect to device
filterwheel->connect("ASI Filter Wheel");

// Set filter by position
filterwheel->setPosition(2);

// Set filter by name
filterwheel->selectFilterByName("Luminance");

// Get filter information
auto info = filterwheel->getFilterInfo(2);
if (info) {
    std::cout << "Filter: " << info->name << " (" << info->type << ")" << std::endl;
}

// Save current configuration
filterwheel->saveFilterConfiguration("MySetup");

// Get statistics
auto totalMoves = filterwheel->getTotalMoves();
auto avgTime = filterwheel->getAverageMoveTime();
```

## Component Registration

The module automatically registers all components and methods with the Atom component system:

```cpp
// Connection management
connect, disconnect, scan, is_connected

// Movement control  
get_position, set_position, is_moving, abort_motion
home_filter_wheel, calibrate_filter_wheel

// Filter management
get_filter_count, get_slot_name, set_slot_name
find_filter_by_name, select_filter_by_name

// Statistics
get_total_moves, get_average_move_time, get_temperature

// Configuration
save_configuration, load_configuration, export_configuration
```

## Logging

All components use structured logging with spdlog:

```cpp
logger_->info("Setting filter position to: {}", position);
logger_->error("Failed to connect to device: {}", deviceName);
logger_->debug("Filter wheel temperature: {:.2f}°C", temp);
```

## Benefits of Modular Design

1. **Separation of Concerns** - Each component handles a specific aspect
2. **Maintainability** - Easier to modify and extend individual features
3. **Testability** - Components can be tested independently
4. **Reusability** - Components can be reused in other device drivers
5. **Code Organization** - Logical grouping of related functionality

## Thread Safety

- All atomic operations use std::atomic
- Property callbacks are thread-safe
- Statistics recording is thread-safe
- Configuration operations include proper locking

## Error Handling

- Comprehensive error checking at all levels
- Proper exception handling with logging
- Graceful degradation when features unavailable
- Timeout handling for long operations

This modular architecture provides a robust, maintainable, and extensible foundation for INDI filterwheel device control.
