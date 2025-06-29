
# Advanced Pacman Package Manager

A comprehensive Python interface to the pacman package manager, supporting both CLI usage and programmatic integration. Designed to handle cross-platform execution on both Linux and Windows (MSYS2) environments with robust error handling and extensive functionality.

## Features

- Cross-Platform Support: Works on both Linux and Windows (MSYS2)
- Async Operations: Support for async operations for long-running commands
- Rich Error Handling: Detailed error reporting with structured exceptions
- Type Annotations: Fully type-annotated for better IDE support
- Comprehensive API: Extensive package management functionality
- AUR Support: Integration with popular AUR helpers
- Configuration Management: Interface for managing pacman configuration
- Robust Logging: Structured logging with loguru
- C++ Integration: pybind11 support for embedding in C++ applications

## Installation

### From PyPI

```bash
pip install advanced-pacman-manager
```

### From Source

```bash
git clone https://github.com/yourusername/advanced-pacman-manager.git
cd advanced-pacman-manager
pip install -e .
```

## Requirements

- Python 3.6+
- pacman package manager
- loguru (automatically installed)
- pybind11 (optional, for C++ integration)

## Basic Usage

### As a Python Library

```python
from pacman_manager import PacmanManager

# Initialize the manager
pacman = PacmanManager()

# Update package database
result = pacman.update_package_database()
if result["success"]:
    print("Database updated successfully!")

# Search for packages
packages = pacman.search_package("python")
for pkg in packages:
    print(f"{pkg.name} ({pkg.version}): {pkg.description}")

# Install a package
result = pacman.install_package("python-pip", no_confirm=True)
if result["success"]:
    print("Package installed successfully!")
```

### As a Command-Line Tool

```bash
# Update the package database
python -m pacman_manager --update-db

# Install a package
python -m pacman_manager --install python-pip --no-confirm

# Search for packages
python -m pacman_manager --search python

# List installed packages
python -m pacman_manager --list-installed
```

## Advanced Features

### Asynchronous Operations

```python
import asyncio
from pacman_manager import PacmanManager

async def update_and_upgrade():
    pacman = PacmanManager()

    # Update database
    result = await pacman.update_package_database_async()
    print("Database updated:", result["success"])

    # Upgrade system
    result = await pacman.upgrade_system_async(no_confirm=True)
    print("System upgraded:", result["success"])

# Run the async function
asyncio.run(update_and_upgrade())
```

### AUR Support

```python
from pacman_manager import PacmanManager

pacman = PacmanManager()

# Check if AUR helper is available
if pacman.has_aur_support():
    print(f"Using AUR helper: {pacman.aur_helper}")

    # Search AUR packages
    packages = pacman.search_aur_package("yay-bin")
    for pkg in packages:
        print(f"{pkg.name} ({pkg.version}): {pkg.description}")

    # Install AUR package
    result = pacman.install_aur_package("yay-bin")
```

### System Maintenance

```python
from pacman_manager import PacmanManager

pacman = PacmanManager()

# Check for package problems
problems = pacman.check_package_problems()
print(f"Found {len(problems['orphaned'])} orphaned packages")
print(f"Found {len(problems['broken_deps'])} broken dependencies")

# Clean orphaned packages
if problems['orphaned']:
    result = pacman.clean_orphaned_packages(no_confirm=True)
    print("Orphaned packages cleaned:", result["success"])

# Export package list for backup
pacman.export_package_list("/path/to/packages.txt", include_foreign=True)
```

## CLI Reference

The command-line interface supports all operations available in the Python API:

```
usage: pacman_manager [-h] [--update-db] [--upgrade] [--install PACKAGE]
                     [--install-multiple PACKAGE [PACKAGE ...]] [--remove PACKAGE]
                     [--remove-deps] [--search QUERY] [--list-installed] [--refresh]
                     [--package-info PACKAGE] [--list-outdated] [--clear-cache]
                     [--keep-recent] [--list-files PACKAGE]
                     [--show-dependencies PACKAGE] [--find-file-owner FILE]
                     [--fast-mirrors] [--downgrade PACKAGE VERSION]
                     [--list-cache] [--multithread THREADS] [--list-group GROUP]
                     [--optional-deps PACKAGE] [--enable-color] [--disable-color]
                     [--aur-install PACKAGE] [--aur-search QUERY]
                     [--check-problems] [--clean-orphaned] [--export-packages FILE]
                     [--include-foreign] [--import-packages FILE] [--no-confirm]
                     [--generate-pybind FILE] [--json] [--version]

Advanced Pacman Package Manager CLI Tool
```

## Configuration

By default, the package uses the system's pacman configuration file (`/etc/pacman.conf` on Linux, or the MSYS2 equivalent on Windows). You can customize the configuration:

```python
from pathlib import Path
from pacman_manager import PacmanManager, PacmanConfig

# Custom configuration path
config = PacmanConfig(Path("/path/to/custom/pacman.conf"))

# Create manager with custom config
pacman = PacmanManager(config_path=Path("/path/to/custom/pacman.conf"))

# Enable color output
pacman.enable_color_output(True)

# Enable multithreaded downloads
pacman.enable_multithreaded_downloads(threads=5)
```

## C++ Integration with pybind11

The library can be integrated into C++ applications through pybind11:

```python
from pacman_manager import Pybind11Integration

# Generate bindings
bindings_code = Pybind11Integration.generate_bindings()

# Save to file
with open("pacman_bindings.cpp", "w") as f:
    f.write(bindings_code)

# Print build instructions
print(Pybind11Integration.build_extension_instructions())
```

## Logging

The library uses loguru for structured logging. You can configure the logger to your needs:

```python
from loguru import logger
import sys

# Remove default handler
logger.remove()

# Add custom handler
logger.add(
    sys.stderr,
    format="{time:YYYY-MM-DD HH:mm:ss} | {level: <8} | {message}",
    level="DEBUG"  # Set to "INFO", "WARNING", etc. as needed
)

# Add file logging
logger.add("pacman_manager.log", rotation="10 MB")
```

## Development

### Running Tests

```bash
pip install pytest
pytest
```

### Building Documentation

```bash
pip install sphinx
cd docs
make html
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
