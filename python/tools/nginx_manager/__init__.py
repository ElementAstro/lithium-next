#!/usr/bin/env python3
"""
Nginx Manager - A comprehensive, async-first tool for managing Nginx.

This package provides an extensible, asynchronous framework for managing Nginx,
including service management, configuration, virtual hosts, and more.
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

# Set up default logging for the package
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
