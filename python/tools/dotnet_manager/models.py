"""Enhanced models for the .NET Framework Manager with modern Python features."""

from __future__ import annotations
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, Any, Protocol, runtime_checkable
from pathlib import Path
import platform


class HashAlgorithm(str, Enum):
    """Supported hash algorithms for file verification."""

    MD5 = "md5"
    SHA1 = "sha1"
    SHA256 = "sha256"
    SHA512 = "sha512"


class DotNetManagerError(Exception):
    """Base exception for .NET Manager operations with enhanced context."""
    
    def __init__(
        self, 
        message: str, 
        *,
        error_code: Optional[str] = None,
        file_path: Optional[Path] = None,
        original_error: Optional[Exception] = None,
        **context: Any
    ) -> None:
        super().__init__(message)
        self.error_code = error_code
        self.file_path = file_path
        self.original_error = original_error
        self.context = context
    
    def __str__(self) -> str:
        parts = [super().__str__()]
        if self.error_code:
            parts.append(f"Code: {self.error_code}")
        if self.file_path:
            parts.append(f"File: {self.file_path}")
        if self.original_error:
            parts.append(f"Cause: {self.original_error}")
        return " | ".join(parts)
    
    def to_dict(self) -> dict[str, Any]:
        """Convert exception to dictionary for structured logging."""
        return {
            "message": str(self.args[0]) if self.args else "",
            "error_code": self.error_code,
            "file_path": str(self.file_path) if self.file_path else None,
            "original_error": str(self.original_error) if self.original_error else None,
            "context": self.context,
            "exception_type": self.__class__.__name__
        }


class UnsupportedPlatformError(DotNetManagerError):
    """Raised when operations are attempted on unsupported platforms."""
    
    def __init__(self, platform_name: str) -> None:
        super().__init__(
            f"This operation is not supported on {platform_name}. Windows is required.",
            error_code="UNSUPPORTED_PLATFORM",
            platform=platform_name
        )


class RegistryAccessError(DotNetManagerError):
    """Raised when registry access operations fail."""
    
    def __init__(
        self, 
        message: str, 
        *, 
        registry_path: Optional[str] = None,
        original_error: Optional[Exception] = None
    ) -> None:
        super().__init__(
            message, 
            error_code="REGISTRY_ACCESS_ERROR",
            original_error=original_error,
            registry_path=registry_path
        )


class DownloadError(DotNetManagerError):
    """Raised when download operations fail."""
    
    def __init__(
        self, 
        message: str, 
        *, 
        url: Optional[str] = None,
        file_path: Optional[Path] = None,
        original_error: Optional[Exception] = None
    ) -> None:
        super().__init__(
            message, 
            error_code="DOWNLOAD_ERROR",
            file_path=file_path,
            original_error=original_error,
            url=url
        )


class ChecksumError(DotNetManagerError):
    """Raised when checksum verification fails."""
    
    def __init__(
        self, 
        message: str, 
        *, 
        file_path: Optional[Path] = None,
        expected_checksum: Optional[str] = None,
        actual_checksum: Optional[str] = None,
        algorithm: Optional[HashAlgorithm] = None
    ) -> None:
        super().__init__(
            message, 
            error_code="CHECKSUM_ERROR",
            file_path=file_path,
            expected_checksum=expected_checksum,
            actual_checksum=actual_checksum,
            algorithm=algorithm.value if algorithm else None
        )


class InstallationError(DotNetManagerError):
    """Raised when installation operations fail."""
    
    def __init__(
        self, 
        message: str, 
        *, 
        installer_path: Optional[Path] = None,
        version_key: Optional[str] = None,
        original_error: Optional[Exception] = None
    ) -> None:
        super().__init__(
            message, 
            error_code="INSTALLATION_ERROR",
            file_path=installer_path,
            original_error=original_error,
            version_key=version_key
        )


@runtime_checkable
class VersionComparable(Protocol):
    """Protocol for objects that can be compared by version."""
    
    def __lt__(self, other: VersionComparable) -> bool: ...
    def __le__(self, other: VersionComparable) -> bool: ...
    def __gt__(self, other: VersionComparable) -> bool: ...
    def __ge__(self, other: VersionComparable) -> bool: ...


