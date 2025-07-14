#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from dataclasses import dataclass
from enum import Enum, auto
from typing import Optional, List


class DependencyError(Exception):
    """Exception raised when a required dependency is missing."""

    pass


class PackageOperationError(Exception):
    """Exception raised when a package operation fails."""

    pass


class VersionError(Exception):
    """Exception raised when there's an issue with package versions."""

    pass


class OutputFormat(Enum):
    """Output format options for package information."""

    TEXT = auto()
    JSON = auto()
    TABLE = auto()
    MARKDOWN = auto()


@dataclass
class PackageInfo:
    """Data class for storing package information."""

    name: str
    version: Optional[str] = None
    latest_version: Optional[str] = None
    summary: Optional[str] = None
    homepage: Optional[str] = None
    author: Optional[str] = None
    author_email: Optional[str] = None
    license: Optional[str] = None
    requires: Optional[List[str]] = None
    required_by: Optional[List[str]] = None
    location: Optional[str] = None

    def __post_init__(self):
        """Initialize list attributes if they are None."""
        if self.requires is None:
            self.requires = []
        if self.required_by is None:
            self.required_by = []
