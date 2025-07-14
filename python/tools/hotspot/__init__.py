#!/usr/bin/env python3
"""
Enhanced WiFi Hotspot Manager with Modern Python Features

A comprehensive utility for managing WiFi hotspots on Linux systems using NetworkManager.
Supports both command-line usage and programmatic API calls through Python or C++ (via pybind11).

Features:
- Create and manage WiFi hotspots with various authentication options
- Monitor connected clients with real-time updates
- Save and load hotspot configurations with validation
- Extensive error handling and structured logging with loguru
- Async-first architecture for better performance
- Rich CLI with enhanced output formatting
- Plugin architecture for extensibility
- Type safety with comprehensive validation
"""

from __future__ import annotations

from loguru import logger

# Core models and enums
from .models import (
    AuthenticationType,
    BandType,
    CommandResult,
    ConnectedClient,
    EncryptionType,
    HotspotConfig,
    HotspotException,
    ConfigurationError,
    NetworkManagerError,
    InterfaceError,
    NetworkInterface,
)

# Command utilities
from .command_utils import (
    run_command,
    run_command_async,
    run_command_with_retry,
    stream_command_output,
    get_command_runner_stats,
    EnhancedCommandRunner,
    CommandExecutionError,
    CommandTimeoutError,
    CommandNotFoundError,
)

# Core manager
from .hotspot_manager import HotspotManager, HotspotPlugin

# Version information
__version__ = "2.0.0"
__author__ = "WiFi Hotspot Manager Team"
__email__ = "info@example.com"
__license__ = "MIT"


def create_pybind11_module() -> dict[str, type]:
    """
    Create the core functions and classes for pybind11 integration.

    This function provides a mapping of classes and functions that can be
    exposed to C++ code via pybind11 for high-performance integrations.

    Returns:
        A dictionary containing the classes and functions to expose via pybind11

    Example:
        >>> bindings = create_pybind11_module()
        >>> manager_class = bindings["HotspotManager"]
        >>> config_class = bindings["HotspotConfig"]
    """
    return {
        "HotspotManager": HotspotManager,
        "HotspotConfig": HotspotConfig,
        "AuthenticationType": AuthenticationType,
        "EncryptionType": EncryptionType,
        "BandType": BandType,
        "CommandResult": CommandResult,
        "ConnectedClient": ConnectedClient,
        "EnhancedCommandRunner": EnhancedCommandRunner,
    }


def get_version_info() -> dict[str, str]:
    """
    Get version and package information.

    Returns:
        Dictionary containing version and metadata information
    """
    return {
        "version": __version__,
        "author": __author__,
        "email": __email__,
        "license": __license__,
        "description": "Enhanced WiFi Hotspot Manager with Modern Python Features",
    }


# Configure default logger for the package
logger.disable("hotspot")  # Disable by default, let applications configure


# Public API exports
__all__ = [
    # Core classes
    "HotspotManager",
    "HotspotConfig",
    "HotspotPlugin",
    # Data models
    "ConnectedClient",
    "CommandResult",
    "NetworkInterface",
    # Enums
    "AuthenticationType",
    "EncryptionType",
    "BandType",
    # Exceptions
    "HotspotException",
    "ConfigurationError",
    "NetworkManagerError",
    "InterfaceError",
    "CommandExecutionError",
    "CommandTimeoutError",
    "CommandNotFoundError",
    # Command utilities
    "run_command",
    "run_command_async",
    "run_command_with_retry",
    "stream_command_output",
    "get_command_runner_stats",
    "EnhancedCommandRunner",
    # Utility functions
    "create_pybind11_module",
    "get_version_info",
    # Package metadata
    "__version__",
    "logger",
]
