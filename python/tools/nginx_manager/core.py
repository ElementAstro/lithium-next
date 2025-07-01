#!/usr/bin/env python3
"""
Core classes and definitions for Nginx Manager.
"""

from enum import Enum
from dataclasses import dataclass
from pathlib import Path


class OperatingSystem(Enum):
    """Enum representing supported operating systems."""
    LINUX = "linux"
    WINDOWS = "windows"
    MACOS = "darwin"
    UNKNOWN = "unknown"


class NginxError(Exception):
    """Base exception class for all Nginx-related errors."""
    pass


class ConfigError(NginxError):
    """Exception raised for Nginx configuration errors."""
    pass


class InstallationError(NginxError):
    """Exception raised for Nginx installation errors."""
    pass


class OperationError(NginxError):
    """Exception raised for failed Nginx operations."""
    pass


@dataclass
class NginxPaths:
    """Class holding paths related to Nginx installation."""
    base_path: Path
    conf_path: Path
    binary_path: Path
    backup_path: Path
    sites_available: Path
    sites_enabled: Path
    logs_path: Path
    ssl_path: Path