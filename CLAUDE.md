# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Lithium-Next is an open-source astrophotography terminal for managing and automating astrophotography tasks. It provides device control (cameras, mounts, focusers, filter wheels), task sequencing, and a web-based API server.

## Build Commands

```bash
# Standard build
mkdir build && cd build
cmake ..
make

# Build with specific compiler
cmake -DCMAKE_CXX_COMPILER=g++-13 ..   # GCC 13+
cmake -DCMAKE_CXX_COMPILER=clang++ ..  # Clang 16+

# Run tests
ctest

# Package
make package
```

**Requirements**: CMake 3.20+, C++23 (falls back to C++20), GCC 13+ or Clang 16+ or MSVC 19.28+

**Dependencies**: libcfitsio, zlib, OpenSSL, libzip, libnova, libfmt, Python 3.7+, pybind11, readline, ncurses

## Running the Application

```bash
./lithium-next [options]
  -p, --port <int>       Server port (default: 8000)
  -h, --host <string>    Server host (default: 0.0.0.0)
  -c, --config <path>    Config file path
  -m, --module-path      Modules directory
  -d, --debug            Enable debug terminal
  -w, --web-panel        Enable web panel
```

## Architecture

### Core Components (`src/`)

| Module | Purpose |
|--------|---------|
| `server/` | Crow-based HTTP server, WebSocket, REST controllers |
| `task/` | Task system with sequencing, dependencies, pre/post-tasks |
| `device/` | Device abstraction (INDI cameras, mounts, focusers, filter wheels) |
| `components/` | Plugin/module management system |
| `script/` | Python integration via pybind11, shell script execution |
| `config/` | Configuration management |
| `target/` | Celestial target search and management |
| `tools/` | Astronomical calculations, coordinate conversion |
| `debug/` | Debug terminal, logging |

### Libraries (`libs/`)

- `atom/` - Core utility library (async, memory, type utilities)
- `thirdparty/` - Crow web framework, other dependencies

### Key Patterns

**Global Pointer Injection**: Uses `AddPtr<T>/GetPtr<T>` pattern for dependency injection via `atom/function/global_ptr.hpp`. Constants defined in `src/constant/constant.hpp`.

**Controller Pattern**: HTTP endpoints defined in `src/server/controller/*.hpp`, each extending the `Controller` base class with `registerRoutes()`.

**Task System**: Tasks (`src/task/task.hpp`) support:
- Parameter validation with type checking
- Dependencies between tasks
- Pre/post task chains
- Timeouts and cancellation
- Priority levels (1-10)

**Component Manager**: Dynamic module loading from `modules/` directory.

## Testing

Tests use Google Test framework:

```bash
# Run all tests
cd build && ctest

# Run specific test
./tests/task/test_task
```

Test files mirror source structure in `tests/` directory.

## Code Style

- Google C++ style (modified), enforced via `.clang-format`
- 4-space indentation, 80-char column limit
- Pre-commit hooks: clang-format, black (Python), trailing whitespace

Format code:
```bash
clang-format -i src/**/*.cpp src/**/*.hpp
```

## Key Files

- `src/app.cpp` - Main entry point, server initialization
- `src/constant/constant.hpp` - Global string constants and hashed keys
- `src/task/task.hpp` - Core task abstraction
- `src/device/manager.hpp` - Device management interface
- `src/components/manager.hpp` - Module loading system
