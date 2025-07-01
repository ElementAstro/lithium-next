# models.py
"""Defines the core data models and types for the auto-updater."""

import os
from enum import Enum
from pathlib import Path
import tempfile
from typing import Any, Dict, Literal, Optional, Union, Callable, Protocol, List

from pydantic import BaseModel, Field, HttpUrl, DirectoryPath, FilePath, validator

# --- Type Definitions ---
PathLike = Union[str, os.PathLike, Path]
HashType = Literal["sha256", "sha512", "md5"]

# --- Enums ---


class UpdateStatus(str, Enum):
    """Status codes for the update process."""
    IDLE = "idle"
    CHECKING = "checking"
    UP_TO_DATE = "up_to_date"
    UPDATE_AVAILABLE = "update_available"
    DOWNLOADING = "downloading"
    VERIFYING = "verifying"
    BACKING_UP = "backing_up"
    EXTRACTING = "extracting"
    INSTALLING = "installing"
    FINALIZING = "finalizing"
    COMPLETE = "complete"
    FAILED = "failed"
    ROLLED_BACK = "rolled_back"

# --- Exceptions ---


class UpdaterError(Exception):
    """Base exception for all updater errors."""
    pass


class NetworkError(UpdaterError):
    """For network-related errors."""
    pass


class VerificationError(UpdaterError):
    """For verification failures."""
    pass


class InstallationError(UpdaterError):
    """For installation failures."""
    pass

# --- Protocols and Interfaces ---


class ProgressCallback(Protocol):
    """Protocol for progress callback functions."""

    async def __call__(self, status: UpdateStatus,
                       progress: float, message: str) -> None: ...


class UpdateInfo(BaseModel):
    """Structured information about an available update."""
    version: str
    download_url: HttpUrl
    file_hash: Optional[str] = None
    release_notes: Optional[str] = None
    release_date: Optional[str] = None


class UpdateStrategy(Protocol):
    """Protocol for defining update-checking strategies."""

    async def check_for_updates(
        self, current_version: str) -> Optional[UpdateInfo]: ...


class PackageHandler(Protocol):
    """Protocol for handling different types of update packages."""

    async def extract(self, archive_path: Path, extract_to: Path,
                      progress_callback: Optional[ProgressCallback]) -> None: ...

# --- Configuration Model ---


class UpdaterConfig(BaseModel):
    """Configuration for the AutoUpdater, validated by Pydantic."""
    strategy: UpdateStrategy
    package_handler: PackageHandler
    install_dir: DirectoryPath
    current_version: str
    temp_dir: Path = Field(default_factory=lambda: Path(
        tempfile.gettempdir()) / "auto_updater_temp")
    backup_dir: Path = Field(default_factory=lambda: Path(
        tempfile.gettempdir()) / "auto_updater_backup")
    progress_callback: Optional[ProgressCallback] = None
    custom_hooks: Dict[str, Callable[[], Any]] = Field(default_factory=dict)

    class Config:
        arbitrary_types_allowed = True

    @validator('install_dir', 'temp_dir', 'backup_dir', pre=True)
    def _ensure_path(cls, v: Any) -> Path:
        return Path(v).resolve()

    def __post_init_post_parse__(self):
        """Create directories after validation."""
        self.temp_dir.mkdir(parents=True, exist_ok=True)
        self.backup_dir.mkdir(parents=True, exist_ok=True)
