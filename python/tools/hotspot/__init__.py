#!/usr/bin/env python3
"""
WiFi Hotspot Manager

A comprehensive utility for managing WiFi hotspots on Linux systems using NetworkManager.
Supports both command-line usage and programmatic API calls through Python or C++ (via pybind11).

Features:
- Create and manage WiFi hotspots with various authentication options
- Monitor connected clients
- Save and load hotspot configurations
- Extensive error handling and logging
"""

import sys
import platform

from loguru import logger

from .models import (
    AuthenticationType,
    EncryptionType,
    BandType,
    HotspotConfig,
    CommandResult,
    ConnectedClient
)

from .command_utils import run_command, run_command_async
from .hotspot_manager import HotspotManager

# Module metadata
__version__ = "1.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"


def _check_platform_support() -> bool:
    """
    Check if the current platform supports hotspot functionality.

    Returns:
        True if running on Linux, False otherwise
    """
    return sys.platform.startswith("linux")


def get_tool_info() -> dict:
    """
    Get comprehensive metadata about the hotspot tool module.

    Returns:
        Dictionary containing module information including name, version,
        description, platform support, available functions, and capabilities
    """
    return {
        "name": "hotspot",
        "version": __version__,
        "description": "WiFi hotspot manager for Linux systems using NetworkManager",
        "author": __author__,
        "license": __license__,
        "supported": _check_platform_support(),
        "platform": ["linux"],
        "functions": [
            "start",
            "stop",
            "restart",
            "get_status",
            "get_connected_clients",
            "get_network_interfaces",
            "get_available_channels",
            "set",
            "save_config",
            "load_config",
            "monitor_clients"
        ],
        "requirements": [
            "NetworkManager",
            "nmcli",
            "iw",
            "arp"
        ],
        "capabilities": [
            "hotspot_creation",
            "client_monitoring",
            "configuration_persistence",
            "dynamic_channel_selection",
            "multi_band_support",
            "wpa_wpa2_wpa3_support"
        ],
        "classes": {
            "HotspotManager": "Main interface for managing WiFi hotspots",
            "HotspotConfig": "Configuration dataclass for hotspot settings",
            "AuthenticationType": "Enum for supported authentication methods",
            "EncryptionType": "Enum for supported encryption algorithms",
            "BandType": "Enum for WiFi frequency bands",
            "CommandResult": "Result object from command execution",
            "ConnectedClient": "Information about a connected client"
        }
    }


def create_pybind11_module():
    """
    Create the core functions and classes for pybind11 integration.

    Returns:
        A dictionary containing the classes and functions to expose via pybind11
    """
    return {
        "HotspotManager": HotspotManager,
        "HotspotConfig": HotspotConfig,
        "AuthenticationType": AuthenticationType,
        "EncryptionType": EncryptionType,
        "BandType": BandType,
        "CommandResult": CommandResult,
    }


__all__ = [
    'HotspotManager',
    'HotspotConfig',
    'AuthenticationType',
    'EncryptionType',
    'BandType',
    'CommandResult',
    'ConnectedClient',
    'create_pybind11_module',
    'get_tool_info',
    'logger'
]
