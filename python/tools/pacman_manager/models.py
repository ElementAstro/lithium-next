#!/usr/bin/env python3
"""
Enhanced data models for the Pacman Package Manager.
Uses modern Python features including slots, frozen dataclasses, and improved type hints.
"""

from __future__ import annotations

import time
from enum import Enum, StrEnum, auto
from dataclasses import dataclass, field
from typing import TypedDict, Self, ClassVar, Callable, Any
from datetime import datetime, timezone
from pathlib import Path

from .pacman_types import PackageName, PackageVersion, RepositoryName, CommandOutput


class PackageStatus(StrEnum):
    """Enum representing the status of a package with string values."""

    INSTALLED = "installed"
    NOT_INSTALLED = "not_installed"
    OUTDATED = "outdated"
    PARTIALLY_INSTALLED = "partially_installed"
    UPGRADE_AVAILABLE = "upgrade_available"
    DEPENDENCY_MISSING = "dependency_missing"
    CONFLICTED = "conflicted"


class PackagePriority(Enum):
    """Priority levels for package operations."""

    LOW = auto()
    NORMAL = auto()
    HIGH = auto()
    CRITICAL = auto()


@dataclass(frozen=True, slots=True)
class Dependency:
    """Represents a package dependency with version constraints."""

    name: PackageName
    version_constraint: str = ""
    optional: bool = False

    def __str__(self) -> str:
        suffix = f" ({self.version_constraint})" if self.version_constraint else ""
        prefix = "[optional] " if self.optional else ""
        return f"{prefix}{self.name}{suffix}"


