"""
.NET Framework Installer and Manager

A comprehensive utility for managing .NET Framework installations on Windows systems,
providing detection, installation, verification, and uninstallation capabilities.

This module can be used both as a command-line tool and as an API through Python
import or C++ applications via pybind11 bindings.
"""

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

__all__ = [
    "DotNetManager",
    "DotNetVersion",
    "HashAlgorithm",
    "check_dotnet_installed",
    "list_installed_dotnets",
    "download_file",
    "install_software",
    "uninstall_dotnet",
]
