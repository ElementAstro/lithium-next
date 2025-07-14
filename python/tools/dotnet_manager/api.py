"""Enhanced API functions for CLI usage, pybind11 integration, and general programmatic use."""

from __future__ import annotations
import asyncio
import time
from functools import wraps
from pathlib import Path
from typing import Optional, Callable, Any

from loguru import logger

from .manager import DotNetManager
from .models import (
    DotNetVersion, SystemInfo, DownloadResult, HashAlgorithm, InstallationResult,
    DotNetManagerError, UnsupportedPlatformError
)


def handle_platform_compatibility(func: Callable[..., Any]) -> Callable[..., Any]:
    """Decorator to handle platform compatibility checks."""
    @wraps(func)
    def wrapper(*args: Any, **kwargs: Any) -> Any:
        try:
            return func(*args, **kwargs)
        except UnsupportedPlatformError as e:
            logger.error(f"Platform compatibility error: {e}")
            raise
        except Exception as e:
            logger.error(f"Unexpected error in {func.__name__}: {e}")
            raise DotNetManagerError(
                f"API function {func.__name__} failed",
                error_code="API_ERROR",
                original_error=e
            ) from e
    
    return wrapper


def handle_async_platform_compatibility(func: Callable[..., Any]) -> Callable[..., Any]:
    """Decorator to handle platform compatibility checks for async functions."""
    @wraps(func)
    async def wrapper(*args: Any, **kwargs: Any) -> Any:
        try:
            return await func(*args, **kwargs)
        except UnsupportedPlatformError as e:
            logger.error(f"Platform compatibility error: {e}")
            raise
        except Exception as e:
            logger.error(f"Unexpected error in {func.__name__}: {e}")
            raise DotNetManagerError(
                f"API function {func.__name__} failed",
                error_code="API_ERROR",
                original_error=e
            ) from e
    
    return wrapper


@handle_platform_compatibility
def get_system_info() -> SystemInfo:
    """
    Get comprehensive information about the system and installed .NET versions.

    Returns:
        SystemInfo object with OS and .NET details
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If system information cannot be gathered
    """
    logger.debug("API: Getting system information")
    
    manager = DotNetManager()
    system_info = manager.get_system_info()
    
    logger.info(
        f"API: System info retrieved - {system_info.installed_version_count} .NET versions",
        extra={
            "platform_compatible": system_info.platform_compatible,
            "architecture": system_info.architecture
        }
    )
    
    return system_info


@handle_platform_compatibility
def check_dotnet_installed(version_key: str) -> bool:
    """
    Check if a specific .NET Framework version is installed.

    Args:
        version_key: The version key to check (e.g., "v4.8")

    Returns:
        True if installed, False otherwise
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If version key is invalid or check fails
    """
    logger.debug(f"API: Checking if .NET version is installed: {version_key}")
    
    if not version_key or not isinstance(version_key, str):
        raise DotNetManagerError(
            "Version key must be a non-empty string",
            error_code="INVALID_VERSION_KEY",
            version_key=version_key
        )
    
    manager = DotNetManager()
    result = manager.check_installed(version_key)
    
    logger.debug(
        f"API: Version check result: {version_key} = {result}",
        extra={"version_key": version_key, "is_installed": result}
    )
    
    return result


@handle_platform_compatibility
def list_installed_dotnets() -> list[DotNetVersion]:
    """
    List all installed .NET Framework versions.

    Returns:
        A list of DotNetVersion objects sorted by release number
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If version scanning fails
    """
    logger.debug("API: Listing installed .NET versions")
    
    manager = DotNetManager()
    versions = manager.list_installed_versions()
    
    logger.info(
        f"API: Found {len(versions)} installed .NET versions",
        extra={"version_count": len(versions)}
    )
    
    return versions


