# __init__.py
"""
Auto Updater Script

This script is designed to automatically check for, download, verify, and install updates for a given application.
It supports multi-threaded downloads, file verification, backup of current files, and logging of update history.
The module can be used both as a command-line tool and as a Python library embedded in other applications.

Author: AI Assistant
Date: 2025-06-09
"""

from .types import (
    UpdateStatus, UpdaterError, NetworkError, VerificationError, InstallationError,
    UpdaterConfig, PathLike, HashType
)
from .core import AutoUpdater
from .sync import AutoUpdaterSync, create_updater, run_updater
from .utils import compare_versions, parse_version, calculate_file_hash
from .logger import logger

__version__ = "1.0.0"

__all__ = [
    # Core classes
    "AutoUpdater",
    "AutoUpdaterSync",

    # Types
    "UpdaterConfig",
    "UpdateStatus",

    # Exceptions
    "UpdaterError",
    "NetworkError",
    "VerificationError",
    "InstallationError",

    # Utility functions
    "compare_versions",
    "parse_version",
    "calculate_file_hash",
    "create_updater",
    "run_updater",

    # Type definitions
    "PathLike",
    "HashType",

    # Logger
    "logger",
]

# Entrypoint for CLI
if __name__ == "__main__":
    import sys
    from .cli import main
    sys.exit(main())
