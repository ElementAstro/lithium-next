# Lithium Tools

Astronomy calculation and utility tools library.

## Directory Structure

```text
src/tools/
├── tools.hpp               # Main aggregated header (entry point)
│
├── astronomy/              # Astronomical types submodule
│   ├── constants.hpp       # Mathematical and physical constants
│   ├── constraints.hpp     # Observation constraints (altitude, time)
│   ├── coordinates.hpp     # Coordinate systems (RA/Dec, Alt/Az, location)
│   ├── exposure.hpp        # Exposure planning types
│   ├── meridian.hpp        # Meridian flip handling
│   ├── target_config.hpp   # Complete target configuration
│   └── types.hpp           # Aggregated header
│
├── calculation/            # Astronomical calculations submodule
│   ├── julian.hpp          # Julian Date calculations
│   ├── sidereal.hpp        # Sidereal time calculations
│   ├── transform.hpp       # Coordinate transformations
│   ├── precession.hpp      # Precession, nutation, aberration
│   └── calculate.hpp       # Aggregated header
│
├── conversion/             # Unit conversion submodule
│   ├── angle.hpp           # Angle conversions (deg/rad/hour)
│   ├── coordinate.hpp      # Coordinate conversions
│   ├── format.hpp          # Formatting utilities (RA/Dec strings)
│   └── convert.hpp         # Aggregated header
│
├── solver/                 # Plate solving submodule
│   ├── wcs.hpp             # WCS parameter handling
│   └── wcs.cpp             # WCS implementation
│
├── CMakeLists.txt
└── README.md
```

## Astronomy Types Module

The `astronomy/` submodule provides comprehensive types for astronomical observation planning:

### Usage

```cpp
#include "tools/astronomy/types.hpp"

using namespace lithium::tools::astronomy;

// Create celestial coordinates
Coordinates target{83.633, 22.014, 2000.0};  // Orion Nebula (M42)

// Create observer location
ObserverLocation observer{39.9, 116.4, 50.0};  // Beijing

// Set altitude constraints
AltitudeConstraints constraints{15.0, 85.0};  // Min 15°, Max 85°

// Create exposure plan
ExposurePlan luminance{"L", 300.0, 20, 1};  // 20x 5min Luminance

// Complete target configuration
TargetConfig config;
config.catalogName = "M42";
config.commonName = "Orion Nebula";
config.coordinates = target;
config.altConstraints = constraints;
config.exposurePlans.push_back(luminance);
```

### Module Components

| File | Description |
|------|-------------|
| `constants.hpp` | Mathematical constants (π, conversions), physical constants (Earth radius, AU, etc.), utility functions |
| `coordinates.hpp` | `Coordinates` (RA/Dec), `HorizontalCoordinates` (Alt/Az), `ObserverLocation` |
| `constraints.hpp` | `AltitudeConstraints`, `ObservabilityWindow`, `TimeConstraints` |
| `meridian.hpp` | `MeridianState`, `MeridianFlipInfo`, `MeridianFlipSettings` |
| `exposure.hpp` | `ExposurePlan`, `ExposurePlanCollection` |
| `target_config.hpp` | `TargetConfig` - complete target configuration |
| `types.hpp` | Aggregated header with all types |

## Calculation Module

The `calculation/` submodule provides astronomical calculation functions:

### Julian Date (`calculation/julian.hpp`)

```cpp
#include "tools/calculation/julian.hpp"

using namespace lithium::tools::calculation;

// Create DateTime
DateTime dt{2024, 12, 26, 12, 0, 0.0};

// Calculate Julian Date
double jd = calculateJulianDate<double>(dt);

// Convert system time to JD
auto now = std::chrono::system_clock::now();
double jdNow = timeToJD(now);

// Modified Julian Date
double mjd = jdToMJD(jd);
```

### Sidereal Time (`calculation/sidereal.hpp`)

```cpp
#include "tools/calculation/sidereal.hpp"

using namespace lithium::tools::calculation;

// Greenwich Mean Sidereal Time
double gmst = calculateGMST(jd);

// Local Sidereal Time
double lst = calculateLST(jd, longitude);

// Hour angle
double ha = calculateHourAngle(lst, ra);
```

### Coordinate Transformations (`calculation/transform.hpp`)

```cpp
#include "tools/calculation/transform.hpp"

using namespace lithium::tools::calculation;

// Equatorial to Horizontal
auto altAz = equatorialToHorizontal(ra, dec, latitude, lst);

// With atmospheric refraction
double refraction = calculateRefraction(altitude);

// Field rotation rate
double rate = calculateFieldRotationRate(alt, az, lat);
```

### Precession (`calculation/precession.hpp`)

```cpp
#include "tools/calculation/precession.hpp"

using namespace lithium::tools::calculation;

// Apply precession
auto newCoords = applyPrecession(coords, fromJD, toJD);

// J2000 conversions
auto j2000 = observedToJ2000(observed, jd);
auto observed = j2000ToObserved(j2000, jd);
```

## Conversion Module

The `conversion/` submodule provides unit conversions:

### Angle Conversions (`conversion/angle.hpp`)

```cpp
#include "tools/conversion/angle.hpp"

using namespace lithium::tools::conversion;

// Basic conversions
double rad = degreeToRad(45.0);
double deg = radToDegree(M_PI / 4);
double hours = degreeToHour(180.0);

// Normalization
double ra = normalizeRightAscension(25.5);  // -> 1.5 hours
double dec = normalizeDeclination(95.0);    // -> 90.0 degrees

// DMS/HMS conversions
double decimal = dmsToDegree(45, 30, 15.5);
```

### Coordinate Conversions (`conversion/coordinate.hpp`)

```cpp
#include "tools/conversion/coordinate.hpp"

using namespace lithium::tools::conversion;

// Equatorial to Cartesian
auto cart = equatorialToCartesian(ra, dec, radius);

// Cartesian to Spherical
auto spherical = cartesianToSpherical(cart);
```

### Formatting (`conversion/format.hpp`)

```cpp
#include "tools/conversion/format.hpp"

using namespace lithium::tools::conversion;

// Format RA/Dec
std::string raStr = formatRa(12.5);      // "12h 30m 00.00s"
std::string decStr = formatDec(-45.25);  // "-45° 15' 00.00\""
```

## Solver Module

The `solver/` submodule provides plate solving utilities:

```cpp
#include "tools/solver/wcs.hpp"

using namespace lithium::tools::solver;

// Parse WCS parameters
WCSParams wcs = extractWCSParams(wcsInfo);

// Pixel to RA/Dec
auto coords = pixelToRaDec(x, y, wcs);

// Get FOV corners
auto corners = getFOVCorners(wcs, width, height);
```

## Quick Start

For most use cases, include the main header:

```cpp
#include "tools/tools.hpp"

// All functionality is available under lithium::tools namespace
```

## Building

The tools library is built as part of the lithium-next project:

```bash
mkdir build && cd build
cmake ..
cmake --build . --target lithium_tools
```

## Dependencies

- atom (type utilities, macros)
- loguru (logging)
- yaml-cpp (configuration)
- nlohmann_json (JSON serialization)