@handle_platform_compatibility
def list_available_dotnets() -> list[DotNetVersion]:
    """
    List all .NET Framework versions available for download.

    Returns:
        A list of available DotNetVersion objects sorted by release number (latest first)
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
    """
    logger.debug("API: Listing available .NET versions")
    
    manager = DotNetManager()
    versions = manager.list_available_versions()
    
    logger.info(
        f"API: Found {len(versions)} available .NET versions",
        extra={"available_count": len(versions)}
    )
    
    return versions


@handle_async_platform_compatibility
async def download_file_async(
    url: str,
    output_path: str,
    expected_checksum: Optional[str] = None,
    show_progress: bool = True
) -> DownloadResult:
    """
    Asynchronously download a file with optional checksum verification.

    Args:
        url: URL to download from
        output_path: Path where the file should be saved
        expected_checksum: Optional SHA256 checksum for verification
        show_progress: Whether to display a progress bar

    Returns:
        DownloadResult object with detailed download information
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If download parameters are invalid
        DownloadError: If download fails
        ChecksumError: If checksum verification fails
    """
    logger.debug(
        f"API: Starting async download: {url}",
        extra={"url": url, "output_path": output_path}
    )
    
    # Validate parameters
    if not url or not isinstance(url, str):
        raise DotNetManagerError(
            "URL must be a non-empty string",
            error_code="INVALID_URL",
            url=url
        )
    
    if not output_path or not isinstance(output_path, str):
        raise DotNetManagerError(
            "Output path must be a non-empty string",
            error_code="INVALID_OUTPUT_PATH",
            output_path=output_path
        )
    
    manager = DotNetManager()
    path = Path(output_path)
    
    start_time = time.time()
    result = await manager.download_file_async(url, path, expected_checksum, show_progress)
    end_time = time.time()
    
    logger.info(
        f"API: Download completed in {end_time - start_time:.2f} seconds",
        extra={
            "url": url,
            "output_path": str(path),
            "size_mb": result.size_mb,
            "success": result.success
        }
    )
    
    return result


@handle_async_platform_compatibility
async def verify_checksum_async(
    file_path: str,
    expected_checksum: str,
    algorithm: HashAlgorithm = HashAlgorithm.SHA256
) -> bool:
    """
    Asynchronously verify a file's checksum.

    Args:
        file_path: Path to the file
        expected_checksum: The expected checksum hash
        algorithm: The hash algorithm to use

    Returns:
        True if the checksum matches, False otherwise
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If parameters are invalid
        ChecksumError: If verification fails due to errors
    """
    logger.debug(
        f"API: Verifying checksum for {file_path}",
        extra={"file_path": file_path, "algorithm": algorithm.value}
    )
    
    # Validate parameters
    if not file_path or not isinstance(file_path, str):
        raise DotNetManagerError(
            "File path must be a non-empty string",
            error_code="INVALID_FILE_PATH",
            file_path=file_path
        )
    
    if not expected_checksum or not isinstance(expected_checksum, str):
        raise DotNetManagerError(
            "Expected checksum must be a non-empty string",
            error_code="INVALID_CHECKSUM",
            expected_checksum=expected_checksum
        )
    
    manager = DotNetManager()
    result = await manager.verify_checksum_async(Path(file_path), expected_checksum, algorithm)
    
    logger.debug(
        f"API: Checksum verification {'passed' if result else 'failed'}",
        extra={"file_path": file_path, "algorithm": algorithm.value, "matches": result}
    )
    
    return result


