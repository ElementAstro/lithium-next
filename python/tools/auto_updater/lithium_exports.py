"""
Lithium export declarations for auto_updater module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from lithium_bridge import expose_command, expose_controller

from .core import AutoUpdater


@expose_controller(
    endpoint="/api/updater/check",
    method="GET",
    description="Check for available updates",
    tags=["updater", "system"],
    version="1.0.0",
)
def check_updates(
    current_version: str,
    update_url: str = None,
) -> dict:
    """
    Check for available updates.

    Args:
        current_version: Current software version
        update_url: URL to check for updates

    Returns:
        Dictionary with update information
    """
    try:
        updater = AutoUpdater(current_version=current_version)
        if update_url:
            updater.update_url = update_url
        update_info = updater.check_for_updates()
        return {
            "current_version": current_version,
            "update_available": update_info is not None,
            "latest_version": update_info.version if update_info else current_version,
            "download_url": update_info.download_url if update_info else None,
        }
    except Exception as e:
        return {
            "current_version": current_version,
            "update_available": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/updater/download",
    method="POST",
    description="Download an update",
    tags=["updater", "system"],
    version="1.0.0",
)
def download_update(
    download_url: str,
    output_path: str,
    verify_checksum: bool = True,
) -> dict:
    """
    Download an update file.

    Args:
        download_url: URL to download from
        output_path: Path to save the downloaded file
        verify_checksum: Whether to verify the checksum

    Returns:
        Dictionary with download result
    """
    try:
        updater = AutoUpdater()
        success = updater.download_update(
            download_url=download_url,
            output_path=output_path,
            verify=verify_checksum,
        )
        return {
            "download_url": download_url,
            "output_path": output_path,
            "success": success,
        }
    except Exception as e:
        return {
            "download_url": download_url,
            "success": False,
            "error": str(e),
        }


@expose_command(
    command_id="updater.check",
    description="Check for updates (command)",
    priority=3,
    timeout_ms=30000,
    tags=["updater"],
)
def cmd_check_updates(current_version: str) -> dict:
    """Check for updates via command dispatcher."""
    return check_updates(current_version)
