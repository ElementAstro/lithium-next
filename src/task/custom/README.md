# Custom Task Modules

Modular task system for astronomical equipment automation.

## Directory Structure

```text
custom/
├── common/                    # Shared components
│   ├── task_base.hpp          # Base class for all tasks
│   ├── types.hpp              # Common types and enums
│   └── validation.hpp         # Parameter validation utilities
│
├── camera/                    # Camera tasks
│   ├── common/                # Camera-specific base class
│   ├── exposure/              # Exposure tasks
│   ├── settings/              # Camera settings
│   ├── calibration/           # Calibration frame tasks
│   ├── imaging/               # Advanced imaging tasks
│   └── camera_tasks.hpp       # Unified camera header
│
├── focuser/                   # Focuser tasks
│   ├── focuser_tasks.hpp
│   └── focuser_tasks.cpp
│
├── filterwheel/               # Filter wheel tasks
│   ├── filterwheel_tasks.hpp
│   └── filterwheel_tasks.cpp
│
├── guider/                    # Autoguiding tasks
│   ├── guider_tasks.hpp
│   └── guider_tasks.cpp
│
├── astrometry/                # Plate solving tasks
│   ├── astrometry_tasks.hpp
│   └── astrometry_tasks.cpp
│
├── observatory/               # Observatory control tasks
│   ├── observatory_tasks.hpp
│   └── observatory_tasks.cpp
│
├── factory.hpp                # Task factory and registry
├── all_tasks.hpp              # Unified include header
└── README.md                  # This file
```

## Module Overview

| Module | Namespace | Description |
|--------|-----------|-------------|
| common | `lithium::task` | Shared base classes and utilities |
| camera | `lithium::task::camera` | Camera exposure and imaging |
| focuser | `lithium::task::focuser` | Focus control and autofocus |
| filterwheel | `lithium::task::filterwheel` | Filter wheel and sequences |
| guider | `lithium::task::guider` | Autoguiding and dithering |
| astrometry | `lithium::task::astrometry` | Plate solving and centering |
| observatory | `lithium::task::observatory` | Safety and dome control |

## Task Inheritance Hierarchy

```
lithium::task::Task (base class)
    └── lithium::task::TaskBase (common functionality)
            ├── lithium::task::camera::CameraTaskBase
            │       ├── TakeExposureTask
            │       ├── TakeManyExposureTask
            │       └── ...
            ├── lithium::task::focuser::AutoFocusTask
            ├── lithium::task::focuser::FocusSeriesTask
            ├── lithium::task::filterwheel::FilterSequenceTask
            ├── lithium::task::guider::StartGuidingTask
            ├── lithium::task::astrometry::PlateSolveTask
            └── lithium::task::observatory::SafetyShutdownTask
```

## Quick Start

### Include All Tasks

```cpp
#include "custom/all_tasks.hpp"

using namespace lithium::task;
```

### Include Specific Module

```cpp
#include "custom/focuser/focuser_tasks.hpp"

using namespace lithium::task::focuser;
```

### Create a Task

```cpp
// Using factory
auto task = TaskFactory::getInstance().createTask("AutoFocus", "my_focus", {
    {"step_size", 100},
    {"max_steps", 15}
});
task->execute({});

// Direct instantiation
focuser::AutoFocusTask focus;
focus.execute({
    {"exposure", 3.0},
    {"step_size", 100}
});
```

### Create a Custom Task

```cpp
#include "common/task_base.hpp"

class MyCustomTask : public TaskBase {
public:
    MyCustomTask() : TaskBase("MyCustomTask") {
        addParamDefinition("my_param", "number", true, nullptr, "My parameter");
    }

protected:
    void executeImpl(const json& params) override {
        logProgress("Starting custom task");

        // Check for cancellation
        if (!shouldContinue()) {
            logProgress("Task cancelled");
            return;
        }

        // Do work...
        logProgress("Work complete", 1.0);
    }
};
```

## Task Features

All tasks inherit these features from `TaskBase`:

- **Parameter Validation**: Define and validate parameters
- **Progress Logging**: `logProgress(message, progress)`
- **Timing**: Automatic execution timing
- **Cancellation**: `cancel()` and `shouldContinue()`
- **Error Handling**: Exception handling with error types
- **Configuration**: JSON-based configuration support

## Task Categories

### Camera Tasks
- `TakeExposureTask` - Single exposure
- `TakeManyExposureTask` - Multiple exposures
- `SubframeExposureTask` - ROI exposure
- `SmartExposureTask` - Auto-optimized exposure
- `CameraSettingsTask` - Camera configuration
- `AutoCalibrationTask` - Calibration frames

### Focuser Tasks
- `AutoFocusTask` - Automated focus
- `FocusSeriesTask` - Focus test series
- `TemperatureFocusTask` - Temperature compensation
- `MoveFocuserTask` - Absolute move
- `MoveFocuserRelativeTask` - Relative move

### Filter Wheel Tasks
- `ChangeFilterTask` - Change filter
- `FilterSequenceTask` - Multi-filter sequence
- `RGBSequenceTask` - RGB imaging
- `NarrowbandSequenceTask` - Narrowband imaging
- `LRGBSequenceTask` - LRGB imaging

### Guider Tasks
- `StartGuidingTask` - Start autoguiding
- `StopGuidingTask` - Stop autoguiding
- `DitherTask` - Dither position
- `GuidedExposureSequenceTask` - Guided exposures
- `CalibrateGuiderTask` - Calibrate guider

### Astrometry Tasks
- `PlateSolveTask` - Solve existing image
- `PlateSolveExposureTask` - Take and solve
- `CenteringTask` - Center on target
- `SyncToSolveTask` - Sync mount
- `BlindSolveTask` - Blind plate solve

### Observatory Tasks
- `WeatherMonitorTask` - Weather monitoring
- `CloudDetectionTask` - Cloud detection
- `SafetyShutdownTask` - Safety shutdown
- `ObservatoryStartupTask` - Startup sequence
- `DomeControlTask` - Dome control
- `SafetyCheckTask` - Safety check

## Design Principles

1. **Single Responsibility** - Each task does one thing well
2. **Composition** - Complex workflows compose simple tasks
3. **Validation** - All parameters validated before execution
4. **Cancellation** - All tasks support graceful cancellation
5. **Logging** - Consistent progress and error logging
6. **Extensibility** - Easy to add new task types
