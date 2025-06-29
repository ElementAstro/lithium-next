"""API functions for both CLI usage and pybind11 integration."""

from pathlib import Path
from typing import List, Optional

from loguru import logger

from .manager import DotNetManager


def check_dotnet_installed(version: str) -> bool:
    """
    Check if a specific .NET Framework version is installed.

    Args:
        version: The version to check (e.g., "v4.8")

    Returns:
        True if installed, False otherwise
    """
    manager = DotNetManager()
    return manager.check_installed(version)


def list_installed_dotnets() -> List[str]:
    """
    List all installed .NET Framework versions.

    Returns:
        List of installed version strings
    """
    manager = DotNetManager()
    versions = manager.list_installed_versions()
    return [str(version) for version in versions]


def download_file(
    url: str,
    filename: str,
    num_threads: int = 4,
    expected_checksum: Optional[str] = None,
) -> bool:
    """
    Download a file with optional multi-threading and checksum verification.

    Args:
        url: URL to download from
        filename: Path where the file should be saved
        num_threads: Number of download threads to use
        expected_checksum: Optional SHA256 checksum for verification

    Returns:
        True if download succeeded, False otherwise
    """
    try:
        manager = DotNetManager(threads=num_threads)
        output_path = manager.download_file(
            url, Path(filename), num_threads, expected_checksum
        )
        return output_path.exists()
    except Exception as e:
        logger.error(f"Download failed: {e}")
        return False


def install_software(installer_path: str, quiet: bool = False) -> bool:
    """
    Execute a software installer.

    Args:
        installer_path: Path to the installer executable
        quiet: Whether to run the installer silently

    Returns:
        True if installation process started successfully, False otherwise
    """
    manager = DotNetManager()
    return manager.install_software(Path(installer_path), quiet)


def uninstall_dotnet(version: str) -> bool:
    """
    Attempt to uninstall a specific .NET Framework version.

    Args:
        version: The version to uninstall (e.g., "v4.8")

    Returns:
        True if uninstallation was attempted successfully, False otherwise
    """
    manager = DotNetManager()
    return manager.uninstall_dotnet(version)
