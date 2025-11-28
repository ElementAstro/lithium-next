# Camera Task Module

Modular camera task system for astronomical imaging automation.

## Architecture

```text
camera/
├── common/                    # Camera-specific components
│   └── camera_task_base.hpp   # CameraTaskBase class
│
├── exposure/                  # Exposure tasks
│   └── exposure_tasks.hpp/cpp # TakeExposure, TakeManyExposure, Subframe, Smart
│
├── settings/                  # Camera configuration
│   └── settings_tasks.hpp/cpp # CameraSettings, CameraPreview
│
├── calibration/               # Calibration frame acquisition
│   └── calibration_tasks.hpp/cpp # AutoCalibration, ThermalCycle, FlatFieldSequence
│
├── imaging/                   # Advanced imaging
│   └── imaging_tasks.hpp/cpp  # DeepSkySequence, Planetary, Timelapse, Mosaic
│
└── camera_tasks.hpp           # Unified include header
```

## Related Modules (Moved Out)

The following task categories have been moved to separate modules for better organization:

| Module | Location | Tasks |
|--------|----------|-------|
| focuser/ | `src/task/custom/focuser/` | AutoFocus, FocusSeries, TemperatureFocus |
| filterwheel/ | `src/task/custom/filterwheel/` | FilterSequence, RGBSequence, NarrowbandSequence |
| guider/ | `src/task/custom/guider/` | StartGuiding, Dither, GuidedExposureSequence |
| astrometry/ | `src/task/custom/astrometry/` | PlateSolve, Centering, SyncToSolve |
| observatory/ | `src/task/custom/observatory/` | WeatherMonitor, SafetyShutdown, DomeControl |

## Namespace

Camera tasks are in `lithium::task::camera` namespace.

## Task List

| Category | Tasks |
|----------|-------|
| Exposure | TakeExposure, TakeManyExposure, SubframeExposure, SmartExposure |
| Settings | CameraSettings, CameraPreview |
| Calibration | AutoCalibration, ThermalCycle, FlatFieldSequence |
| Imaging | DeepSkySequence, PlanetaryImaging, Timelapse, Mosaic |

## Usage

### Include Header

```cpp
#include "custom/camera/camera_tasks.hpp"

using namespace lithium::task::camera;
```

### Include All Tasks

```cpp
#include "custom/all_tasks.hpp"

// Access all task namespaces
using namespace lithium::task::camera;
using namespace lithium::task::focuser;
using namespace lithium::task::guider;
```

### Create Task

```cpp
// Using factory
auto task = TaskFactory::getInstance().createTask("TakeExposure", "my_exposure", {
    {"exposure", 30.0},
    {"type", "light"},
    {"gain", 100}
});
task->execute({});

// Direct instantiation
TakeExposureTask exposure;
exposure.execute({
    {"exposure", 60.0},
    {"type", "light"},
    {"filter", "Ha"}
});
```

### Extend Tasks

```cpp
#include "common/camera_task_base.hpp"

class MyCustomTask : public CameraTaskBase {
public:
    MyCustomTask() : CameraTaskBase("MyCustomTask") {
        addParamDefinition("my_param", "number", true, nullptr, "My parameter");
    }

protected:
    void executeImpl(const json& params) override {
        logProgress("Executing custom task");
        // Implementation
    }
};
```

## Design Principles

1. **Single Responsibility**: Each task does one thing well
2. **Composition**: Complex workflows compose simple tasks
3. **Validation**: All parameters validated before execution
4. **Logging**: Consistent progress logging via `logProgress()`
5. **Timing**: Automatic execution timing via base class
6. **Cancellation**: All tasks support cancellation via `cancel()`
