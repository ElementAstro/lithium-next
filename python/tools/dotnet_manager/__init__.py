"""
.NET Framework Installer and Manager

A comprehensive utility for managing .NET Framework installations on Windows systems,
providing detection, installation, verification, and uninstallation capabilities.

This module can be used both as a command-line tool and as an API through Python
import or C++ applications via pybind11 bindings.
"""

import platform
from .models import DotNetVersion, HashAlgorithm
from .manager import DotNetManager
from .api import (
    check_dotnet_installed,
    list_installed_dotnets,
    download_file,
    install_software,
    uninstall_dotnet,
)

__version__ = "2.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"


def _check_platform_support() -> bool:
    """Check if the current platform is supported (Windows only)."""
    return platform.system() == "Windows"


def get_tool_info() -> dict:
    """
    Get metadata information about the dotnet_manager module.

    Returns:
        dict: A dictionary containing module metadata including name, version,
              description, author, license, platform support, available functions,
              requirements, and capabilities.
    """
    return {
        "name": "dotnet_manager",
        "version": __version__,
        "description": "Comprehensive .NET Framework manager for Windows - supports detection, installation, verification, and management of .NET Framework versions",
        "author": __author__,
        "license": __license__,
        "supported": _check_platform_support(),
        "platform": ["windows"],
        "functions": [
            "check_dotnet_installed",
            "list_installed_dotnets",
            "download_file",
            "install_software",
            "uninstall_dotnet",
            "get_tool_info",
        ],
        "requirements": ["requests", "tqdm", "loguru", "Windows OS"],
        "capabilities": [
            "Detect installed .NET Framework versions via registry",
            "List all installed .NET Framework versions",
            "Download .NET Framework installers with multi-threading",
            "Verify file integrity with SHA256/SHA1/MD5 checksums",
            "Execute installers silently or with UI",
            "Attempt to uninstall .NET Framework versions",
            "Support for .NET Framework 4.5+ versions",
        ],
        "classes": {
            "DotNetManager": "Core manager class for .NET Framework operations",
            "DotNetVersion": "Data class representing a .NET Framework version",
            "HashAlgorithm": "Enum for supported hash algorithms (MD5, SHA1, SHA256, SHA512)",
        },
    }


__all__ = [
    "DotNetManager",
    "DotNetVersion",
    "HashAlgorithm",
    "check_dotnet_installed",
    "list_installed_dotnets",
    "download_file",
    "install_software",
    "uninstall_dotnet",
    "get_tool_info",
]
