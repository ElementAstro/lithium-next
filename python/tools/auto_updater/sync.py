# sync.py
"""Synchronous wrapper for AutoUpdater, suitable for use with pybind11."""

import asyncio
import json
from pathlib import Path
from typing import Any, Callable, Dict, Optional, Union

from .updater import AutoUpdater
from .models import UpdaterConfig, UpdateStatus, ProgressCallback
from .strategies import JsonUpdateStrategy
from .packaging import ZipPackageHandler


class SyncProgressCallback:
    """Adapts an async progress callback to a synchronous interface."""

    def __init__(self, sync_callback: Callable[[str, float, str], None]):
        self._sync_callback = sync_callback

    async def __call__(
        self, status: UpdateStatus, progress: float, message: str
    ) -> None:
        self._sync_callback(status.value, progress, message)


class AutoUpdaterSync:
    """
    Synchronous wrapper for AutoUpdater, suitable for use with pybind11.
    This class provides the same functionality as AutoUpdater but with
    a synchronous API that's easier to integrate with C++ applications.
    """

    def __init__(
        self,
        config: Dict[str, Any],
        progress_callback: Optional[Callable[[str, float, str], None]] = None,
    ):
        """
        Initialize the synchronous auto updater.

        Args:
            config: Configuration dictionary.
            progress_callback: Optional callback for progress updates (status, progress, message).
        """
        # Convert dict config to UpdaterConfig model
        updater_config = UpdaterConfig(
            strategy=JsonUpdateStrategy(url=config["url"]),
            package_handler=ZipPackageHandler(),
            install_dir=Path(config["install_dir"]),
            current_version=config["current_version"],
            temp_dir=Path(config.get("temp_dir", Path(config["install_dir"]) / "temp")),
            backup_dir=Path(
                config.get("backup_dir", Path(config["install_dir"]) / "backup")
            ),
            custom_hooks=config.get("custom_hooks", {}),
        )

        if progress_callback:
            updater_config.progress_callback = SyncProgressCallback(progress_callback)

        self.updater = AutoUpdater(updater_config)
        # 修复：asyncio.current_tasks 并不存在，应该用 asyncio.all_tasks
        loop = None
        try:
            loop = asyncio.get_event_loop()
        except RuntimeError:
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
        self._loop = loop

    def _run_async(self, coro):
        """Helper to run an async coroutine synchronously."""
        return self._loop.run_until_complete(coro)

    def check_for_updates(self) -> bool:
        """Check for updates synchronously."""
        return self._run_async(self.updater.check_for_updates())

    def download_update(self) -> str:
        """Download the update synchronously."""
        return str(self._run_async(self.updater.download_update()))

    def verify_update(self, download_path: str) -> bool:
        """Verify the update synchronously."""
        return self._run_async(self.updater.verify_update(Path(download_path)))

    def backup_current_installation(self) -> str:
        """Back up the current installation synchronously."""
        return str(self._run_async(self.updater.backup_current_installation()))

    def extract_update(self, download_path: str) -> str:
        """Extract the update archive synchronously."""
        return str(self._run_async(self.updater.extract_update(Path(download_path))))

    def install_update(self, extract_dir: str) -> bool:
        """Install the update synchronously."""
        return self._run_async(self.updater.install_update(Path(extract_dir)))

    def rollback(self, backup_dir: str) -> bool:
        """Roll back to a previous backup synchronously."""
        return self._run_async(self.updater.rollback(Path(backup_dir)))

    def update(self) -> bool:
        """
        Execute the full update process synchronously.

        Returns:
            bool: True if update was successful, False otherwise.
        """
        return self._run_async(self.updater.update())

    def cleanup(self) -> None:
        """Clean up temporary files."""
        self._run_async(self.updater.cleanup())


def create_updater(config_json: str, progress_callback=None) -> AutoUpdaterSync:
    """
    Create an AutoUpdaterSync instance from JSON configuration string.
    This function is designed for pybind11 integration.

    Args:
        config_json: JSON string containing configuration.
        progress_callback: Optional callback function for progress updates.

    Returns:
        AutoUpdaterSync: Synchronous updater instance.
    """
    config = json.loads(config_json)
    return AutoUpdaterSync(config, progress_callback)


def run_updater(config: Dict[str, Any], in_thread: bool = False) -> bool:
    """
    Run the updater with the provided configuration.

    Args:
        config: Configuration parameters for the updater.
        in_thread: Whether to run the updater in a separate thread.

    Returns:
        bool: True if update was successful, False otherwise.
    """
    # This function will now use the synchronous wrapper
    updater = AutoUpdaterSync(config)

    if in_thread:
        # Running in a separate thread still requires an event loop for the async calls
        # This is a simplified approach; for robust threading with asyncio, consider
        # asyncio.run_coroutine_threadsafe or a dedicated event loop in the thread.
        import threading

        thread = threading.Thread(target=lambda: updater.update())
        thread.daemon = True
        thread.start()
        return True
    else:
        return updater.update()
