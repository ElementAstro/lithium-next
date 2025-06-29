#!/usr/bin/env python3
"""
Data models for the Pacman Package Manager
"""

from enum import Enum, auto
from dataclasses import dataclass, field
from typing import List, Dict, Optional, TypedDict


class PackageStatus(Enum):
    """Enum representing the status of a package"""

    INSTALLED = auto()
    NOT_INSTALLED = auto()
    OUTDATED = auto()
    PARTIALLY_INSTALLED = auto()


@dataclass
class PackageInfo:
    """Data class to store package information"""

    name: str
    version: str
    description: str = ""
    install_size: str = ""
    installed: bool = False
    repository: str = ""
    dependencies: List[str] = field(default_factory=list)
    optional_dependencies: Optional[List[str]] = field(default_factory=list)
    build_date: str = ""
    install_date: Optional[str] = None

    def __post_init__(self):
        """Initialize default lists"""
        if self.dependencies is None:
            self.dependencies = []
        if self.optional_dependencies is None:
            self.optional_dependencies = []


class CommandResult(TypedDict):
    """Type definition for command execution results"""

    success: bool
    stdout: str
    stderr: str
    command: List[str]
    return_code: int
