# Camera Support Matrix

This document provides a comprehensive overview of all supported camera brands and their features in the lithium astrophotography control software.

## Supported Camera Brands

| Brand | Driver Type | SDK Required | Cooling | Video | Filter Wheel | Guide Chip | Status |
|-------|-------------|--------------|---------|-------|--------------|------------|--------|
| **INDI** | Universal | INDI Server | ✅ | ✅ | ✅ | ✅ | ✅ Stable |
| **QHY** | Native SDK | QHY SDK | ✅ | ✅ | ❌ | ❌ | ✅ Stable |
| **ZWO ASI** | Native SDK | ASI SDK | ✅ | ✅ | ❌ | ❌ | ✅ Stable |
| **Atik** | Native SDK | Atik SDK | ✅ | ✅ | ✅ | ❌ | 🚧 Beta |
| **SBIG** | Native SDK | SBIG Universal | ✅ | ⚠️ | ✅ | ✅ | 🚧 Beta |
| **FLI** | Native SDK | FLI SDK | ✅ | ✅ | ✅ | ❌ | 🚧 Beta |
| **PlayerOne** | Native SDK | PlayerOne SDK | ✅ | ✅ | ❌ | ❌ | 🚧 Beta |
| **ASCOM** | Windows Only | ASCOM Platform | ✅ | ❌ | ✅ | ❌ | ⚠️ Limited |
| **Simulator** | Built-in | None | ✅ | ✅ | ✅ | ✅ | ✅ Stable |

## Feature Comparison

### Core Features

| Feature | INDI | QHY | ASI | Atik | SBIG | FLI | PlayerOne | ASCOM | Simulator |
|---------|------|-----|-----|------|------|-----|-----------|-------|-----------|
| **Exposure Control** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Abort Exposure** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Progress Monitoring** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Subframe/ROI** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Binning** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Multiple Formats** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |

### Advanced Features

| Feature | INDI | QHY | ASI | Atik | SBIG | FLI | PlayerOne | ASCOM | Simulator |
|---------|------|-----|-----|------|------|-----|-----------|-------|-----------|
| **Temperature Control** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Gain Control** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |
| **Offset Control** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |
| **Video Streaming** | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ | ✅ | ❌ | ✅ |
| **Sequence Capture** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |
| **Auto Exposure** | ⚠️ | ✅ | ✅ | ✅ | ❌ | ❌ | ✅ | ❌ | ✅ |
| **Auto Gain** | ⚠️ | ❌ | ✅ | ❌ | ❌ | ❌ | ✅ | ❌ | ✅ |

### Hardware-Specific Features

| Feature | INDI | QHY | ASI | Atik | SBIG | FLI | PlayerOne | ASCOM | Simulator |
|---------|------|-----|-----|------|------|-----|-----------|-------|-----------|
| **Mechanical Shutter** | ✅ | ✅ | ❌ | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ |
| **Guide Chip** | ✅ | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ | ⚠️ | ✅ |
| **Integrated Filter Wheel** | ✅ | ❌ | ❌ | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ |
| **Fan Control** | ✅ | ❌ | ❌ | ✅ | ❌ | ✅ | ❌ | ⚠️ | ✅ |
| **USB Traffic Control** | ❌ | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ |
| **Hardware Binning** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

## Camera-Specific Implementations

### QHY Cameras
- **Models Supported**: QHY5III, QHY16803, QHY42Pro, QHY268M/C, etc.
- **Special Features**:
  - Advanced USB traffic control
  - Multiple readout modes
  - Anti-amp glow technology
  - GPS synchronization (select models)
- **SDK Requirements**: QHY SDK v6.0.2+
- **Platforms**: Linux, Windows, macOS

### ZWO ASI Cameras
- **Models Supported**: ASI120, ASI183, ASI294, ASI2600, etc.
- **Special Features**:
  - High-speed USB 3.0 interface
  - Auto-exposure and auto-gain
  - Hardware ROI and binning
  - Low noise electronics
- **SDK Requirements**: ASI SDK v1.21+
- **Platforms**: Linux, Windows, macOS, ARM

