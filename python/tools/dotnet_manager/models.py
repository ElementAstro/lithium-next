"""Models for the .NET Framework Manager."""

from dataclasses import dataclass
from enum import Enum
from typing import Optional


class HashAlgorithm(str, Enum):
    """Supported hash algorithms for file verification."""

    MD5 = "md5"
    SHA1 = "sha1"
    SHA256 = "sha256"
    SHA512 = "sha512"


@dataclass
class DotNetVersion:
    """Represents a .NET Framework version with related metadata."""

    key: str  # Registry key component (e.g., "v4.8")
    name: str  # Human-readable name (e.g., ".NET Framework 4.8")
    release: Optional[str] = None  # Specific release version
    installer_url: Optional[str] = None  # URL to download the installer
    # Expected SHA256 hash of the installer
    installer_sha256: Optional[str] = None

    def __str__(self) -> str:
        """String representation of the .NET version."""
        return f"{self.name} ({self.release or 'unknown'})"
