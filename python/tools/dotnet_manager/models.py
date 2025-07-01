"""Models for the .NET Framework Manager."""

from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, List


class HashAlgorithm(str, Enum):
    """Supported hash algorithms for file verification."""
    MD5 = "md5"
    SHA1 = "sha1"
    SHA256 = "sha256"
    SHA512 = "sha512"


@dataclass
class DotNetVersion:
    """Represents a .NET Framework version with related metadata."""
    key: str             # Registry key component (e.g., "v4.8")
    name: str            # Human-readable name (e.g., ".NET Framework 4.8")
    release: Optional[int] = None          # Specific release version number
    service_pack: Optional[int] = None     # Service pack level, if applicable
    installer_url: Optional[str] = None    # URL to download the installer
    installer_sha256: Optional[str] = None # Expected SHA256 hash of the installer
    min_windows_version: Optional[str] = None # Minimum required Windows version

    def __str__(self) -> str:
        """String representation of the .NET version."""
        version_str = f"{self.name} (Release: {self.release or 'N/A'})"
        if self.service_pack:
            version_str += f" SP{self.service_pack}"
        return version_str


@dataclass
class SystemInfo:
    """Encapsulates information about the current system."""
    os_name: str
    os_version: str
    os_build: str
    architecture: str
    installed_versions: List[DotNetVersion] = field(default_factory=list)


@dataclass
class DownloadResult:
    """Represents the result of a download operation."""
    path: str
    size: int
    checksum_matched: Optional[bool] = None