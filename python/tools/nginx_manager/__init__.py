#!/usr/bin/env python3
"""
Nginx Manager - A comprehensive tool for managing Nginx web server

This package provides functionality for managing Nginx installations, including:
- Installation and service management
- Configuration handling and validation
- Virtual host management
- SSL certificate management
- Log analysis and monitoring

The package supports both command-line usage and embedding via pybind11.
"""

from .core import (
    OperatingSystem,
    NginxError,
    ConfigError,
    InstallationError,
    OperationError,
    NginxPaths,
)
from .manager import NginxManager
from .bindings import NginxManagerBindings
from .logging_config import setup_logging

# Set up default logging
setup_logging()

__all__ = [
    "NginxManager",
    "NginxManagerBindings",
    "NginxError",
    "ConfigError",
    "InstallationError",
    "OperationError",
    "OperatingSystem",
    "NginxPaths",
    "setup_logging",
]
