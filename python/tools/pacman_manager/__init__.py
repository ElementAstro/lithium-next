#!/usr/bin/env python3
"""
Pacman Package Manager - Python Interface

A comprehensive Python interface for the Pacman package manager,
supporting both ArchLinux (native) and Windows (MSYS2) environments.

Features:
- Package installation, removal, and updates
- Package search and information queries
- Repository management
- AUR helper integration (yay, paru, etc.)
- Async and sync operation modes
- Cross-platform support (Windows MSYS2 + Linux)

Author:
    Max Qian <lightapt.com>

License:
    GPL-3.0-or-later

Version:
    1.0.0
"""

import platform
from typing import Dict, Any, List

from .manager import PacmanManager
from .models import PackageInfo, CommandResult, PackageStatus
from .config import PacmanConfig
from .exceptions import (
    PacmanError,
    CommandError,
    PackageNotFoundError,
    ConfigError
)
from .pybind_integration import PacmanPyBindAdapter

__version__ = "1.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"

__all__ = [
    # Core classes
    "PacmanManager",
    "PacmanConfig",

    # Data models
    "PackageInfo",
    "CommandResult",
    "PackageStatus",

    # Exceptions
    "PacmanError",
    "CommandError",
    "PackageNotFoundError",
    "ConfigError",

    # pybind adapter
    "PacmanPyBindAdapter",

    # Discovery function
    "get_tool_info",
]


def _check_platform_support() -> bool:
    """Check if the current platform supports pacman."""
    system = platform.system().lower()
    return system in ["linux", "windows"]


def get_tool_info() -> Dict[str, Any]:
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
        "name": "pacman_manager",
        "version": __version__,
        "description": "ArchLinux/MSYS2 Pacman package manager interface",
        "author": __author__,
        "license": __license__,
        "supported": _check_platform_support(),
        "platform": ["windows", "linux"],
        "functions": [
            # Package operations
            "install_package",
            "install_packages",
            "remove_package",
            "upgrade_system",
            "update_database",
            # Search and info
            "search_package",
            "show_package_info",
            "list_installed_packages",
            "list_outdated_packages",
            "list_package_files",
            # Maintenance
            "clear_cache",
            "clean_orphaned_packages",
            "check_package_problems",
            # AUR support
            "install_aur_package",
            "search_aur_package",
            # Configuration
            "enable_repo",
            "get_enabled_repos",
        ],
        "requirements": ["loguru"],
        "capabilities": [
            "async_operations",
            "aur_support",
            "cross_platform",
            "package_search",
            "dependency_resolution",
            "cache_management",
        ],
        "classes": {
            "PacmanManager": "Main package manager interface",
            "PacmanConfig": "Configuration file management",
            "PacmanPyBindAdapter": "Simplified pybind11 interface",
        }
    }


# Module-level convenience functions for direct calling
def install_package(package: str, **kwargs) -> Dict[str, Any]:
    """
    Install a package using pacman.

    Convenience function for C++ pybind11 calling without instantiation.

    Args:
        package: Package name to install
        **kwargs: Additional arguments passed to PacmanManager

    Returns:
        Dict with success status and message
    """
    return PacmanPyBindAdapter.install_package(package)


def search_package(query: str, **kwargs) -> List[Dict[str, Any]]:
    """
    Search for packages matching a query.

    Convenience function for C++ pybind11 calling without instantiation.

    Args:
        query: Search query string
        **kwargs: Additional arguments

    Returns:
        List of matching package information dicts
    """
    return PacmanPyBindAdapter.search_package(query)


def list_installed_packages() -> List[Dict[str, Any]]:
    """
    List all installed packages.

    Convenience function for C++ pybind11 calling without instantiation.

    Returns:
        List of installed package information dicts
    """
    return PacmanPyBindAdapter.list_installed_packages()