@handle_platform_compatibility
def install_software(installer_path: str, quiet: bool = True, timeout_seconds: int = 3600) -> InstallationResult:
    """
    Execute a software installer with enhanced monitoring.

    Args:
        installer_path: Path to the installer executable
        quiet: Whether to run the installer silently
        timeout_seconds: Maximum time to wait for installation

    Returns:
        InstallationResult with detailed installation information
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If installer path is invalid
        InstallationError: If installation fails
    """
    logger.info(
        f"API: Starting installation: {installer_path}",
        extra={"installer_path": installer_path, "quiet": quiet, "timeout": timeout_seconds}
    )
    
    # Validate parameters
    if not installer_path or not isinstance(installer_path, str):
        raise DotNetManagerError(
            "Installer path must be a non-empty string",
            error_code="INVALID_INSTALLER_PATH",
            installer_path=installer_path
        )
    
    manager = DotNetManager()
    result = manager.install_software(Path(installer_path), quiet, timeout_seconds)
    
    logger.info(
        f"API: Installation {'completed' if result.success else 'failed'}",
        extra={
            "installer_path": installer_path,
            "success": result.success,
            "return_code": result.return_code
        }
    )
    
    return result


@handle_platform_compatibility
def uninstall_dotnet(version_key: str) -> bool:
    """
    Attempt to uninstall a specific .NET Framework version.
    
    Note: This is generally not recommended or possible for system components.

    Args:
        version_key: The version to uninstall (e.g., "v4.8")

    Returns:
        False (uninstallation not supported)
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If version key is invalid
    """
    logger.warning(
        f"API: Uninstall requested for {version_key}",
        extra={"version_key": version_key}
    )
    
    # Validate parameters
    if not version_key or not isinstance(version_key, str):
        raise DotNetManagerError(
            "Version key must be a non-empty string",
            error_code="INVALID_VERSION_KEY",
            version_key=version_key
        )
    
    manager = DotNetManager()
    result = manager.uninstall_dotnet(version_key)
    
    logger.info(
        f"API: Uninstall operation completed (not supported)",
        extra={"version_key": version_key, "result": result}
    )
    
    return result


@handle_platform_compatibility
def get_latest_known_version() -> Optional[DotNetVersion]:
    """
    Get the latest .NET version known to the manager.

    Returns:
        A DotNetVersion object for the latest known version, or None
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
    """
    logger.debug("API: Getting latest known .NET version")
    
    manager = DotNetManager()
    latest = manager.get_latest_known_version()
    
    if latest:
        logger.debug(
            f"API: Latest known version: {latest.key}",
            extra={"version_key": latest.key, "release": latest.release}
        )
    else:
        logger.warning("API: No known .NET versions available")
    
    return latest


@handle_platform_compatibility
def get_version_info(version_key: str) -> Optional[DotNetVersion]:
    """
    Get detailed information about a specific .NET version.

    Args:
        version_key: The version key to look up (e.g., "v4.8")

    Returns:
        DotNetVersion object with detailed information, or None if not found
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If version key is invalid
    """
    logger.debug(f"API: Getting version info for {version_key}")
    
    # Validate parameters
    if not version_key or not isinstance(version_key, str):
        raise DotNetManagerError(
            "Version key must be a non-empty string",
            error_code="INVALID_VERSION_KEY",
            version_key=version_key
        )
    
    manager = DotNetManager()
    version_info = manager.get_version_info(version_key)
    
    if version_info:
        logger.debug(
            f"API: Found version info for {version_key}",
            extra={"version_key": version_key, "is_downloadable": version_info.is_downloadable}
        )
    else:
        logger.debug(f"API: No version info found for {version_key}")
    
    return version_info


# Synchronous wrapper for download for simpler use cases
def download_file(
    url: str,
    output_path: str,
    expected_checksum: Optional[str] = None,
    show_progress: bool = True
) -> DownloadResult:
    """
    Synchronously download a file. Wraps the async version.

    Args:
        url: URL to download from
        output_path: Path where the file should be saved
        expected_checksum: Optional SHA256 checksum for verification
        show_progress: Whether to display a progress bar

    Returns:
        DownloadResult object with detailed download information
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If parameters are invalid
        DownloadError: If download fails
        ChecksumError: If checksum verification fails
    """
    logger.debug(
        f"API: Starting sync download wrapper: {url}",
        extra={"url": url, "output_path": output_path}
    )
    
    try:
        return asyncio.run(download_file_async(url, output_path, expected_checksum, show_progress))
    except Exception as e:
        logger.error(f"API: Sync download wrapper failed: {e}")
        raise


