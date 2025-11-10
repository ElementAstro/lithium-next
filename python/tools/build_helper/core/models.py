#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Data models for the build system helper.
"""

from enum import Enum, auto
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, TypedDict, Optional, Union


class BuildStatus(Enum):
    """Enumeration of possible build status values."""

    NOT_STARTED = auto()
    CONFIGURING = auto()
    BUILDING = auto()
    TESTING = auto()
    INSTALLING = auto()
    CLEANING = auto()
    GENERATING_DOCS = auto()
    COMPLETED = auto()
    FAILED = auto()


@dataclass
class BuildResult:
    """Data class to store build operation results."""

    success: bool
    output: str
    error: str = ""
    exit_code: int = 0
    execution_time: float = 0.0

    @property
    def failed(self) -> bool:
        """Convenience property to check if the build failed."""
        return not self.success


class BuildOptions(TypedDict, total=False):
    """Type definition for build options dictionary."""

    source_dir: Path
    build_dir: Path
    install_prefix: Path
    build_type: str
    generator: str
    options: List[str]
    env_vars: Dict[str, str]
    verbose: bool
    parallel: int
    target: str
