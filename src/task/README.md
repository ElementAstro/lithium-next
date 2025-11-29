# Lithium Task System

A comprehensive framework for managing, executing, and sequencing astronomical imaging tasks.

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

## Core Components

| Component | File | Purpose |
|-----------|------|---------|
| Task | `task.hpp/cpp` | Base class for all executable tasks |
| Target | `target.hpp/cpp` | Groups tasks, manages astronomical target data |
| ExposureSequence | `sequencer.hpp/cpp` | Orchestrates targets with observability scheduling |
| TaskFactory | `custom/factory.hpp/cpp` | Dynamic task creation |
| TaskGenerator | `generator.hpp/cpp` | Macro processing and script generation |
| AstroTypes | `tools/astronomy/types.hpp` | Astronomical data structures (coordinates, observability, exposure plans) |

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

## Task Categories (42 Tasks Total)

### Camera Tasks (`custom/camera/`)

- **basic_exposure**: TakeExposure, TakeManyExposure, SubframeExposure, CameraSettings, CameraPreview
- **sequence_tasks**: SmartExposure, DeepSkySequence, PlanetaryImaging, Timelapse
- **calibration_tasks**: AutoCalibration, ThermalCycle, FlatFieldSequence
- **focus_tasks**: AutoFocus, FocusSeries, TemperatureFocus
- **filter_tasks**: FilterSequence, RGBSequence, NarrowbandSequence
- **guide_tasks**: AutoGuiding, GuidedExposure, DitherSequence
- **safety_tasks**: WeatherMonitor, CloudDetection, SafetyShutdown
- **platesolve_tasks**: PlateSolveExposure, Centering, Mosaic

### Device Tasks

- **DeviceConnect**: Connect to astronomical equipment
- **DeviceDisconnect**: Safely disconnect devices

### Configuration Tasks

- **LoadConfig**: Load configuration from file
- **SaveConfig**: Save configuration to file

### Script Tasks

- **RunScript**: Execute Python or shell scripts
- **RunWorkflow**: Execute multi-step workflows

### Search Tasks

- **TargetSearch**: Search celestial object catalogs

### Mount Tasks

- **MountSlew**: Slew mount to coordinates
- **MountPark**: Park mount at position
- **MountTrack**: Control mount tracking

### Focuser Tasks

- **FocuserMove**: Move focuser to position

### Workflow Tasks (`custom/workflow/`)

- **TargetAcquisition**: Complete target acquisition (slew, plate solve, center, guide, focus)
- **ExposureSequence**: Single target exposure sequence with filter changes and dithering
- **Session**: Complete observation session management
- **SafetyCheck**: Weather and safety monitoring
- **MeridianFlip**: Automated meridian flip handling
- **Dither**: Dithering between exposures
- **Wait**: Configurable wait conditions (duration, time, altitude, twilight)
- **CalibrationFrame**: Calibration frame acquisition (darks, flats, bias)

## Usage

### Creating Tasks via Factory

```cpp
#include "task/custom/factory.hpp"
#include "task/registration.hpp"

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
#include "task/sequencer.hpp"
#include "task/target.hpp"

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
#include "task/api_adapter.hpp"

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

## File Structure

```text
src/task/
├── lithium_task.hpp       # Unified header (entry point)
├── task.hpp/cpp           # Base Task class
├── target.hpp/cpp         # Target with astronomical data
├── sequencer.hpp/cpp      # ExposureSequence with observability scheduling
├── generator.hpp/cpp      # TaskGenerator
├── (astro_types moved to tools/astronomy/)
├── imagepath.hpp/cpp      # Image filename parsing
├── registration.hpp/cpp   # Task registration (50+ tasks)
├── integration_utils.hpp/cpp  # Integration helpers
├── api_adapter.hpp/cpp    # API types, converters, utilities
├── CMakeLists.txt
├── README.md
└── custom/
    ├── factory.hpp/cpp    # TaskFactory singleton
    ├── common/            # Common task utilities
    │   └── task_base.hpp  # Base class for derived tasks
    ├── camera/            # Camera tasks
    │   ├── exposure/      # Exposure tasks
    │   ├── settings/      # Camera settings tasks
    │   ├── calibration/   # Calibration tasks
    │   └── imaging/       # Imaging workflow tasks
    ├── focuser/           # Focuser tasks
    │   └── focuser_tasks.hpp/cpp
    ├── workflow/          # Astronomical workflow tasks (NEW)
    │   └── workflow_tasks.hpp/cpp  # Session, acquisition, dither, etc.
    ├── script/            # Script subsystem
    │   ├── base.hpp/cpp
    │   ├── python.hpp/cpp
    │   ├── shell.hpp/cpp
    │   └── workflow.hpp/cpp
    ├── device_task.hpp/cpp
    ├── config_task.hpp/cpp
    ├── script_task.hpp/cpp
    ├── search_task.hpp/cpp
    └── solver_task.hpp
```

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
