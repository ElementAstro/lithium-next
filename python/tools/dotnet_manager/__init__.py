"""
Enhanced .NET Framework Installer and Manager

A comprehensive utility for managing .NET Framework installations on Windows systems,
providing detection, installation, verification, and uninstallation capabilities with
modern Python features, robust error handling, and advanced logging.

This module can be used both as a command-line tool and as an API through Python
import or C++ applications via pybind11 bindings.

Features:
- Modern Python 3.9+ features with type hints and protocols
- Comprehensive error handling with structured exceptions
- Enhanced logging with loguru for better debugging
- Async/await support for downloads and I/O operations
- Progress tracking and performance metrics
- Robust checksum verification
- Platform compatibility checks
"""

from __future__ import annotations

# Import enhanced models with new exception classes
from .models import (
    DotNetVersion, HashAlgorithm, SystemInfo, DownloadResult, InstallationResult,
    DotNetManagerError, UnsupportedPlatformError, RegistryAccessError,
    DownloadError, ChecksumError, InstallationError, VersionComparable
)

# Platform-specific imports (Windows-only components)
try:
    # Import enhanced manager (Windows-specific)
    from .manager import DotNetManager
    
    # Import enhanced API functions (Windows-specific)
    from .api import (
        get_system_info,
        check_dotnet_installed,
        list_installed_dotnets,
        list_available_dotnets,
        download_file,
        download_file_async,
        verify_checksum,
        verify_checksum_async,
        install_software,
        uninstall_dotnet,
        get_latest_known_version,
        get_version_info,
        download_and_install_version
    )
    _PLATFORM_IMPORTS_AVAILABLE = True
except ImportError:
    # On non-Windows platforms, these imports will fail
    # Set placeholders that raise appropriate errors when used
    DotNetManager = None
    get_system_info = None
    check_dotnet_installed = None
    list_installed_dotnets = None
    list_available_dotnets = None
    download_file = None
    download_file_async = None
    verify_checksum = None
    verify_checksum_async = None
    install_software = None
    uninstall_dotnet = None
    get_latest_known_version = None
    get_version_info = None
    download_and_install_version = None
    _PLATFORM_IMPORTS_AVAILABLE = False

__version__ = "3.1.0"
__author__ = "Max Qian <astro_air@126.com>"

__all__ = [
    # Core Manager
    "DotNetManager",
    
    # Models and Data Classes
    "DotNetVersion",
    "HashAlgorithm", 
    "SystemInfo",
    "DownloadResult",
    "InstallationResult",
    
    # Exception Classes
    "DotNetManagerError",
    "UnsupportedPlatformError", 
    "RegistryAccessError",
    "DownloadError",
    "ChecksumError",
    "InstallationError",
    
    # Protocols
    "VersionComparable",
    
    # API Functions - System Information
    "get_system_info",
    "check_dotnet_installed",
    "list_installed_dotnets",
    "list_available_dotnets",
    "get_latest_known_version",
    "get_version_info",
    
    # API Functions - Download and Install
    "download_file",
    "download_file_async", 
    "verify_checksum",
    "verify_checksum_async",
    "install_software",
    "uninstall_dotnet",
    "download_and_install_version",
]