### Atik Cameras
- **Models Supported**: One series, Titan, Infinity, Horizon
- **Special Features**:
  - Excellent cooling performance
  - Integrated filter wheels (select models)
  - Advanced readout modes
  - Low noise design
- **SDK Requirements**: Atik SDK v2.1+
- **Platforms**: Linux, Windows

### SBIG Cameras
- **Models Supported**: ST series, STF series, STX series
- **Special Features**:
  - Dual-chip design (main + guide)
  - Integrated filter wheels
  - Mechanical shutter
  - Anti-blooming gates
- **SDK Requirements**: SBIG Universal Driver v4.99+
- **Platforms**: Linux, Windows

### FLI Cameras
- **Models Supported**: MicroLine, ProLine, MaxCam
- **Special Features**:
  - Precision temperature control
  - Integrated filter wheels and focusers
  - Multiple gain modes
  - Professional-grade build quality
- **SDK Requirements**: FLI SDK v1.104+
- **Platforms**: Linux, Windows

### PlayerOne Cameras
- **Models Supported**: Apollo, Uranus, Neptune series
- **Special Features**:
  - Advanced sensor technology
  - Hardware pixel binning
  - Low readout noise
  - High quantum efficiency
- **SDK Requirements**: PlayerOne SDK v3.1+
- **Platforms**: Linux, Windows

## Auto-Detection Rules

The camera factory uses intelligent auto-detection based on camera names:

1. **QHY Pattern**: "qhy", "quantum" → QHY driver
2. **ASI Pattern**: "asi", "zwo" → ASI driver
3. **Atik Pattern**: "atik", "titan", "infinity" → Atik driver
4. **SBIG Pattern**: "sbig", "st-" → SBIG driver
5. **FLI Pattern**: "fli", "microline", "proline" → FLI driver
6. **PlayerOne Pattern**: "playerone", "player one", "poa" → PlayerOne driver
7. **ASCOM Pattern**: Contains "." (ProgID format) → ASCOM driver
8. **Simulator Pattern**: "simulator", "sim" → Simulator driver
9. **Default**: INDI → Native SDK → Simulator (fallback order)

## Installation Requirements

### Linux
```bash
# INDI (universal)
sudo apt install indi-full

# QHY SDK
# Download from QHY website and install

# ASI SDK
# Download from ZWO website and install

# Other SDKs
# Download from respective manufacturers
```

### Windows
```powershell
# ASCOM Platform
# Download and install ASCOM Platform

# Native SDKs
# Download from manufacturers' websites
```

### macOS
```bash
# INDI
brew install indi

# Native SDKs available from manufacturers
```

## Performance Characteristics

| Camera Type | Typical Readout | Max Frame Rate | Cooling Range | Power Draw |
|-------------|----------------|----------------|---------------|------------|
| **QHY** | 1-10 FPS | 30 FPS | -40°C | 5-12W |
| **ASI** | 10-100 FPS | 200+ FPS | -35°C | 3-8W |
| **Atik** | 1-5 FPS | 20 FPS | -45°C | 8-15W |
| **SBIG** | 0.5-2 FPS | 5 FPS | -50°C | 10-20W |
| **FLI** | 1-3 FPS | 10 FPS | -50°C | 12-25W |
| **PlayerOne** | 5-50 FPS | 100+ FPS | -35°C | 4-10W |

## Compatibility Notes

- **Thread Safety**: All implementations are fully thread-safe
- **Memory Management**: RAII-compliant with smart pointers
- **Error Handling**: Comprehensive error codes and logging
- **Platform Support**: Primary focus on Linux, with Windows/macOS support
- **SDK Versions**: Regular updates for latest SDK compatibility
- **Hot-Plug**: Support for USB device hot-plugging where supported by SDK

## Future Roadmap

### Planned Additions
- **Moravian Instruments** cameras
- **Altair Astro** cameras
- **ToupTek** cameras
- **Canon/Nikon DSLR** support via gPhoto2
- **Raspberry Pi HQ Camera** support

### Enhancements
- GPU-accelerated image processing
- Machine learning-based auto-focusing
- Advanced calibration frameworks
- Cloud storage integration
- Remote observatory support
