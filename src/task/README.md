# Lithium Task System

High-performance task management library with split architecture for astronomical imaging.

## Directory Structure

```text
src/task/
├── task.hpp                    # Main aggregated header (entry point)
│
├── core/                       # Core manager submodule
│   ├── task.hpp/cpp            # Task base class
│   ├── target.hpp/cpp          # Target with astronomical data
│   ├── sequencer.hpp/cpp       # ExposureSequence with scheduling
│   ├── generator.hpp/cpp       # TaskGenerator for macros
│   ├── factory.hpp/cpp         # TaskFactory singleton
│   ├── registration.hpp/cpp    # Task registration
│   ├── exception.hpp           # Task exception types
│   └── types.hpp               # Aggregated header
│
├── components/                 # Component submodule
│   ├── common/                 # Shared components
│   │   ├── task_base.hpp       # Base class for device tasks
│   │   ├── types.hpp           # Common types and enums
│   │   └── validation.hpp      # Parameter validation
│   ├── camera/                 # Camera tasks
│   ├── focuser/                # Focuser tasks
│   ├── filterwheel/            # Filter wheel tasks
│   ├── guider/                 # Autoguiding tasks
│   ├── astrometry/             # Plate solving tasks
│   ├── observatory/            # Observatory control tasks
│   ├── workflow/               # Workflow tasks
│   ├── script/                 # Script execution tasks
│   └── components.hpp          # Aggregated header
│
├── utils/                      # Utilities submodule
│   ├── imagepath.hpp/cpp       # Image filename parsing
│   ├── integration_utils.hpp/cpp # Integration helpers
│   └── utils.hpp               # Aggregated header
│
├── adapters/                   # Adapters submodule
│   ├── api_adapter.hpp/cpp     # API types and converters
│   └── adapters.hpp            # Aggregated header
│
├── CMakeLists.txt
└── README.md
```

## Architecture Overview

```text
┌─────────────────────────────────────────────────────────┐
│                    ExposureSequence                      │
│  - Orchestrates multiple targets                         │
│  - Scheduling & Recovery strategies                      │
│  - Database persistence                                  │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┴────────────┐
        ▼                         ▼
┌───────────────┐         ┌───────────────┐
│    Target     │         │ TaskGenerator │
│  - Task groups│         │ - Macros      │
│  - Dependencies│        │ - Scripts     │
└───────┬───────┘         └───────────────┘
        │
        ▼
┌───────────────┐         ┌───────────────┐
│     Task      │◄────────│  TaskFactory  │
│  - Execute    │         │ - Create      │
│  - Validate   │         │ - Register    │
└───────────────┘         └───────────────┘
```

## Core Module

The `core/` submodule provides the main task management classes:

| File | Description |
|------|-------------|
| `task.hpp` | Base `Task` class with execution, timeout, dependencies |
| `target.hpp` | `Target` class with astronomical data and task groups |
| `sequencer.hpp` | `ExposureSequence` with scheduling strategies |
| `generator.hpp` | `TaskGenerator` for macro processing |
| `factory.hpp` | `TaskFactory` singleton for task creation |
| `registration.hpp` | Built-in task registration |
| `exception.hpp` | Task exception types |
| `types.hpp` | Aggregated header with all core types |

## Astronomical Features

### Target Observation Data

Each Target now includes comprehensive astronomical data:

```cpp
#include "task/target.hpp"

using namespace lithium::tools::astronomy;

// Configure astronomical target
TargetConfig config;
config.catalogName = "M31";
config.commonName = "Andromeda Galaxy";
config.coordinates = Coordinates::fromHMS(0.7123, 41.2689);
config.priority = 8;

// Add exposure plans
ExposurePlan lFilter{"L", 300.0, 20};  // 20x 5min Luminance
ExposurePlan rFilter{"R", 180.0, 10};  // 10x 3min Red
config.exposurePlans = {lFilter, rFilter};

target->setAstroConfig(config);
```

### Observability-Based Scheduling

```cpp
using namespace lithium::tools::astronomy;

// Set observer location
ObserverLocation location{39.9, 116.4, 50.0};
sequence.setObserverLocation(location);

// Sort targets by observability
sequence.sortTargetsByObservability();

// Get next best target to observe
auto* nextTarget = sequence.getNextObservableTarget();

// Get observability summary
json summary = sequence.getObservabilitySummary();
```

### Meridian Flip Handling

```cpp
// Check if any target needs meridian flip
std::string targetNeedingFlip = sequence.checkMeridianFlips();

// Mark flip completed after handling
target->markMeridianFlipCompleted();
```

## Components Module

The `components/` submodule provides device-specific tasks:

| Module | Namespace | Description |
|--------|-----------|-------------|
| common | `lithium::task` | Shared base classes and utilities |
| camera | `lithium::task::camera` | Camera exposure and imaging |
| focuser | `lithium::task::focuser` | Focus control and autofocus |
| filterwheel | `lithium::task::filterwheel` | Filter wheel and sequences |
| guider | `lithium::task::guider` | Autoguiding and dithering |
| astrometry | `lithium::task::astrometry` | Plate solving and centering |
| observatory | `lithium::task::observatory` | Safety and dome control |
| workflow | `lithium::task::workflow` | High-level workflow tasks |
| script | `lithium::task::script` | Script execution tasks |

## Task Categories (42+ Tasks Total)

### Camera Tasks (`components/camera/`)

