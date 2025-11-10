# core.py
import os
import json
import zipfile
import shutil
import requests
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import Any, Dict, Optional, Union
from tqdm import tqdm

from .types import (
    UpdateStatus,
    NetworkError,
    VerificationError,
    InstallationError,
    UpdaterError,
    ProgressCallback,
    UpdaterConfig,
    PathLike,
)
from .utils import compare_versions, calculate_file_hash
from .logger import logger


class AutoUpdater:
    """
    Advanced Auto Updater for software applications.

    This class handles the entire update process:
    1. Checking for updates
    2. Downloading update packages
    3. Verifying downloads using hash checking
    4. Backing up existing installation
    5. Installing the update
    6. Rolling back if needed

    The updater supports both synchronous and asynchronous operations,
    making it suitable for command-line usage or integration with GUI applications.
    """

    def __init__(
        self,
        config: Union[Dict[str, Any], UpdaterConfig],
        progress_callback: Optional[ProgressCallback] = None,
    ):
        """
        Initialize the AutoUpdater.

        Args:
            config: Configuration dictionary or UpdaterConfig object
            progress_callback: Optional callback for progress updates
        """
        # Initialize configuration
        if isinstance(config, dict):
            self.config = UpdaterConfig(**config)
        else:
            self.config = config

        # Initialize other attributes
        self.progress_callback = progress_callback
        self.update_info: Optional[Dict[str, Any]] = None
        self.status = UpdateStatus.CHECKING
        self.session = requests.Session()
        self._executor = None

        # Ensure directories exist
        if self.config.temp_dir is not None:
            self.config.temp_dir.mkdir(parents=True, exist_ok=True)
        if self.config.backup_dir is not None:
            self.config.backup_dir.mkdir(parents=True, exist_ok=True)

    def _get_executor(self) -> ThreadPoolExecutor:
        """
        Get or create a thread pool executor.

        Returns:
            ThreadPoolExecutor: The executor object
        """
        if self._executor is None:
            self._executor = ThreadPoolExecutor(max_workers=self.config.num_threads)
        return self._executor

    def _report_progress(
        self, status: UpdateStatus, progress: float, message: str
    ) -> None:
        """
        Report progress to the callback if provided.

        Args:
            status: Current status of the update process
            progress: Progress value between 0.0 and 1.0
            message: Descriptive message
        """
        self.status = status
        logger.info(f"[{status.value}] {message} ({progress:.1%})")

        if self.progress_callback:
            self.progress_callback(status, progress, message)

    def check_for_updates(self) -> bool:
        """
        Check for available updates.

        Returns:
            bool: True if an update is available, False otherwise

        Raises:
            NetworkError: If there is an issue connecting to the update server
        """
        self._report_progress(UpdateStatus.CHECKING, 0.0, "Checking for updates...")

        try:
            # Make request with retry logic
            response = None
            for attempt in range(3):
                try:
                    response = self.session.get(self.config.url, timeout=30)
                    response.raise_for_status()
                    break
                except (requests.RequestException, ConnectionError) as e:
                    if attempt == 2:  # Last attempt
                        raise NetworkError(f"Failed to check for updates: {e}")
                    time.sleep(1 * (attempt + 1))  # Backoff delay

            if response is None:
                raise NetworkError("Failed to get a response from the update server.")

            # Parse update information
            data = response.json()
            self.update_info = data

            # Check if update is available
            latest_version = data.get("version")
            if not latest_version:
                logger.warning("Version information missing in update data")
                return False

            is_newer = compare_versions(self.config.current_version, latest_version) < 0

            if is_newer:
                self._report_progress(
                    UpdateStatus.UPDATE_AVAILABLE,
                    1.0,
                    f"Update available: {latest_version}",
                )
                return True
            else:
                self._report_progress(
                    UpdateStatus.UP_TO_DATE,
                    1.0,
                    f"Already up to date: {self.config.current_version}",
                )
                return False

        except json.JSONDecodeError:
            raise UpdaterError("Invalid JSON response from update server")

    def download_file(self, url: str, dest_path: Path) -> None:
        """
        Download a file with progress reporting.

        Args:
            url: URL to download from
            dest_path: Destination path for the downloaded file

        Raises:
            NetworkError: If the download fails
        """
        try:
            # Ensure directory exists
            dest_path.parent.mkdir(parents=True, exist_ok=True)

            # Stream the download with progress tracking
            response = self.session.get(url, stream=True, timeout=30)
            response.raise_for_status()

            # Get file size if available
            total_size = int(response.headers.get("content-length", 0))

            # Set up progress bar
            with tqdm(
                total=total_size,
                unit="B",
                unit_scale=True,
                desc=f"Downloading {dest_path.name}",
            ) as progress_bar:
                with open(dest_path, "wb") as f:
                    for chunk in response.iter_content(chunk_size=8192):
                        if chunk:  # Filter out keep-alive chunks
                            f.write(chunk)
                            progress_bar.update(len(chunk))

                            # Report progress at intervals
                            if (
                                total_size > 0
                                and progress_bar.n % (total_size // 10 + 1) == 0
                            ):
                                progress = progress_bar.n / total_size
                                self._report_progress(
                                    UpdateStatus.DOWNLOADING,
                                    progress,
                                    f"Downloaded {progress_bar.n} of {total_size} bytes",
                                )

        except requests.exceptions.RequestException as e:
            raise NetworkError(f"Failed to download file: {e}")

    def download_update(self) -> Path:
        """
        Download the update package.

        Returns:
            Path: Path to the downloaded file

        Raises:
            NetworkError: If the download fails
            UpdaterError: If update information is not available
        """
        if not self.update_info:
            raise UpdaterError(
                "No update information available. Call check_for_updates first."
            )

        self._report_progress(
            UpdateStatus.DOWNLOADING,
            0.0,
            f"Downloading update {self.update_info['version']}...",
        )

        download_url = self.update_info.get("download_url")
        if not download_url:
            raise UpdaterError("Download URL not provided in update information")

        # Prepare download path
        if self.config.temp_dir is None:
            raise UpdaterError(
                "Temporary directory (temp_dir) is not set in configuration."
            )
        download_path = (
            self.config.temp_dir / f"update_{self.update_info['version']}.zip"
        )

        # Download the file
        self.download_file(download_url, download_path)

        self._report_progress(
            UpdateStatus.DOWNLOADING, 1.0, f"Download complete: {download_path}"
        )
        return download_path

    def verify_update(self, download_path: Path) -> bool:
        """
        Verify the downloaded update file.

        Args:
            download_path: Path to the downloaded file

        Returns:
            bool: True if verification passed, False otherwise
        """
        if not self.update_info:
            raise UpdaterError("No update information available")

        self._report_progress(
            UpdateStatus.VERIFYING, 0.0, "Verifying downloaded update..."
        )

        # Verify file hash if configured and hash is provided
        if self.config.verify_hash and "file_hash" in self.update_info:
            expected_hash = self.update_info["file_hash"]
            self._report_progress(
                UpdateStatus.VERIFYING,
                0.3,
                f"Calculating {self.config.hash_algorithm} hash...",
            )

            calculated_hash = calculate_file_hash(
                download_path, self.config.hash_algorithm
            )

            if calculated_hash.lower() != expected_hash.lower():
                self._report_progress(
                    UpdateStatus.FAILED,
                    1.0,
                    f"Hash verification failed. Expected: {expected_hash}, Got: {calculated_hash}",
                )
                return False

            self._report_progress(
                UpdateStatus.VERIFYING, 1.0, "Hash verification passed"
            )
        else:
            # If no hash verification is needed
            self._report_progress(
                UpdateStatus.VERIFYING,
                1.0,
                "Hash verification skipped (not configured or hash not provided)",
            )

        return True

    def backup_current_installation(self) -> Path:
        """
        Back up the current installation.

        Returns:
            Path: Path to the backup directory

        Raises:
            InstallationError: If backup fails
        """
        self._report_progress(
            UpdateStatus.BACKING_UP, 0.0, "Backing up current installation..."
        )

        # Create timestamped backup directory
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        if self.config.backup_dir is None:
            raise InstallationError("Backup directory is not set in configuration.")
        backup_dir = (
            self.config.backup_dir / f"backup_{self.config.current_version}_{timestamp}"
        )
        backup_dir.mkdir(parents=True, exist_ok=True)

        try:
            # Get list of files to backup (exclude temp and backup directories)
            excluded_dirs = set()
            if self.config.temp_dir is not None:
                excluded_dirs.add(self.config.temp_dir.resolve())
            if self.config.backup_dir is not None:
                excluded_dirs.add(self.config.backup_dir.resolve())

            # Get all files in installation directory
            all_items = list(self.config.install_dir.glob("**/*"))
            items_to_backup = [
                item
                for item in all_items
                if not any(p in item.parents or p == item for p in excluded_dirs)
                and not item.is_dir()  # Only count files for progress tracking
            ]

            total_items = len(items_to_backup)
            if total_items == 0:
                self._report_progress(
                    UpdateStatus.BACKING_UP, 1.0, "No files to backup"
                )
                return backup_dir

            # Copy files and track progress
            processed = 0

            # Create parent directories first
            for item in items_to_backup:
                rel_path = item.relative_to(self.config.install_dir)
                dest_path = backup_dir / rel_path
                dest_path.parent.mkdir(parents=True, exist_ok=True)

            # Copy files with progress tracking
            with self._get_executor() as executor:
                # Submit all copy tasks
                futures = []
                for item in items_to_backup:
                    rel_path = item.relative_to(self.config.install_dir)
                    dest_path = backup_dir / rel_path
                    futures.append(executor.submit(shutil.copy2, item, dest_path))

                # Process results as they complete
                for future in futures:
                    # Wait for task to complete
                    future.result()
                    processed += 1

                    # Update progress periodically
                    if processed % max(1, total_items // 20) == 0:
                        self._report_progress(
                            UpdateStatus.BACKING_UP,
                            processed / total_items,
                            f"Backed up {processed}/{total_items} files",
                        )

            # Create a manifest file with backup information
            manifest = {
                "timestamp": timestamp,
                "version": self.config.current_version,
                "backup_path": str(backup_dir),
            }

            with open(backup_dir / "backup_manifest.json", "w") as f:
                json.dump(manifest, f, indent=2)

            self._report_progress(
                UpdateStatus.BACKING_UP, 1.0, f"Backup complete: {backup_dir}"
            )
            return backup_dir

        except Exception as e:
            self._report_progress(UpdateStatus.FAILED, 0.0, f"Backup failed: {e}")
            raise InstallationError(
                f"Failed to backup current installation: {e}"
            ) from e

    def extract_update(self, download_path: Path) -> Path:
        """
        Extract the update archive.

        Args:
            download_path: Path to the downloaded archive

        Returns:
            Path: Path to the extraction directory

        Raises:
            InstallationError: If extraction fails
        """
        self._report_progress(
            UpdateStatus.EXTRACTING, 0.0, "Extracting update files..."
        )

        if self.config.temp_dir is None:
            raise InstallationError(
                "Temporary directory (temp_dir) is not set in configuration."
            )
        extract_dir = self.config.temp_dir / "extracted"

        # Clean up existing extraction directory if it exists
        if extract_dir.exists():
            shutil.rmtree(extract_dir)

        # Create extraction directory
        extract_dir.mkdir(parents=True, exist_ok=True)

        try:
            # Extract the archive
            with zipfile.ZipFile(download_path, "r") as zip_ref:
                # Get total number of items for progress tracking
                total_items = len(zip_ref.namelist())

                # Extract files with progress tracking
                for i, item in enumerate(zip_ref.namelist()):
                    zip_ref.extract(item, extract_dir)

                    # Update progress periodically
                    if i % max(1, total_items // 10) == 0:
                        self._report_progress(
                            UpdateStatus.EXTRACTING,
                            i / total_items,
                            f"Extracted {i}/{total_items} files",
                        )

            self._report_progress(UpdateStatus.EXTRACTING, 1.0, "Extraction complete")
            return extract_dir

        except zipfile.BadZipFile as e:
            self._report_progress(
                UpdateStatus.FAILED, 0.0, f"Failed to extract update: {e}"
            )
            raise InstallationError(f"Failed to extract update: {e}") from e

    def install_update(self, extract_dir: Path) -> bool:
        """
        Install the extracted update files.

        Args:
            extract_dir: Path to the extracted files

        Returns:
            bool: True if installation was successful

        Raises:
            InstallationError: If installation fails
        """
        self._report_progress(
            UpdateStatus.INSTALLING, 0.0, "Installing update files..."
        )

        try:
            # Get list of all files in the extraction directory
            all_items = list(extract_dir.glob("**/*"))

            # Separate directories and files for processing
            # We need to create directories first, then copy files
            dirs = [item for item in all_items if item.is_dir()]
            files = [item for item in all_items if item.is_file()]

            # Create directories
            for item in dirs:
                rel_path = item.relative_to(extract_dir)
                dest_path = self.config.install_dir / rel_path
                dest_path.mkdir(parents=True, exist_ok=True)

            # Copy files with progress tracking
            total_files = len(files)
            for i, item in enumerate(files):
                rel_path = item.relative_to(extract_dir)
                dest_path = self.config.install_dir / rel_path

                # Ensure parent directory exists
                dest_path.parent.mkdir(parents=True, exist_ok=True)

                # Copy file
                shutil.copy2(item, dest_path)

                # Update progress periodically
                if i % max(1, total_files // 10) == 0:
                    self._report_progress(
                        UpdateStatus.INSTALLING,
                        i / total_files,
                        f"Installed {i}/{total_files} files",
                    )

            # Run custom post-install actions
            if (
                self.config.custom_params is not None
                and "post_install" in self.config.custom_params
            ):
                self._report_progress(
                    UpdateStatus.FINALIZING, 0.9, "Running post-install actions..."
                )
                self.config.custom_params["post_install"]()

            if self.update_info is not None:
                self._report_progress(
                    UpdateStatus.COMPLETE,
                    1.0,
                    f"Update to version {self.update_info['version']} installed successfully",
                )
            else:
                self._report_progress(
                    UpdateStatus.COMPLETE, 1.0, "Update installed successfully"
                )

            # Log the update
            self._log_update()

            return True

        except Exception as e:
            self._report_progress(UpdateStatus.FAILED, 0.0, f"Installation failed: {e}")
            raise InstallationError(f"Failed to install update: {e}") from e

    def rollback(self, backup_dir: Path) -> bool:
        """
        Roll back to a previous backup.

        Args:
            backup_dir: Path to the backup directory

        Returns:
            bool: True if rollback was successful

        Raises:
            InstallationError: If rollback fails
        """
        self._report_progress(
            UpdateStatus.BACKING_UP, 0.0, f"Rolling back to backup: {backup_dir}"
        )

        try:
            # Check if backup directory exists
            if not backup_dir.exists():
                raise InstallationError(f"Backup directory not found: {backup_dir}")

            # Check for manifest file
            manifest_path = backup_dir / "backup_manifest.json"
            if manifest_path.exists():
                with open(manifest_path, "r") as f:
                    manifest = json.load(f)
                version = manifest.get("version", "unknown")
            else:
                version = "unknown"

            # Get all files in backup
            backup_files = list(backup_dir.glob("**/*"))
            files_to_restore = [
                f
                for f in backup_files
                if f.is_file() and f.name != "backup_manifest.json"
            ]

            total_files = len(files_to_restore)
            if total_files == 0:
                self._report_progress(
                    UpdateStatus.ROLLED_BACK, 1.0, "No files found in backup"
                )
                return False

            # Copy files back with progress tracking
            for i, file_path in enumerate(files_to_restore):
                rel_path = file_path.relative_to(backup_dir)
                dest_path = self.config.install_dir / rel_path

                # Ensure parent directory exists
                dest_path.parent.mkdir(parents=True, exist_ok=True)

                # Copy file back
                shutil.copy2(file_path, dest_path)

                # Update progress periodically
                if i % max(1, total_files // 10) == 0:
                    self._report_progress(
                        UpdateStatus.BACKING_UP,
                        i / total_files,
                        f"Restored {i}/{total_files} files",
                    )

            self._report_progress(
                UpdateStatus.ROLLED_BACK, 1.0, f"Rollback to version {version} complete"
            )
            return True

        except Exception as e:
            self._report_progress(UpdateStatus.FAILED, 0.0, f"Rollback failed: {e}")
            raise InstallationError(f"Failed to rollback: {e}") from e

    def _log_update(self) -> None:
        """Log the update details to a file."""
        if not self.update_info:
            return

        log_file = self.config.install_dir / "update_log.json"

        try:
            # Load existing log or create new one
            if log_file.exists():
                with open(log_file, "r") as f:
                    log_data = json.load(f)
            else:
                log_data = {"updates": []}

            # Add new entry
            log_data["updates"].append(
                {
                    "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                    "from_version": self.config.current_version,
                    "to_version": self.update_info["version"],
                    "download_url": self.update_info.get("download_url", ""),
                }
            )

            # Write log
            with open(log_file, "w") as f:
                json.dump(log_data, f, indent=2)

        except Exception as e:
            logger.warning(f"Failed to log update: {e}")

    def cleanup(self) -> None:
        """Clean up temporary files and resources."""
        try:
            # Close executor if it exists
            if self._executor:
                self._executor.shutdown(wait=False)
                self._executor = None

            # Close session
            if self.session:
                self.session.close()

            # Delete temporary files
            if self.config.temp_dir is not None and self.config.temp_dir.exists():
                shutil.rmtree(self.config.temp_dir, ignore_errors=True)
                self.config.temp_dir.mkdir(parents=True, exist_ok=True)

        except Exception as e:
            logger.warning(f"Cleanup failed: {e}")

    def update(self) -> bool:
        """
        Execute the full update process.

        Returns:
            bool: True if update was successful, False if no update was needed or update failed

        Raises:
            UpdaterError: If any part of the update process fails
        """
        try:
            # Check for updates
            update_available = self.check_for_updates()
            if not update_available:
                return False

            # Download update
            download_path = self.download_update()

            # Verify update
            if not self.verify_update(download_path):
                raise VerificationError("Update verification failed")

            # Run custom post-download actions if specified
            if (
                self.config.custom_params is not None
                and "post_download" in self.config.custom_params
            ):
                self._report_progress(
                    UpdateStatus.FINALIZING, 0.0, "Running post-download actions..."
                )
                self.config.custom_params["post_download"]()

            # Backup current installation
            backup_dir = self.backup_current_installation()

            # Extract update
            extract_dir = self.extract_update(download_path)

            # Install update
            try:
                self.install_update(extract_dir)
                return True
            except InstallationError:
                # If installation fails, try to rollback
                logger.warning("Installation failed, attempting rollback...")
                self.rollback(backup_dir)
                raise

        except Exception as e:
            self._report_progress(
                UpdateStatus.FAILED, 0.0, f"Update process failed: {e}"
            )
            raise
        finally:
            self.cleanup()
