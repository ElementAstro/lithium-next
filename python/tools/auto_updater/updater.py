# updater.py
"""The core AutoUpdater class, orchestrating the update process asynchronously."""

import asyncio
import shutil
import time
import aiofiles
from pathlib import Path
from typing import Optional

import aiohttp
from loguru import logger
from tqdm.asyncio import tqdm

from .models import (
    UpdaterConfig, UpdateStatus, UpdateInfo, NetworkError,
    VerificationError, InstallationError, UpdaterError, ProgressCallback
)
from .utils import calculate_file_hash, compare_versions


class AutoUpdater:
    """
    Advanced Auto Updater for software applications.

    This class orchestrates the entire update process:
    1. Checking for updates using a defined strategy.
    2. Downloading update packages asynchronously.
    3. Verifying downloads using hash checking.
    4. Backing up existing installation.
    5. Installing the update using a defined package handler.
    6. Rolling back if needed.
    """

    def __init__(self, config: UpdaterConfig):
        """
        Initialize the AutoUpdater.

        Args:
            config: Configuration object for the updater.
        """
        self.config = config
        self.update_info: Optional[UpdateInfo] = None
        self.status: UpdateStatus = UpdateStatus.IDLE
        self._progress_callback = config.progress_callback

    async def _report_progress(self, status: UpdateStatus, progress: float, message: str) -> None:
        """
        Report progress to the callback if provided.
        """
        self.status = status
        logger.info(f"[{status.value}] {message} ({progress:.1%})")
        if self._progress_callback:
            await self._progress_callback(status, progress, message)

    async def check_for_updates(self) -> bool:
        """
        Check for available updates using the configured strategy.

        Returns:
            bool: True if an update is available, False otherwise.
        """
        await self._report_progress(UpdateStatus.CHECKING, 0.0, "Checking for updates...")
        try:
            self.update_info = await self.config.strategy.check_for_updates(self.config.current_version)
            if self.update_info:
                await self._report_progress(UpdateStatus.UPDATE_AVAILABLE, 1.0, f"Update available: {self.update_info.version}")
                return True
            else:
                await self._report_progress(UpdateStatus.UP_TO_DATE, 1.0, f"Already up to date: {self.config.current_version}")
                return False
        except NetworkError as e:
            await self._report_progress(UpdateStatus.FAILED, 0.0, f"Failed to check for updates: {e}")
            raise

    async def download_update(self) -> Path:
        """
        Download the update package asynchronously.

        Returns:
            Path: Path to the downloaded file.
        """
        if not self.update_info:
            raise UpdaterError(
                "No update information available. Call check_for_updates first.")

        await self._report_progress(UpdateStatus.DOWNLOADING, 0.0, f"Downloading update {self.update_info.version}...")

        download_url = str(self.update_info.download_url)
        download_path = self.config.temp_dir / \
            f"update_{self.update_info.version}.zip"

        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(download_url) as response:
                    response.raise_for_status()
                    total_size = int(response.headers.get('content-length', 0))

                    with tqdm(total=total_size, unit='B', unit_scale=True, desc=download_path.name) as pbar:
                        async with aiofiles.open(download_path, 'wb') as f:
                            async for chunk in response.content.iter_chunked(8192):
                                await f.write(chunk)
                                pbar.update(len(chunk))
                                await self._report_progress(
                                    UpdateStatus.DOWNLOADING,
                                    pbar.n / total_size if total_size else 0,
                                    f"Downloaded {pbar.n} of {total_size} bytes"
                                )
            await self._report_progress(UpdateStatus.DOWNLOADING, 1.0, f"Download complete: {download_path}")
            return download_path
        except aiohttp.ClientError as e:
            download_path.unlink(missing_ok=True)
            raise NetworkError(
                f"Failed to download file from {download_url}: {e}") from e

    async def verify_update(self, download_path: Path) -> bool:
        """
        Verify the downloaded update file.

        Args:
            download_path: Path to the downloaded file.

        Returns:
            bool: True if verification passed, False otherwise.
        """
        if not self.update_info or not self.update_info.file_hash:
            logger.warning(
                "No file hash provided in update info, skipping verification.")
            await self._report_progress(UpdateStatus.VERIFYING, 1.0, "Verification skipped (no hash provided).")
            return True

        await self._report_progress(UpdateStatus.VERIFYING, 0.0, "Verifying downloaded update...")

        expected_hash = self.update_info.file_hash
        # Assuming SHA256 for now
        calculated_hash = await asyncio.to_thread(calculate_file_hash, download_path, "sha256")

        if calculated_hash.lower() != expected_hash.lower():
            await self._report_progress(UpdateStatus.FAILED, 1.0, "Hash verification failed.")
            raise VerificationError(
                f"Hash mismatch. Expected: {expected_hash}, Got: {calculated_hash}")

        await self._report_progress(UpdateStatus.VERIFYING, 1.0, "Hash verification passed.")
        return True

    async def backup_current_installation(self) -> Path:
        """
        Back up the current installation asynchronously.

        Returns:
            Path: Path to the backup directory.
        """
        await self._report_progress(UpdateStatus.BACKING_UP, 0.0, "Backing up current installation...")

        timestamp = asyncio.to_thread(lambda: time.strftime("%Y%m%d_%H%M%S"))
        backup_dir = self.config.backup_dir / f"backup_{self.config.current_version}_{await timestamp}"
        await asyncio.to_thread(backup_dir.mkdir, parents=True, exist_ok=True)

        try:
            # This is a simplified backup. For a real app, you'd copy specific files/dirs.
            # For now, we'll just copy the install_dir to the backup_dir.
            await asyncio.to_thread(shutil.copytree, self.config.install_dir, backup_dir, dirs_exist_ok=True)
            await self._report_progress(UpdateStatus.BACKING_UP, 1.0, f"Backup complete: {backup_dir}")
            return backup_dir
        except Exception as e:
            await self._report_progress(UpdateStatus.FAILED, 0.0, f"Backup failed: {e}")
            raise InstallationError(
                f"Failed to backup current installation: {e}") from e

    async def extract_update(self, download_path: Path) -> Path:
        """
        Extract the update archive using the configured package handler.

        Args:
            download_path: Path to the downloaded archive.

        Returns:
            Path: Path to the extraction directory.
        """
        extract_dir = self.config.temp_dir / "extracted"
        await asyncio.to_thread(shutil.rmtree, extract_dir, ignore_errors=True)
        await asyncio.to_thread(extract_dir.mkdir, parents=True, exist_ok=True)

        await self.config.package_handler.extract(download_path, extract_dir, self._report_progress)
        return extract_dir

    async def install_update(self, extract_dir: Path) -> bool:
        """
        Install the extracted update files.

        Args:
            extract_dir: Path to the extracted files.

        Returns:
            bool: True if installation was successful.
        """
        await self._report_progress(UpdateStatus.INSTALLING, 0.0, "Installing update files...")

        try:
            # This is a simplified installation. For a real app, you'd copy specific files/dirs.
            # For now, we'll just copy the extracted files to the install_dir.
            await asyncio.to_thread(shutil.copytree, extract_dir, self.config.install_dir, dirs_exist_ok=True)

            if self.update_info:
                await self._report_progress(UpdateStatus.COMPLETE, 1.0, f"Update to version {self.update_info.version} installed successfully.")
            else:
                await self._report_progress(UpdateStatus.COMPLETE, 1.0, "Update installed successfully.")

            # Run post-install hook if defined
            if "post_install" in self.config.custom_hooks:
                await self._report_progress(UpdateStatus.FINALIZING, 0.9, "Running post-install hook...")
                await asyncio.to_thread(self.config.custom_hooks["post_install"])

            return True
        except Exception as e:
            await self._report_progress(UpdateStatus.FAILED, 0.0, f"Installation failed: {e}")
            raise InstallationError(f"Failed to install update: {e}") from e

    async def rollback(self, backup_dir: Path) -> bool:
        """
        Roll back to a previous backup asynchronously.

        Args:
            backup_dir: Path to the backup directory.

        Returns:
            bool: True if rollback was successful.
        """
        await self._report_progress(UpdateStatus.BACKING_UP, 0.0, f"Rolling back to backup: {backup_dir}")
        try:
            if not await asyncio.to_thread(backup_dir.exists):
                raise InstallationError(
                    f"Backup directory not found: {backup_dir}")

            # Clear current installation directory (be careful with this in real apps!)
            for item in await asyncio.to_thread(self.config.install_dir.iterdir):
                if await asyncio.to_thread(item.is_dir):
                    await asyncio.to_thread(shutil.rmtree, item)
                else:
                    await asyncio.to_thread(item.unlink)

            # Copy backup back to install_dir
            await asyncio.to_thread(shutil.copytree, backup_dir, self.config.install_dir, dirs_exist_ok=True)

            await self._report_progress(UpdateStatus.ROLLED_BACK, 1.0, f"Rollback from {self.config.current_version} complete.")
            return True
        except Exception as e:
            await self._report_progress(UpdateStatus.FAILED, 0.0, f"Rollback failed: {e}")
            raise InstallationError(f"Failed to rollback: {e}") from e

    async def update(self) -> bool:
        """
        Execute the full update process asynchronously.

        Returns:
            bool: True if update was successful, False if no update was needed or update failed.
        """
        try:
            if "pre_update" in self.config.custom_hooks:
                await self._report_progress(UpdateStatus.FINALIZING, 0.0, "Running pre-update hook...")
                await asyncio.to_thread(self.config.custom_hooks["pre_update"])

            update_available = await self.check_for_updates()
            if not update_available:
                return False

            download_path = await self.download_update()
            await self.verify_update(download_path)

            if "post_download" in self.config.custom_hooks:
                await self._report_progress(UpdateStatus.FINALIZING, 0.5, "Running post-download hook...")
                await asyncio.to_thread(self.config.custom_hooks["post_download"])

            backup_dir = await self.backup_current_installation()
            extract_dir = await self.extract_update(download_path)

            try:
                await self.install_update(extract_dir)
                if "post_install" in self.config.custom_hooks:
                    await self._report_progress(UpdateStatus.FINALIZING, 1.0, "Running post-install hook...")
                    await asyncio.to_thread(self.config.custom_hooks["post_install"])
                return True
            except InstallationError:
                logger.warning("Installation failed, attempting rollback...")
                await self.rollback(backup_dir)
                raise

        except Exception as e:
            await self._report_progress(UpdateStatus.FAILED, 0.0, f"Update process failed: {e}")
            raise
        finally:
            await self.cleanup()

    async def cleanup(self) -> None:
        """
        Clean up temporary files and resources asynchronously.
        """
        try:
            if self.config.temp_dir.exists():
                await asyncio.to_thread(shutil.rmtree, self.config.temp_dir, ignore_errors=True)
                await asyncio.to_thread(self.config.temp_dir.mkdir, parents=True, exist_ok=True)
        except Exception as e:
            logger.warning(f"Cleanup failed: {e}")
