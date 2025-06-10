# Auto Updater

Auto Updater is a powerful, flexible tool for managing software updates in Python applications. It handles the entire update process from checking for available updates to downloading, verifying, and installing them.

## Features

- Complete Update Lifecycle: Check for updates, download updates, verify integrity, backup existing installation, and install updates
- Robust Error Handling: Comprehensive error handling with rollback capabilities
- Progress Tracking: Detailed progress reporting through callbacks
- Multi-threaded Downloads: Speed up downloads with configurable thread count
- Verification: Built-in file integrity verification using SHA-256, SHA-512, or MD5
- Backup & Restore: Automatic backup of current installation with rollback capability
- Modular Architecture: Use as a complete solution or just the components you need
- CLI and Library Interfaces: Use as a command-line tool or integrate into your application

## Installation

```bash
pip install auto_updater
```

## Basic Usage

### As a Library

```python
from auto_updater import AutoUpdater
from pathlib import Path

# Configure the updater
config = {
    "url": "https://example.com/updates.json",
    "install_dir": Path("/path/to/app"),
    "current_version": "1.2.3",
    "num_threads": 4,
    "verify_hash": True,
    "hash_algorithm": "sha256"
}

# Create updater instance
updater = AutoUpdater(config)

# Check and install updates if available
if updater.check_for_updates():
    print("Update found!")
    
    # Complete update process - download, verify, backup, and install
    if updater.update():
        print(f"Successfully updated to version {updater.update_info['version']}")
else:
    print("No updates available")
```

### As a Command-Line Tool

```bash
# Basic update
auto-updater --config config.json

# Check for updates but don't install
auto-updater --config config.json --check-only

# Download updates but don't install
auto-updater --config config.json --download-only

# Rollback to a previous backup
auto-updater --config config.json --rollback /path/to/backup/directory
```

## Configuration

The updater accepts the following configuration options:

```json
{
    "url": "https://example.com/updates.json",
    "install_dir": "/path/to/app",
    "current_version": "1.2.3",
    "num_threads": 4,
    "verify_hash": true,
    "hash_algorithm": "sha256",
    "temp_dir": "/path/to/temp",
    "backup_dir": "/path/to/backup"
}
```

| Option            | Description                                  | Required | Default                |
| ----------------- | -------------------------------------------- | -------- | ---------------------- |
| `url`             | URL for update information JSON              | Yes      | -                      |
| `install_dir`     | Directory where the application is installed | Yes      | -                      |
| `current_version` | Current application version                  | Yes      | -                      |
| `num_threads`     | Number of threads for downloading            | No       | 4                      |
| `verify_hash`     | Whether to verify file hashes                | No       | `true`                 |
| `hash_algorithm`  | Hash algorithm (`sha256`, `sha512`, `md5`)   | No       | `sha256`               |
| `temp_dir`        | Directory for temporary files                | No       | `{install_dir}/temp`   |
| `backup_dir`      | Directory for backups                        | No       | `{install_dir}/backup` |
| `custom_params`   | Custom parameters for extensions             | No       | `{}`                   |

## Server-side Update JSON

The server should provide update information in the following JSON format:

```json
{
    "version": "1.3.0",
    "download_url": "https://example.com/downloads/app-1.3.0.zip",
    "file_hash": "a1b2c3d4e5f6...",
    "release_notes": "Fixed bugs and improved performance",
    "release_date": "2025-06-01",
    "required": false
}
```

## Advanced Usage

### Progress Callbacks

```python
from auto_updater import AutoUpdater, UpdateStatus

def progress_callback(status, progress, message):
    """
    Handle progress updates.
    
    Args:
        status (UpdateStatus): Current update status
        progress (float): Progress value (0.0 to 1.0)
        message (str): Status message
    """
    print(f"[{status.value}] {progress:.1%} - {message}")

# Create updater with progress reporting
updater = AutoUpdater(
    config=config,
    progress_callback=progress_callback
)

# Run update
updater.update()
```

### Customizing the Update Process

```python
from auto_updater import AutoUpdater

updater = AutoUpdater(config)

# Step-by-step update process
if updater.check_for_updates():
    # Download the update package
    download_path = updater.download_update()
    
    # Verify the downloaded package
    if updater.verify_update(download_path):
        # Backup current installation
        backup_dir = updater.backup_current_installation()
        
        try:
            # Extract the update package
            extract_dir = updater.extract_update(download_path)
            
            # Install the update
            if updater.install_update(extract_dir):
                print("Update installed successfully!")
        except Exception as e:
            print(f"Update failed: {e}")
            # Roll back to previous version
            updater.rollback(backup_dir)
```

### Working with the Synchronous Wrapper

For compatibility with C++ via pybind11 or similar:

```python
from auto_updater import AutoUpdaterSync

# Create synchronous updater
updater = AutoUpdaterSync(config)

# Check for updates
if updater.check_for_updates():
    # Get path to downloaded file as string
    download_path = updater.download_update()
    
    # Use regular strings for paths
    extract_dir = updater.extract_update(download_path)
    updater.install_update(extract_dir)
```

### Using the Logger

```python
from auto_updater import logger

# Configure logger
logger.remove()  # Remove default handler
logger.add("updates.log", rotation="10 MB")  # Log to file with rotation
logger.add(lambda msg: print(msg, end=""), level="INFO")  # Print to console

# Now use the updater normally
from auto_updater import AutoUpdater
updater = AutoUpdater(config)
updater.update()
```

## CLI Options

```
usage: auto-updater [-h] --config CONFIG [--check-only] [--download-only]
                   [--verify-only] [--rollback ROLLBACK] [--verbose]

Auto Updater - Check for and install software updates

options:
  -h, --help           show this help message and exit
  --config CONFIG      Path to the configuration file (JSON)
  --check-only         Only check for updates, don't download or install
  --download-only      Download but don't install updates
  --verify-only        Download and verify but don't install updates
  --rollback ROLLBACK  Path to backup directory to rollback to
  --verbose, -v        Increase verbosity (can be used multiple times)
```

## Development

### Setting Up the Development Environment

```bash
# Clone the repository
git clone https://github.com/example/auto_updater.git
cd auto_updater

# Create a virtual environment
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install development dependencies
pip install -e ".[dev]"

# Run tests
pytest
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request