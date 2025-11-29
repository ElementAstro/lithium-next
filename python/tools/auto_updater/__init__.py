# __init__.py
"""
Auto Updater Script

This script is designed to automatically check for, download, verify, and install updates for a given application.
It supports multi-threaded downloads, file verification, backup of current files, and logging of update history.
The module can be used both as a command-line tool and as a Python library embedded in other applications.

Features:
- Automatic update checking against remote sources
- Multi-threaded concurrent downloads with progress tracking
- Cryptographic verification using SHA256/SHA512/MD5
- Automatic backup and rollback capabilities
- Comprehensive logging and update history
- Compatible with both CLI and programmatic usage
- pybind11 integration via AutoUpdaterPyBindAdapter

Author: Max Qian
License: GPL-3.0-or-later
Date: 2025-06-09
"""

from .types import (
    UpdateStatus,
    UpdaterError,
    NetworkError,
    VerificationError,
    InstallationError,
    UpdaterConfig,
    PathLike,
    HashType,
)
from .core import AutoUpdater
from .sync import AutoUpdaterSync, create_updater, run_updater
from .utils import compare_versions, parse_version, calculate_file_hash
from .logger import logger
from .pybind_adapter import AutoUpdaterPyBindAdapter

__version__ = "1.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"

__all__ = [
    # Core classes
    "AutoUpdater",
    "AutoUpdaterSync",
    "AutoUpdaterPyBindAdapter",
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
    "get_tool_info",
    # Type definitions
    "PathLike",
    "HashType",
    # Logger
    "logger",
]


def get_tool_info() -> dict:
    """
    Return metadata about this tool for discovery by PythonWrapper.

    This function follows the Lithium-Next Python tool discovery convention,
    allowing the C++ PythonWrapper to introspect and catalog this module's
    capabilities.

    Returns:
        Dict containing tool metadata including name, version, description,
        available functions, requirements, and platform compatibility.
    """
    return {
        "name": "auto_updater",
        "version": __version__,
        "description": "Automatic software update management with verification and rollback support",
        "author": __author__,
        "license": __license__,
        "supported": True,
        "platform": ["windows", "linux", "macos"],
        "functions": [
            # Core update operations
            "check_for_updates",
            "download_update",
            "verify_update",
            "backup_current_installation",
            "extract_update",
            "install_update",
            "rollback",
            "update",
            "cleanup",
            # Utility functions
            "compare_versions",
            "parse_version",
            "calculate_file_hash",
            "create_updater",
            "run_updater",
        ],
        "requirements": [
            "requests",
            "tqdm",
            "loguru",
        ],
        "capabilities": [
            "update_checking",
            "concurrent_downloads",
            "hash_verification",
            "automatic_backup",
            "rollback_support",
            "progress_tracking",
            "pybind11_integration",
        ],
        "classes": {
            "AutoUpdater": "Main async update management interface",
            "AutoUpdaterSync": "Synchronous wrapper for pybind11 compatibility",
            "AutoUpdaterPyBindAdapter": "Simplified pybind11 interface with structured returns",
        },
    }


# Entrypoint for CLI
if __name__ == "__main__":
    import sys
    from .cli import main

    sys.exit(main())
