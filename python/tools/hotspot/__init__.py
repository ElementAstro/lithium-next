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

# Function to create a pybind11 module


def def create_pybind11_module():
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
    'logger'
]