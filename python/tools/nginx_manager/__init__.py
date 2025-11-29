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

import platform
from .core import (
    OperatingSystem,
    NginxError,
    ConfigError,
    InstallationError,
    OperationError,
    NginxPaths,
)
from .manager import NginxManager
from .bindings import NginxManagerBindings, NginxPyBindAdapter
from .logging_config import setup_logging

# Module metadata
__version__ = "1.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"
__description__ = "Comprehensive Nginx server management tool with installation, configuration, SSL, and monitoring capabilities"

# Set up default logging
setup_logging()

__all__ = [
    "NginxManager",
    "NginxManagerBindings",
    "NginxPyBindAdapter",
    "NginxError",
    "ConfigError",
    "InstallationError",
    "OperationError",
    "OperatingSystem",
    "NginxPaths",
    "setup_logging",
    "get_tool_info",
]


def _check_platform_support() -> bool:
    """
    Check if the current platform is supported.

    Returns:
        True if platform is Linux or macOS, False otherwise
    """
    system = platform.system().lower()
    return system in ("linux", "darwin")


def get_tool_info() -> dict:
    """
    Get module metadata and capabilities information.

    Returns:
        Dictionary containing module information including name, version,
        description, author, license, supported platforms, functions,
        requirements, capabilities, and classes
    """
    return {
        "name": "nginx_manager",
        "version": __version__,
        "description": __description__,
        "author": __author__,
        "license": __license__,
        "supported": _check_platform_support(),
        "platform": ["linux", "macos"],
        "functions": [
            "install_nginx",
            "start_nginx",
            "stop_nginx",
            "reload_nginx",
            "restart_nginx",
            "check_config",
            "get_status",
            "get_version",
            "is_nginx_installed",
            "backup_config",
            "restore_config",
            "list_backups",
            "create_virtual_host",
            "enable_virtual_host",
            "disable_virtual_host",
            "list_virtual_hosts",
            "analyze_logs",
            "generate_ssl_cert",
            "configure_ssl",
            "health_check",
        ],
        "requirements": ["loguru>=0.7.0", "pybind11>=2.6.0"],
        "capabilities": [
            "nginx_installation",
            "service_management",
            "configuration_validation",
            "virtual_host_management",
            "ssl_certificate_handling",
            "log_analysis",
            "health_monitoring",
            "configuration_backup_restore",
            "cross_platform_support",
        ],
        "classes": {
            "NginxManager": "Main class for Nginx operations",
            "NginxPyBindAdapter": "PyBind11 adapter with static methods for C++ integration",
            "NginxManagerBindings": "Instance-based bindings for C++ pybind11 integration",
            "OperatingSystem": "Enum for supported operating systems",
            "NginxError": "Base exception class",
            "ConfigError": "Configuration-related exceptions",
            "InstallationError": "Installation-related exceptions",
            "OperationError": "Operation-related exceptions",
            "NginxPaths": "Dataclass containing Nginx installation paths",
        },
    }