@dataclass
class DotNetVersion:
    """Represents a .NET Framework version with enhanced functionality."""
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
    
    def __lt__(self, other: DotNetVersion) -> bool:
        """Compare versions by release number for sorting."""
        if not isinstance(other, DotNetVersion):
            return NotImplemented
        
        self_release = self.release or 0
        other_release = other.release or 0
        return self_release < other_release
    
    def __le__(self, other: DotNetVersion) -> bool:
        return self < other or self == other
    
    def __gt__(self, other: DotNetVersion) -> bool:
        return not self <= other
    
    def __ge__(self, other: DotNetVersion) -> bool:
        return not self < other
    
    def __eq__(self, other: object) -> bool:
        if not isinstance(other, DotNetVersion):
            return NotImplemented
        return self.key == other.key and self.release == other.release
    
    def __hash__(self) -> int:
        return hash((self.key, self.release))
    
    @property
    def is_downloadable(self) -> bool:
        """Check if this version can be downloaded."""
        return bool(self.installer_url and self.installer_sha256)
    
    @property
    def version_number(self) -> str:
        """Extract numeric version from key (e.g., "4.8" from "v4.8")."""
        return self.key.lstrip('v')
    
    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        return {
            "key": self.key,
            "name": self.name,
            "release": self.release,
            "service_pack": self.service_pack,
            "installer_url": self.installer_url,
            "installer_sha256": self.installer_sha256,
            "min_windows_version": self.min_windows_version,
            "is_downloadable": self.is_downloadable,
            "version_number": self.version_number
        }


@dataclass
class SystemInfo:
    """Encapsulates comprehensive information about the current system."""
    os_name: str
    os_version: str
    os_build: str
    architecture: str
    installed_versions: list[DotNetVersion] = field(default_factory=list)
    platform_compatible: bool = field(init=False)
    
    def __post_init__(self) -> None:
        """Set platform compatibility after initialization."""
        self.platform_compatible = self.os_name.lower() == "windows"
    
    @property
    def latest_installed_version(self) -> Optional[DotNetVersion]:
        """Get the latest installed .NET version."""
        if not self.installed_versions:
            return None
        return max(self.installed_versions, key=lambda v: v.release or 0)
    
    @property
    def installed_version_count(self) -> int:
        """Get the count of installed versions."""
        return len(self.installed_versions)
    
    def has_version(self, version_key: str) -> bool:
        """Check if a specific version is installed."""
        return any(v.key == version_key for v in self.installed_versions)
    
    def get_version(self, version_key: str) -> Optional[DotNetVersion]:
        """Get a specific installed version by key."""
        for version in self.installed_versions:
            if version.key == version_key:
                return version
        return None
    
    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        return {
            "os_name": self.os_name,
            "os_version": self.os_version,
            "os_build": self.os_build,
            "architecture": self.architecture,
            "platform_compatible": self.platform_compatible,
            "installed_version_count": self.installed_version_count,
            "latest_installed_version": (
                self.latest_installed_version.to_dict() 
                if self.latest_installed_version else None
            ),
            "installed_versions": [v.to_dict() for v in self.installed_versions]
        }


@dataclass
class DownloadResult:
    """Represents the result of a download operation with enhanced metadata."""
    path: str
    size: int
    checksum_matched: Optional[bool] = None
    download_time_seconds: Optional[float] = None
    average_speed_mbps: Optional[float] = None
    
    @property
    def size_mb(self) -> float:
        """Get size in megabytes."""
        return self.size / (1024 * 1024)
    
    @property
    def success(self) -> bool:
        """Check if download was successful."""
        return Path(self.path).exists() and (
            self.checksum_matched is None or self.checksum_matched
        )
    
    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        return {
            "path": self.path,
            "size": self.size,
            "size_mb": self.size_mb,
            "checksum_matched": self.checksum_matched,
            "download_time_seconds": self.download_time_seconds,
            "average_speed_mbps": self.average_speed_mbps,
            "success": self.success
        }


@dataclass
class InstallationResult:
    """Represents the result of an installation operation."""
    success: bool
    version_key: str
    installer_path: Optional[Path] = None
    error_message: Optional[str] = None
    return_code: Optional[int] = None
    
    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        return {
            "success": self.success,
            "version_key": self.version_key,
            "installer_path": str(self.installer_path) if self.installer_path else None,
            "error_message": self.error_message,
            "return_code": self.return_code
        }