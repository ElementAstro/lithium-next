# sync.py
from pathlib import Path
import json
import threading
from typing import Dict, Any, Optional, Callable, Union

from .core import AutoUpdater
from .types import UpdateStatus
from .logger import logger


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
            config: Configuration dictionary or UpdaterConfig object
            progress_callback: Optional callback for progress updates (status, progress, message)
        """
        self.updater = AutoUpdater(config)

        # Wrap the progress callback if provided
        if progress_callback:
            self.updater.progress_callback = (
                lambda status, progress, message: progress_callback(
                    status.value, progress, message
                )
            )

    def check_for_updates(self) -> bool:
        """
        Check for updates synchronously.

        Returns:
            bool: True if updates are available, False otherwise
        """
        return self.updater.check_for_updates()

    def download_update(self) -> str:
        """
        Download the update synchronously.

        Returns:
            str: Path to the downloaded file
        """
        return str(self.updater.download_update())

    def verify_update(self, download_path: str) -> bool:
        """
        Verify the update synchronously.

        Args:
            download_path: Path to the downloaded file

        Returns:
            bool: True if verification passed, False otherwise
        """
        return self.updater.verify_update(Path(download_path))

    def backup_current_installation(self) -> str:
        """
        Back up the current installation synchronously.

        Returns:
            str: Path to the backup directory
        """
        return str(self.updater.backup_current_installation())

    def extract_update(self, download_path: str) -> str:
        """
        Extract the update archive synchronously.

        Args:
            download_path: Path to the downloaded archive

        Returns:
            str: Path to the extraction directory
        """
        return str(self.updater.extract_update(Path(download_path)))

    def install_update(self, extract_dir: str) -> bool:
        """
        Install the update synchronously.

        Args:
            extract_dir: Path to the extracted files

        Returns:
            bool: True if installation was successful
        """
        return self.updater.install_update(Path(extract_dir))

    def rollback(self, backup_dir: str) -> bool:
        """
        Roll back to a previous backup synchronously.

        Args:
            backup_dir: Path to the backup directory

        Returns:
            bool: True if rollback was successful
        """
        return self.updater.rollback(Path(backup_dir))

    def update(self) -> bool:
        """
        Execute the full update process synchronously.

        Returns:
            bool: True if update was successful, False otherwise
        """
        return self.updater.update()

    def cleanup(self) -> None:
        """
        Clean up temporary files.
        """
        self.updater.cleanup()


# Functions for pybind11 integration
def create_updater(config_json: str, progress_callback=None):
    """
    Create an AutoUpdaterSync instance from JSON configuration string.
    This function is designed for pybind11 integration.

    Args:
        config_json: JSON string containing configuration
        progress_callback: Optional callback function for progress updates

    Returns:
        AutoUpdaterSync: Synchronous updater instance
    """
    config = json.loads(config_json)
    return AutoUpdaterSync(config, progress_callback)


def run_updater(config: Dict[str, Any], in_thread: bool = False) -> bool:
    """
    Run the updater with the provided configuration.

    Args:
        config: Configuration parameters for the updater
        in_thread: Whether to run the updater in a separate thread

    Returns:
        bool: True if update was successful, False otherwise
    """
    updater = AutoUpdater(config)

    if in_thread:
        # Run in a separate thread
        thread = threading.Thread(target=lambda: updater.update())
        thread.daemon = True
        thread.start()
        return True
    else:
        # Run in the current thread
        return updater.update()
