#!/usr/bin/env python3
"""
Core classes and definitions for Nginx Manager.
"""

from __future__ import annotations

from enum import Enum, auto
from dataclasses import dataclass
from pathlib import Path
from typing import Self, Any


class OperatingSystem(Enum):
    """Enum representing supported operating systems."""
    LINUX = auto()
    WINDOWS = auto()
    MACOS = auto()
    UNKNOWN = auto()

    @classmethod
    def from_platform(cls, platform_name: str) -> OperatingSystem:
        """Create OperatingSystem from platform string."""
        mapping = {
            "linux": cls.LINUX,
            "windows": cls.WINDOWS,
            "darwin": cls.MACOS,
        }
        return mapping.get(platform_name.lower(), cls.UNKNOWN)


class NginxError(Exception):
    """Base exception class for all Nginx-related errors."""

    def __init__(self, message: str, error_code: int | None = None, details: dict[str, Any] | None = None) -> None:
        super().__init__(message)
        self.error_code = error_code
        self.details = details or {}

    def __str__(self) -> str:
        base_msg = super().__str__()
        if self.error_code:
            base_msg += f" (Error Code: {self.error_code})"
        if self.details:
            details_str = ", ".join(
                f"{k}: {v}" for k, v in self.details.items())
            base_msg += f" - {details_str}"
        return base_msg


class ConfigError(NginxError):
    """Exception raised for Nginx configuration errors."""


class InstallationError(NginxError):
    """Exception raised for Nginx installation errors."""


class OperationError(NginxError):
    """Exception raised for failed Nginx operations."""


@dataclass(frozen=True, slots=True)
class NginxPaths:
    """Immutable class holding paths related to Nginx installation."""
    base_path: Path
    conf_path: Path
    binary_path: Path
    backup_path: Path
    sites_available: Path
    sites_enabled: Path
    logs_path: Path
    ssl_path: Path

    @classmethod
    def from_base_path(cls, base_path: Path, binary_path: Path, logs_path: Path) -> Self:
        """Create NginxPaths from base path and derived paths."""
        return cls(
            base_path=base_path,
            conf_path=base_path / "nginx.conf",
            binary_path=binary_path,
            backup_path=base_path / "backup",
            sites_available=base_path / "sites-available",
            sites_enabled=base_path / "sites-enabled",
            logs_path=logs_path,
            ssl_path=base_path / "ssl",
        )

    def ensure_directories(self) -> None:
        """Ensure all necessary directories exist."""
        for path_attr in ["backup_path", "sites_available", "sites_enabled", "ssl_path"]:
            path = getattr(self, path_attr)
            path.mkdir(parents=True, exist_ok=True)
