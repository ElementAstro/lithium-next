# Advanced Astrophotography Tasks

This directory contains advanced automated imaging tasks for the Lithium astrophotography control software. These tasks provide sophisticated functionality for automated imaging sequences and intelligent exposure control.

## Available Tasks

### SmartExposureTask
- **Purpose**: Automatically optimizes exposure time to achieve target signal-to-noise ratio (SNR)
- **Key Features**:
  - Iterative exposure optimization
  - Configurable SNR targets
  - Automatic exposure adjustment
  - Support for min/max exposure limits
- **Use Case**: Optimal exposure determination for varying conditions

### DeepSkySequenceTask
- **Purpose**: Performs automated deep sky imaging sequences with multiple filters
- **Key Features**:
  - Multi-filter support
  - Automatic dithering
  - Progress tracking
  - Configurable exposure counts per filter
- **Use Case**: Long-duration deep sky object imaging

### PlanetaryImagingTask
- **Purpose**: High-speed planetary imaging with lucky imaging support
- **Key Features**:
  - High frame rate capture
  - Multi-filter planetary imaging
  - Configurable video length
  - Lucky imaging optimization
- **Use Case**: Planetary detail capture through atmospheric turbulence

### TimelapseTask
- **Purpose**: Captures timelapse sequences with configurable intervals
- **Key Features**:
  - Automatic exposure adjustment
  - Multiple timelapse types (sunset, lunar, star trails)
  - Configurable frame intervals
  - Long-duration capture support
- **Use Case**: Time-lapse astronomy and atmospheric phenomena

### MeridianFlipTask
- **Purpose**: Automated meridian flip when telescope crosses the meridian
- **Key Features**:
  - Automatic flip detection and execution
  - Plate solving verification after flip
  - Optional autofocus after flip
  - Camera rotation support
  - Configurable flip timing
- **Use Case**: Uninterrupted long exposure sequences across meridian

### IntelligentSequenceTask
- **Purpose**: Advanced multi-target imaging with intelligent decision making
- **Key Features**:
  - Dynamic target selection based on conditions
  - Weather monitoring integration
  - Target priority calculation
  - Automatic session planning
  - Visibility checking
- **Use Case**: Fully automated observatory operations

### AutoCalibrationTask
- **Purpose**: Comprehensive calibration frame capture and organization
- **Key Features**:
  - Automated dark, bias, and flat frame capture
  - Multi-filter flat field support
  - Optimal exposure determination for flats
  - Organized file structure
  - Skip existing calibration option
- **Use Case**: Maintenance-free calibration library creation

## Task Categories

All advanced tasks are categorized as "Advanced" in the task system and provide:
- Enhanced error handling and logging
- Parameter validation
- Timeout management
- Priority scheduling
- Progress reporting

## Dependencies

These tasks depend on:
- `TakeExposure` task for basic camera operations
- Camera device drivers
- Task execution framework
- Logging and error handling systems

## Integration

Tasks are automatically registered with the task factory system and can be executed through:
- REST API endpoints
- Script automation
- Manual task execution
- Scheduled sequences

## Usage Examples

### Smart Exposure
```json
{
  "task": "SmartExposure",
  "params": {
    "target_snr": 50.0,
    "max_exposure": 300.0,
    "min_exposure": 1.0,
    "max_attempts": 5
  }
}
```

### Deep Sky Sequence
```json
{
  "task": "DeepSkySequence",
  "params": {
    "target_name": "M42",
    "total_exposures": 60,
    "exposure_time": 300.0,
    "filters": ["L", "R", "G", "B"],
    "dithering": true
  }
}
```

### Planetary Imaging
```json
{
  "task": "PlanetaryImaging",
  "params": {
    "planet": "Jupiter",
    "video_length": 120,
    "frame_rate": 30.0,
    "filters": ["R", "G", "B"]
  }
}
```

### Timelapse
```json
{
  "task": "Timelapse",
  "params": {
    "total_frames": 200,
    "interval": 30.0,
    "exposure_time": 10.0,
    "type": "sunset",
    "auto_exposure": true
  }
}
```

### Meridian Flip
```json
{
  "task": "MeridianFlip",
  "params": {
    "target_ra": 12.5,
    "target_dec": 45.0,
    "flip_offset_minutes": 5.0,
    "autofocus_after_flip": true,
    "platesolve_after_flip": true
  }
}
```

### Intelligent Sequence
```json
{
  "task": "IntelligentSequence",
  "params": {
    "targets": [
      {
        "name": "M42",
        "ra": 5.58,
        "dec": -5.39,
        "exposures": 60,
        "exposure_time": 300.0,
        "filters": ["L", "R", "G", "B"],
        "priority": 8.0
      },
      {
        "name": "M31",
        "ra": 0.71,
        "dec": 41.27,
        "exposures": 40,
        "exposure_time": 600.0,
        "filters": ["L"],
        "priority": 6.0
      }
    ],
    "session_duration_hours": 8.0,
    "weather_monitoring": true,
    "dynamic_target_selection": true
  }
}
```

### Auto Calibration
```json
{
  "task": "AutoCalibration",
  "params": {
    "output_directory": "./calibration/2024-06-15",
    "dark_frame_count": 30,
    "bias_frame_count": 50,
    "flat_frame_count": 20,
    "filters": ["L", "R", "G", "B"],
    "exposure_times": [300.0, 600.0],
    "temperature": -10.0,
    "skip_existing": true
  }
}
```

## Development

When adding new advanced tasks:
1. Create separate .hpp and .cpp files
2. Inherit from the Task base class
3. Implement required virtual methods
4. Add task registration in `task_registration.cpp`
5. Update CMakeLists.txt if needed
6. Add comprehensive parameter validation
7. Include proper error handling and logging
