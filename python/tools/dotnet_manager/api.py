"""API functions for CLI usage, pybind11 integration, and general programmatic use."""

import asyncio
from pathlib import Path
from typing import List, Optional

from loguru import logger

from .manager import DotNetManager
from .models import DotNetVersion, SystemInfo, DownloadResult, HashAlgorithm


def get_system_info() -> SystemInfo:
    """
    Get detailed information about the system and installed .NET versions.

    Returns:
        SystemInfo object with OS and .NET details.
    """
    manager = DotNetManager()
    return manager.get_system_info()


def check_dotnet_installed(version_key: str) -> bool:
    """
    Check if a specific .NET Framework version is installed.

    Args:
        version_key: The version key to check (e.g., "v4.8").

    Returns:
        True if installed, False otherwise.
    """
    manager = DotNetManager()
    return manager.check_installed(version_key)


def list_installed_dotnets() -> List[DotNetVersion]:
    """
    List all installed .NET Framework versions.

    Returns:
        A list of DotNetVersion objects.
    """
    manager = DotNetManager()
    return manager.list_installed_versions()


async def download_file_async(
    url: str,
    output_path: str,
    checksum: Optional[str] = None,
    show_progress: bool = True
) -> DownloadResult:
    """
    Asynchronously download a file with optional checksum verification.

    Args:
        url: URL to download from.
        output_path: Path where the file should be saved.
        checksum: Optional SHA256 checksum for verification.
        show_progress: Whether to display a progress bar.

    Returns:
        DownloadResult object with details of the download.
    """
    manager = DotNetManager()
    path = Path(output_path)
    try:
        downloaded_path = await manager.download_file_async(url, path, checksum, show_progress)
        checksum_matched = None
        if checksum:
            checksum_matched = await manager.verify_checksum_async(downloaded_path, checksum)
        
        return DownloadResult(
            path=str(downloaded_path),
            size=downloaded_path.stat().st_size,
            checksum_matched=checksum_matched
        )
    except Exception as e:
        logger.error(f"Download failed: {e}")
        raise


async def verify_checksum_async(
    file_path: str,
    expected_checksum: str,
    algorithm: HashAlgorithm = HashAlgorithm.SHA256
) -> bool:
    """
    Asynchronously verify a file's checksum.

    Args:
        file_path: Path to the file.
        expected_checksum: The expected checksum hash.
        algorithm: The hash algorithm to use.

    Returns:
        True if the checksum matches, False otherwise.
    """
    manager = DotNetManager()
    return await manager.verify_checksum_async(Path(file_path), expected_checksum, algorithm)


def install_software(installer_path: str, quiet: bool = True) -> bool:
    """
    Execute a software installer.

    Args:
        installer_path: Path to the installer executable.
        quiet: Whether to run the installer silently.

    Returns:
        True if the installation process started successfully, False otherwise.
    """
    manager = DotNetManager()
    return manager.install_software(Path(installer_path), quiet)


def uninstall_dotnet(version_key: str) -> bool:
    """
    Attempt to uninstall a specific .NET Framework version.
    (Note: This is generally not recommended or possible for system components).

    Args:
        version_key: The version to uninstall (e.g., "v4.8").

    Returns:
        True if uninstallation was attempted, False otherwise.
    """
    manager = DotNetManager()
    return manager.uninstall_dotnet(version_key)


def get_latest_known_version() -> Optional[DotNetVersion]:
    """
    Get the latest .NET version known to the manager.

    Returns:
        A DotNetVersion object for the latest known version, or None.
    """
    manager = DotNetManager()
    return manager.get_latest_known_version()

# Synchronous wrapper for download for simpler use cases
def download_file(
    url: str,
    output_path: str,
    checksum: Optional[str] = None,
    show_progress: bool = True
) -> DownloadResult:
    """
    Synchronously download a file. Wraps the async version.

    Args:
        url: URL to download from.
        output_path: Path where the file should be saved.
        checksum: Optional SHA256 checksum for verification.
        show_progress: Whether to display a progress bar.

    Returns:
        DownloadResult object with details of the download.
    """
    try:
        return asyncio.run(download_file_async(url, output_path, checksum, show_progress))
    except Exception as e:
        logger.error(f"Download failed: {e}")
        raise