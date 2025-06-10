# types.py
from enum import Enum
from typing import Any, Dict, Optional, Union, Callable, List, Tuple, Protocol, TypedDict, Literal
from dataclasses import dataclass
from pathlib import Path
import os

# Type definitions
VersionType = Union[str, Tuple[int, ...]]
HashType = Literal["sha256", "sha512", "md5"]
PathLike = Union[str, os.PathLike, Path]


class UpdateStatus(Enum):
    """Status codes for the update process."""
    CHECKING = "checking"
    UP_TO_DATE = "up_to_date"
    UPDATE_AVAILABLE = "update_available"
    DOWNLOADING = "downloading"
    VERIFYING = "verifying"
    BACKING_UP = "backing_up"
    INSTALLING = "installing"
    EXTRACTING = "extracting"
    FINALIZING = "finalizing"
    COMPLETE = "complete"
    FAILED = "failed"
    ROLLED_BACK = "rolled_back"


# Exception classes for better error handling
class UpdaterError(Exception):
    """Base exception class for all updater errors."""
    pass


class NetworkError(UpdaterError):
    """Exception raised for network-related errors."""
    pass


class VerificationError(UpdaterError):
    """Exception raised for verification failures."""
    pass


class InstallationError(UpdaterError):
    """Exception raised for installation failures."""
    pass


class ProgressCallback(Protocol):
    """Protocol defining the structure for progress callback functions."""

    def __call__(self, status: UpdateStatus,
                 progress: float, message: str) -> None: ...


@dataclass
class UpdaterConfig:
    """Configuration for the AutoUpdater.

    Attributes:
        url (str): URL to check for updates
        install_dir (Path): Directory where the application is installed
        current_version (str): Current version of the application
        num_threads (int): Number of threads to use for downloading
        verify_hash (bool): Whether to verify file hash
        hash_algorithm (HashType): Hash algorithm to use for verification
        temp_dir (Optional[Path]): Directory for temporary files
        backup_dir (Optional[Path]): Directory for backups
    """
    url: str
    install_dir: Path
    current_version: str
    num_threads: int = 4
    verify_hash: bool = True
    hash_algorithm: HashType = "sha256"
    temp_dir: Optional[Path] = None
    backup_dir: Optional[Path] = None
    custom_params: Optional[Dict[str, Any]] = None

    def __post_init__(self) -> None:
        """Initialize derived attributes after instance creation."""
        # Convert string paths to Path objects
        if isinstance(self.install_dir, str):
            self.install_dir = Path(self.install_dir)

        # Ensure install_dir is absolute
        self.install_dir = self.install_dir.absolute()

        # Set default temp_dir if not provided
        if self.temp_dir is None:
            self.temp_dir = self.install_dir / "temp"
        elif isinstance(self.temp_dir, str):
            self.temp_dir = Path(self.temp_dir)

        # Set default backup_dir if not provided
        if self.backup_dir is None:
            self.backup_dir = self.install_dir / "backup"
        elif isinstance(self.backup_dir, str):
            self.backup_dir = Path(self.backup_dir)

        # Initialize custom_params if not provided
        if self.custom_params is None:
            self.custom_params = {}
