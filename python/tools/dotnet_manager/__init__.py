"""
.NET Framework Installer and Manager

A comprehensive utility for managing .NET Framework installations on Windows systems,
providing detection, installation, verification, and uninstallation capabilities.

This module can be used both as a command-line tool and as an API through Python
import or C++ applications via pybind11 bindings.
"""

from .models import DotNetVersion, HashAlgorithm, SystemInfo, DownloadResult
from .manager import DotNetManager
from .api import (
    get_system_info,
    check_dotnet_installed,
    list_installed_dotnets,
    download_file,
    download_file_async,
    verify_checksum_async,
    install_software,
    uninstall_dotnet,
    get_latest_known_version
)

__version__ = "3.0.0"

__all__ = [
    "DotNetManager",
    "DotNetVersion",
    "HashAlgorithm",
    "SystemInfo",
    "DownloadResult",
    "get_system_info",
    "check_dotnet_installed",
    "list_installed_dotnets",
    "download_file",
    "download_file_async",
    "verify_checksum_async",
    "install_software",
    "uninstall_dotnet",
    "get_latest_known_version"
]