# Plate Solve Tasks Module

This module contains all plate solving related tasks for the Lithium astronomical imaging system. Plate solving is a key technique in astrometry that determines the exact coordinates and orientation of an astronomical image by comparing it to star catalogs.

## Tasks Included

### 1. PlateSolveExposureTask
- **Purpose**: Takes an exposure and performs plate solving for astrometry
- **Category**: Astrometry
- **Key Features**:
  - Configurable exposure parameters (time, binning, gain, offset)
  - Multiple solving attempts with adaptive exposure increase
  - Mock implementation for testing without hardware
  - Detailed logging and timing information

**Parameters**:
- `exposure` (double): Plate solve exposure time (default: 5.0s)
- `binning` (int): Camera binning for solving (default: 2)
- `max_attempts` (int): Maximum solve attempts (default: 3)
- `timeout` (double): Solve timeout in seconds (default: 60.0)
- `gain` (int): Camera gain (default: 100)
- `offset` (int): Camera offset (default: 10)

### 2. CenteringTask
- **Purpose**: Centers the telescope on a target using plate solving
- **Category**: Astrometry
- **Key Features**:
  - Iterative centering with configurable tolerance
  - Automatic offset calculation and mount correction
  - Supports multiple centering iterations
  - Integrated plate solving for position verification

**Required Parameters**:
- `target_ra` (double): Target Right Ascension in hours (0-24)
- `target_dec` (double): Target Declination in degrees (-90 to 90)

**Optional Parameters**:
- `tolerance` (double): Centering tolerance in arcseconds (default: 30.0)
- `max_iterations` (int): Maximum centering iterations (default: 5)
- `exposure` (double): Plate solve exposure time (default: 5.0)

### 3. MosaicTask
- **Purpose**: Performs automated mosaic imaging with plate solving and positioning
- **Category**: Astrometry
- **Key Features**:
  - Grid-based mosaic pattern generation
  - Configurable overlap between frames
  - Automatic centering at each position
  - Multiple frames per position support
  - Progress tracking and detailed logging

**Required Parameters**:
- `center_ra` (double): Mosaic center RA in hours (0-24)
- `center_dec` (double): Mosaic center Dec in degrees (-90 to 90)
- `grid_width` (int): Number of columns in mosaic grid (1-10)
- `grid_height` (int): Number of rows in mosaic grid (1-10)

**Optional Parameters**:
- `overlap` (double): Frame overlap percentage (default: 20.0, 0-50)
- `frame_exposure` (double): Exposure time per frame (default: 300.0)
- `frames_per_position` (int): Frames per mosaic position (default: 1)
- `auto_center` (bool): Auto-center each position (default: true)
- `gain` (int): Camera gain (default: 100)
- `offset` (int): Camera offset (default: 10)

## Mock Implementation

All tasks include mock implementations for testing without actual hardware:

- **MockPlateSolver**: Simulates plate solving with randomized coordinates
- **MockMount**: Simulates telescope mount movements and positioning

To enable mock mode, compile with `-DMOCK_CAMERA` flag.

## Dependencies

- **spdlog**: For logging
- **nlohmann_json**: For JSON parameter handling
- **atom/error/exception**: For error handling
- **Basic exposure tasks**: For camera operations

## Integration

The module integrates with:
- Camera exposure tasks for image acquisition
- Mount control for telescope positioning
- Task factory system for registration
- Enhanced task system for parameter validation and timeouts

## Usage Examples

### Simple Plate Solving
```json
{
  "task": "PlateSolveExposure",
  "params": {
    "exposure": 10.0,
    "binning": 2,
    "max_attempts": 3
  }
}
```

### Target Centering
```json
{
  "task": "Centering",
  "params": {
    "target_ra": 20.5,
    "target_dec": 40.25,
    "tolerance": 30.0,
    "max_iterations": 5
  }
}
```

### 2x2 Mosaic
```json
{
  "task": "Mosaic",
  "params": {
    "center_ra": 12.0,
    "center_dec": 45.0,
    "grid_width": 2,
    "grid_height": 2,
    "overlap": 25.0,
    "frame_exposure": 600.0,
    "frames_per_position": 3
  }
}
```

## Error Handling

All tasks include comprehensive error handling with:
- Parameter validation
- Runtime error reporting
- Timeout management
- Detailed error messages with context

## Future Enhancements

Planned improvements include:
- Integration with real plate solving software (Astrometry.net, ASTAP)
- Advanced mosaic patterns (spiral, custom paths)
- Dynamic exposure adjustment based on solving success
- Sky quality assessment integration
- Automatic guide star selection for centering