- **basic_exposure**: TakeExposure, TakeManyExposure, SubframeExposure, CameraSettings, CameraPreview
- **sequence_tasks**: SmartExposure, DeepSkySequence, PlanetaryImaging, Timelapse
- **calibration_tasks**: AutoCalibration, ThermalCycle, FlatFieldSequence
- **focus_tasks**: AutoFocus, FocusSeries, TemperatureFocus
- **filter_tasks**: FilterSequence, RGBSequence, NarrowbandSequence
- **guide_tasks**: AutoGuiding, GuidedExposure, DitherSequence
- **safety_tasks**: WeatherMonitor, CloudDetection, SafetyShutdown
- **platesolve_tasks**: PlateSolveExposure, Centering, Mosaic

### Device Tasks (`components/`)

- **DeviceConnect**: Connect to astronomical equipment
- **DeviceDisconnect**: Safely disconnect devices

### Configuration Tasks (`components/`)

- **LoadConfig**: Load configuration from file
- **SaveConfig**: Save configuration to file

### Script Tasks (`components/script/`)

- **RunScript**: Execute Python or shell scripts
- **RunWorkflow**: Execute multi-step workflows

### Search Tasks (`components/`)

- **TargetSearch**: Search celestial object catalogs

### Mount Tasks

- **MountSlew**: Slew mount to coordinates
- **MountPark**: Park mount at position
- **MountTrack**: Control mount tracking

### Focuser Tasks (`components/focuser/`)

- **FocuserMove**: Move focuser to position

### Workflow Tasks (`components/workflow/`)

- **TargetAcquisition**: Complete target acquisition (slew, plate solve, center, guide, focus)
- **ExposureSequence**: Single target exposure sequence with filter changes and dithering
- **Session**: Complete observation session management
- **SafetyCheck**: Weather and safety monitoring
- **MeridianFlip**: Automated meridian flip handling
- **Dither**: Dithering between exposures
- **Wait**: Configurable wait conditions (duration, time, altitude, twilight)
- **CalibrationFrame**: Calibration frame acquisition (darks, flats, bias)

## Usage

### Quick Start

For most use cases, include the main header:

```cpp
#include "task/task.hpp"

// All functionality is available under lithium::task namespace
using namespace lithium::task;

// Initialize task system
initializeTaskSystem();

// Create and execute a simple task
auto task = createTask("TakeExposure", "my_exposure", {
    {"exposure", 30.0},
    {"binning", 1},
    {"gain", 100}
});
task->execute({});

// Create a sequence
auto sequence = createSequence();
sequence->addTarget(createTarget("M31", tasksJson));
sequence->executeAll();
```

### Creating Tasks via Factory

```cpp
#include "task/core/factory.hpp"
#include "task/core/registration.hpp"

// Initialize task registration
lithium::task::registerBuiltInTasks();

// Create task instance
auto& factory = lithium::task::TaskFactory::getInstance();
auto task = factory.createTask("TakeExposure", "my_exposure", {
    {"exposure", 30.0},
    {"binning", 1},
    {"gain", 100}
});

// Execute
task->execute(params);
```

### Creating Sequences

```cpp
#include "task/core/sequencer.hpp"
#include "task/core/target.hpp"

// Create sequence
lithium::task::ExposureSequence sequence;

// Add target with tasks
auto target = std::make_unique<lithium::task::Target>("M31_LRGB");
target->loadTasksFromJson(tasksJson);
sequence.addTarget(std::move(target));

// Set callbacks
sequence.setOnTargetEnd([](const std::string& name, auto status) {
    std::cout << "Target " << name << " completed\n";
});

// Execute
sequence.executeAll();
```

### Execution Strategies

```cpp
sequence.setExecutionStrategy(ExposureSequence::ExecutionStrategy::Sequential);
sequence.setExecutionStrategy(ExposureSequence::ExecutionStrategy::Parallel);
sequence.setSchedulingStrategy(ExposureSequence::SchedulingStrategy::Priority);
sequence.setRecoveryStrategy(ExposureSequence::RecoveryStrategy::Retry);
```

## API Integration

The task system integrates with the server through controller layer. API data types
and utilities are provided by `api_adapter.hpp`:

```cpp
#include "task/adapters/api_adapter.hpp"

// Use API data types
lithium::task::api::ApiResponse response =
    lithium::task::api::ApiResponse::success(data, "Operation completed");

// Create WebSocket events
auto event = lithium::task::api::WsEvent::create(
    lithium::task::api::WsEventType::SequenceProgress,
    progressData
);

// Convert sequence data
auto target = lithium::task::api::SequenceConverter::fromApiJson(sequenceJson);
```

### REST Endpoints (via server/controller)

- `POST /api/v1/sequences` - Create and start sequence
- `GET /api/v1/sequences/{id}` - Get sequence status
- `POST /api/v1/sequences/{id}/pause` - Pause sequence
- `POST /api/v1/sequences/{id}/resume` - Resume sequence
- `DELETE /api/v1/sequences/{id}` - Stop sequence

## Features

- **High-performance task execution** with timeout and cancellation support
- **Astronomical scheduling** with observability-based target ordering
- **Macro processing** for dynamic task generation
- **Multi-format serialization** (JSON, YAML)
- **Database persistence** for sequence storage
- **Thread-safe operations** with optimized locking
- **Performance monitoring** and metrics collection
- **Comprehensive error handling** and logging

## Thread Safety

All public operations in `ExposureSequence` and components are thread-safe:
- Read operations use shared locks
- Write operations use exclusive locks
- Callbacks are invoked asynchronously
- Background execution is handled in dedicated threads

## Building

The task system is built as part of the lithium-next project:

```bash
mkdir build && cd build
cmake ..
cmake --build . --target lithium_task
```

## Dependencies

- atom (type utilities)
- lithium_config
- lithium_database
- lithium_tools (astronomy types)
- loguru
- yaml-cpp
