# packaging.py
"""Defines handlers for different types of update packages."""

import zipfile
from pathlib import Path
from typing import Optional
import aiofiles

from .models import ProgressCallback, UpdateStatus, InstallationError


class ZipPackageHandler:
    """Handles ZIP archive packages."""

    async def extract(
        self,
        archive_path: Path,
        extract_to: Path,
        progress_callback: Optional[ProgressCallback],
    ) -> None:
        """Extracts a ZIP archive with progress reporting."""
        try:
            if progress_callback:
                await progress_callback(
                    UpdateStatus.EXTRACTING, 0.0, "Starting extraction..."
                )

            with zipfile.ZipFile(archive_path, "r") as zip_ref:
                total_files = len(zip_ref.infolist())
                for i, file_info in enumerate(zip_ref.infolist()):
                    zip_ref.extract(file_info, extract_to)
                    if progress_callback and i % 10 == 0:
                        progress = (i + 1) / total_files
                        await progress_callback(
                            UpdateStatus.EXTRACTING,
                            progress,
                            f"Extracted {i+1}/{total_files} files",
                        )

            if progress_callback:
                await progress_callback(
                    UpdateStatus.EXTRACTING, 1.0, "Extraction complete."
                )

        except zipfile.BadZipFile as e:
            raise InstallationError(f"Invalid ZIP file: {e}") from e
        except Exception as e:
            raise InstallationError(f"Failed to extract archive: {e}") from e