@dataclass(slots=True, kw_only=True)
class PackageInfo:
    """Enhanced data class to store comprehensive package information."""

    # Required fields
    name: PackageName
    version: PackageVersion

    # Basic info with defaults
    description: str = ""
    install_size: int = 0  # Size in bytes
    download_size: int = 0  # Size in bytes
    installed: bool = False
    repository: RepositoryName = RepositoryName("")

    # Dependency information
    dependencies: list[Dependency] = field(default_factory=list)
    optional_dependencies: list[Dependency] = field(default_factory=list)
    provides: list[PackageName] = field(default_factory=list)
    conflicts: list[PackageName] = field(default_factory=list)
    replaces: list[PackageName] = field(default_factory=list)

    # Metadata
    build_date: datetime | None = None
    install_date: datetime | None = None
    last_update: datetime | None = None
    maintainer: str = ""
    homepage: str = ""
    license: str = ""
    architecture: str = ""

    # Package status and priority
    status: PackageStatus = PackageStatus.NOT_INSTALLED
    priority: PackagePriority = PackagePriority.NORMAL

    # Files and paths
    files: list[Path] = field(default_factory=list)
    backup_files: list[Path] = field(default_factory=list)

    # Advanced metadata
    checksum: str = ""
    signature: str = ""
    groups: list[str] = field(default_factory=list)
    keywords: list[str] = field(default_factory=list)

    # Class variables
    _FIELD_FORMATTERS: ClassVar[dict[str, Callable[[int], str]]] = {
        "install_size": lambda x: f"{x / 1024 / 1024:.2f} MB" if x > 0 else "Unknown",
        "download_size": lambda x: f"{x / 1024 / 1024:.2f} MB" if x > 0 else "Unknown",
    }

    def __post_init__(self) -> None:
        """Post-initialization processing."""
        # Convert string dates to datetime objects if needed
        if isinstance(self.build_date, str) and self.build_date:
            self.build_date = datetime.fromisoformat(self.build_date)
        if isinstance(self.install_date, str) and self.install_date:
            self.install_date = datetime.fromisoformat(self.install_date)

        # Set status based on install status if not explicitly set
        if self.status == PackageStatus.NOT_INSTALLED and self.installed:
            self.status = PackageStatus.INSTALLED

    @property
    def formatted_install_size(self) -> str:
        """Get human-readable install size."""
        return self._FIELD_FORMATTERS["install_size"](self.install_size)

    @property
    def formatted_download_size(self) -> str:
        """Get human-readable download size."""
        return self._FIELD_FORMATTERS["download_size"](self.download_size)

    @property
    def total_dependencies(self) -> int:
        """Get total number of dependencies."""
        return len(self.dependencies) + len(self.optional_dependencies)

    @property
    def is_installed(self) -> bool:
        """Check if package is installed."""
        return self.status == PackageStatus.INSTALLED

    @property
    def needs_update(self) -> bool:
        """Check if package needs update."""
        return self.status in (PackageStatus.OUTDATED, PackageStatus.UPGRADE_AVAILABLE)

    def matches_filter(self, **kwargs) -> bool:
        """Check if package matches given filter criteria."""
        for key, value in kwargs.items():
            if hasattr(self, key):
                if getattr(self, key) != value:
                    return False
            elif (
                key == "keyword"
                and value.lower() not in " ".join(self.keywords).lower()
            ):
                return False
        return True

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary representation."""
        return {
            "name": str(self.name),
            "version": str(self.version),
            "description": self.description,
            "install_size": self.install_size,
            "download_size": self.download_size,
            "installed": self.installed,
            "repository": str(self.repository),
            "status": self.status.value,
            "priority": self.priority.name,
            "dependencies": [str(dep) for dep in self.dependencies],
            "optional_dependencies": [str(dep) for dep in self.optional_dependencies],
            "build_date": self.build_date.isoformat() if self.build_date else None,
            "install_date": (
                self.install_date.isoformat() if self.install_date else None
            ),
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> Self:
        """Create instance from dictionary."""
        # Convert string fields to appropriate types
        if "name" in data:
            data["name"] = PackageName(data["name"])
        if "version" in data:
            data["version"] = PackageVersion(data["version"])
        if "repository" in data:
            data["repository"] = RepositoryName(data["repository"])
        if "status" in data:
            data["status"] = PackageStatus(data["status"])
        if "priority" in data:
            data["priority"] = PackagePriority[data["priority"]]

        return cls(**data)


class CommandResult(TypedDict):
    """Enhanced type definition for command execution results."""

    success: bool
    stdout: CommandOutput
    stderr: CommandOutput
    command: list[str]
    return_code: int
    duration: float
    timestamp: float
    working_directory: str
    environment: dict[str, str] | None


@dataclass(frozen=True, slots=True)
class OperationSummary:
    """Summary of a package operation."""

    operation: str
    packages_affected: list[PackageName]
    success_count: int
    failure_count: int
    duration: float
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    @property
    def total_packages(self) -> int:
        """Total number of packages involved."""
        return len(self.packages_affected)

    @property
    def success_rate(self) -> float:
        """Success rate as percentage."""
        if self.total_packages == 0:
            return 100.0
        return (self.success_count / self.total_packages) * 100


@dataclass(slots=True)
class PackageCache:
    """Cache entry for package information."""

    package_info: PackageInfo
    cached_at: float = field(default_factory=time.time)
    ttl: float = 3600.0  # 1 hour default TTL
    access_count: int = 0

    def is_expired(self) -> bool:
        """Check if cache entry is expired."""
        return time.time() - self.cached_at > self.ttl

    def touch(self) -> None:
        """Update access time and count."""
        self.access_count += 1
        self.cached_at = time.time()


@dataclass(frozen=True, slots=True)
class RepositoryInfo:
    """Information about a package repository."""

    name: RepositoryName
    url: str
    enabled: bool = True
    priority: int = 0
    mirror_list: list[str] = field(default_factory=list)
    last_sync: datetime | None = None
    package_count: int = 0

    @property
    def is_synced(self) -> bool:
        """Check if repository has been synced recently."""
        if not self.last_sync:
            return False
        age = datetime.now(timezone.utc) - self.last_sync
        return age.total_seconds() < 86400  # 24 hours


# Export all models
__all__ = [
    "PackageStatus",
    "PackagePriority",
    "Dependency",
    "PackageInfo",
    "CommandResult",
    "OperationSummary",
    "PackageCache",
    "RepositoryInfo",
]