def verify_checksum(
    file_path: str,
    expected_checksum: str,
    algorithm: HashAlgorithm = HashAlgorithm.SHA256
) -> bool:
    """
    Synchronously verify a file's checksum. Wraps the async version.

    Args:
        file_path: Path to the file
        expected_checksum: The expected checksum hash
        algorithm: The hash algorithm to use

    Returns:
        True if the checksum matches, False otherwise
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If parameters are invalid
        ChecksumError: If verification fails due to errors
    """
    logger.debug(
        f"API: Starting sync checksum verification: {file_path}",
        extra={"file_path": file_path, "algorithm": algorithm.value}
    )
    
    try:
        return asyncio.run(verify_checksum_async(file_path, expected_checksum, algorithm))
    except Exception as e:
        logger.error(f"API: Sync checksum verification failed: {e}")
        raise


@handle_platform_compatibility
def download_and_install_version(
    version_key: str,
    quiet: bool = True,
    verify_checksum: bool = True,
    cleanup_installer: bool = True
) -> tuple[DownloadResult, InstallationResult]:
    """
    Download and install a specific .NET Framework version in one operation.

    Args:
        version_key: The version to download and install (e.g., "v4.8")
        quiet: Whether to run the installer silently
        verify_checksum: Whether to verify the download checksum
        cleanup_installer: Whether to delete the installer after installation

    Returns:
        Tuple of (DownloadResult, InstallationResult)
        
    Raises:
        UnsupportedPlatformError: If not running on Windows
        DotNetManagerError: If version is not available or parameters are invalid
        DownloadError: If download fails
        ChecksumError: If checksum verification fails
        InstallationError: If installation fails
    """
    logger.info(
        f"API: Starting download and install for {version_key}",
        extra={
            "version_key": version_key,
            "quiet": quiet,
            "verify_checksum": verify_checksum,
            "cleanup_installer": cleanup_installer
        }
    )
    
    # Validate parameters
    if not version_key or not isinstance(version_key, str):
        raise DotNetManagerError(
            "Version key must be a non-empty string",
            error_code="INVALID_VERSION_KEY",
            version_key=version_key
        )
    
    # Get version information
    version_info = get_version_info(version_key)
    if not version_info:
        raise DotNetManagerError(
            f"Unknown version key: {version_key}",
            error_code="UNKNOWN_VERSION",
            version_key=version_key
        )
    
    if not version_info.is_downloadable:
        raise DotNetManagerError(
            f"Version {version_key} is not available for download",
            error_code="VERSION_NOT_DOWNLOADABLE",
            version_key=version_key
        )
    
    try:
        # Download the installer
        manager = DotNetManager()
        installer_path = manager.download_dir / f"dotnet_installer_{version_key}.exe"
        
        expected_checksum = version_info.installer_sha256 if verify_checksum else None
        
        download_result = download_file(
            version_info.installer_url,
            str(installer_path),
            expected_checksum,
            show_progress=True
        )
        
        # Install the software
        installation_result = install_software(str(installer_path), quiet)
        
        # Cleanup if requested
        if cleanup_installer and installer_path.exists():
            try:
                installer_path.unlink()
                logger.debug(f"Cleaned up installer: {installer_path}")
            except Exception as e:
                logger.warning(f"Failed to cleanup installer: {e}")
        
        logger.info(
            f"API: Download and install completed for {version_key}",
            extra={
                "version_key": version_key,
                "download_success": download_result.success,
                "install_success": installation_result.success
            }
        )
        
        return download_result, installation_result
        
    except Exception as e:
        logger.error(f"API: Download and install failed for {version_key}: {e}")
        raise