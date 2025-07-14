# core.py
"""Synchronous wrapper for the async AutoUpdater implementation."""
import asyncio
from pathlib import Path
from typing import Dict, Any, Optional

from .updater import AutoUpdater as AsyncAutoUpdater
from .models import UpdaterConfig
from .logger import logger


class AutoUpdater:
    """
    Synchronous wrapper for the async AutoUpdater class.
    Provides the same functionality but with a synchronous API.
    """

    def __init__(self, config_dict: Dict[str, Any]):
        """
        Initialize the auto updater.

        Args:
            config_dict: Configuration dictionary.
        """
        # Convert dictionary config to UpdaterConfig
        if isinstance(config_dict, dict):
            # Process paths in the config
            if 'install_dir' in config_dict and isinstance(config_dict['install_dir'], str):
                config_dict['install_dir'] = Path(config_dict['install_dir'])
            if 'temp_dir' in config_dict and isinstance(config_dict['temp_dir'], str):
                config_dict['temp_dir'] = Path(config_dict['temp_dir'])
            if 'backup_dir' in config_dict and isinstance(config_dict['backup_dir'], str):
                config_dict['backup_dir'] = Path(config_dict['backup_dir'])

            self.config = UpdaterConfig(**config_dict)
        else:
            self.config = config_dict

        # Create async updater
        self._async_updater = AsyncAutoUpdater(self.config)

    def _run_async(self, coro):
        """Run an async coroutine synchronously."""
        try:
            loop = asyncio.get_event_loop()
        except RuntimeError:
            # Create new event loop if there isn't one
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)

        return loop.run_until_complete(coro)

    def check_for_updates(self) -> bool:
        """
        Check for available updates.

        Returns:
            bool: True if an update is available, False otherwise.
        """
        return self._run_async(self._async_updater.check_for_updates())

    def download_update(self) -> Path:
        """
        Download the update package.

        Returns:
            Path: Path to the downloaded file.
        """
        return self._run_async(self._async_updater.download_update())

    def verify_update(self, download_path: Path) -> bool:
        """
        Verify the integrity of the downloaded update.

        Args:
            download_path: Path to the downloaded update file.

        Returns:
            bool: True if verification passed, False otherwise.
        """
        return self._run_async(self._async_updater.verify_update(download_path))

    def backup_current_installation(self) -> Path:
        """
        Create a backup of the current installation.

        Returns:
            Path: Path to the backup directory.
        """
        return self._run_async(self._async_updater.backup_current_installation())

    def extract_update(self, download_path: Path) -> Path:
        """
        Extract the update package.

        Args:
            download_path: Path to the downloaded update file.

        Returns:
            Path: Path to the directory where the update was extracted.
        """
        return self._run_async(self._async_updater.extract_update(download_path))

    def install_update(self, extract_dir: Path) -> bool:
        """
        Install the extracted update.

        Args:
            extract_dir: Path to the directory with extracted update files.

        Returns:
            bool: True if installation was successful, False otherwise.
        """
        return self._run_async(self._async_updater.install_update(extract_dir))

    def rollback(self, backup_dir: Path) -> bool:
        """
        Rollback to a previous backup.

        Args:
            backup_dir: Path to the backup directory.

        Returns:
            bool: True if rollback was successful, False otherwise.
        """
        return self._run_async(self._async_updater.rollback(backup_dir))

    def update(self) -> bool:
        """
        Run the full update process (check, download, verify, backup, extract, install).

        Returns:
            bool: True if the update was successful, False otherwise.
        """
        return self._run_async(self._async_updater.update())

    def cleanup(self) -> None:
        """Clean up temporary files and resources."""
        self._run_async(self._async_updater.cleanup())

    @property
    def update_info(self):
        """Get information about the available update."""
        return self._async_updater.update_info
