"""
Lithium export declarations for dotnet_manager module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from lithium_bridge import expose_command, expose_controller

from .api import (
    check_dotnet_installed,
    download_file,
    install_software,
    list_installed_dotnets,
)


@expose_controller(
    endpoint="/api/dotnet/check",
    method="GET",
    description="Check if a .NET Framework version is installed",
    tags=["dotnet", "system"],
    version="1.0.0"
)
def check_dotnet(version: str) -> dict:
    """
    Check if a specific .NET Framework version is installed.

    Args:
        version: The version to check (e.g., "v4.8")

    Returns:
        Dictionary with installation status
    """
    installed = check_dotnet_installed(version)
    return {
        "version": version,
        "installed": installed,
    }


@expose_controller(
    endpoint="/api/dotnet/list",
    method="GET",
    description="List all installed .NET Framework versions",
    tags=["dotnet", "system"],
    version="1.0.0"
)
def list_dotnets() -> dict:
    """
    List all installed .NET Framework versions.

    Returns:
        Dictionary with list of installed versions
    """
    versions = list_installed_dotnets()
    return {
        "versions": versions,
        "count": len(versions),
    }


@expose_controller(
    endpoint="/api/dotnet/download",
    method="POST",
    description="Download a file with optional multi-threading",
    tags=["dotnet", "download"],
    version="1.0.0"
)
def download(
    url: str,
    filename: str,
    num_threads: int = 4,
    expected_checksum: str = None,
) -> dict:
    """
    Download a file with optional multi-threading and checksum verification.

    Args:
        url: URL to download from
        filename: Path where the file should be saved
        num_threads: Number of download threads to use
        expected_checksum: Optional SHA256 checksum for verification

    Returns:
        Dictionary with download result
    """
    success = download_file(
        url=url,
        filename=filename,
        num_threads=num_threads,
        expected_checksum=expected_checksum,
    )
    return {
        "url": url,
        "filename": filename,
        "success": success,
    }


@expose_controller(
    endpoint="/api/dotnet/install",
    method="POST",
    description="Execute a software installer",
    tags=["dotnet", "install"],
    version="1.0.0"
)
def install(installer_path: str, quiet: bool = False) -> dict:
    """
    Execute a software installer.

    Args:
        installer_path: Path to the installer executable
        quiet: Whether to run the installer silently

    Returns:
        Dictionary with installation result
    """
    success = install_software(installer_path=installer_path, quiet=quiet)
    return {
        "installer_path": installer_path,
        "quiet": quiet,
        "success": success,
    }


@expose_command(
    command_id="dotnet.check",
    description="Check .NET installation (command)",
    priority=5,
    timeout_ms=5000,
    tags=["dotnet"]
)
def cmd_check_dotnet(version: str) -> dict:
    """Check .NET installation via command dispatcher."""
    return check_dotnet(version)


@expose_command(
    command_id="dotnet.list",
    description="List installed .NET versions (command)",
    priority=5,
    timeout_ms=5000,
    tags=["dotnet"]
)
def cmd_list_dotnets() -> dict:
    """List .NET versions via command dispatcher."""
    return list_dotnets()
