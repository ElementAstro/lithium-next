#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Data models for the build system helper with enhanced type safety and performance.
"""

from __future__ import annotations

import time
from enum import StrEnum, auto
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Union, Any, Protocol, runtime_checkable
from collections.abc import Mapping

from loguru import logger


class BuildStatus(StrEnum):
    """Enumeration of possible build status values using StrEnum for better serialization."""
    NOT_STARTED = "not_started"
    CONFIGURING = "configuring"
    BUILDING = "building"
    TESTING = "testing"
    INSTALLING = "installing"
    CLEANING = "cleaning"
    GENERATING_DOCS = "generating_docs"
    COMPLETED = "completed"
    FAILED = "failed"

    def is_terminal(self) -> bool:
        """Check if this status represents a terminal state."""
        return self in {BuildStatus.COMPLETED, BuildStatus.FAILED}

    def is_active(self) -> bool:
        """Check if this status represents an active/running state."""
        return self in {
            BuildStatus.CONFIGURING,
            BuildStatus.BUILDING,
            BuildStatus.TESTING,
            BuildStatus.INSTALLING,
            BuildStatus.CLEANING,
            BuildStatus.GENERATING_DOCS
        }


@dataclass(frozen=True, slots=True)
class BuildResult:
    """
    Immutable data class to store build operation results with enhanced metrics.
    
    Uses slots for memory efficiency and frozen=True for immutability.
    """
    success: bool
    output: str
    error: str = ""
    exit_code: int = 0
    execution_time: float = 0.0
    timestamp: float = field(default_factory=time.time)
    memory_usage: Optional[int] = None  # Peak memory usage in bytes
    cpu_time: Optional[float] = None    # CPU time in seconds

    def __post_init__(self) -> None:
        """Validate the BuildResult after initialization."""
        if self.execution_time < 0:
            raise ValueError("execution_time cannot be negative")
        if self.exit_code < 0:
            raise ValueError("exit_code cannot be negative")

    @property
    def failed(self) -> bool:
        """Convenience property to check if the build failed."""
        return not self.success

    @property
    def duration_ms(self) -> float:
        """Get execution time in milliseconds."""
        return self.execution_time * 1000

    def log_result(self, operation: str) -> None:
        """Log the build result with structured data."""
        log_data = {
            "operation": operation,
            "success": self.success,
            "exit_code": self.exit_code,
            "execution_time": self.execution_time,
            "timestamp": self.timestamp
        }
        
        if self.memory_usage:
            log_data["memory_usage_mb"] = self.memory_usage / (1024 * 1024)
        if self.cpu_time:
            log_data["cpu_time"] = self.cpu_time

        if self.success:
            logger.success(f"{operation} completed successfully", **log_data)
        else:
            logger.error(f"{operation} failed", **log_data)

    def to_dict(self) -> Dict[str, Any]:
        """Convert BuildResult to dictionary for serialization."""
        return {
            "success": self.success,
            "output": self.output,
            "error": self.error,
            "exit_code": self.exit_code,
            "execution_time": self.execution_time,
            "timestamp": self.timestamp,
            "memory_usage": self.memory_usage,
            "cpu_time": self.cpu_time
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> BuildResult:
        """Create BuildResult from dictionary."""
        return cls(
            success=data["success"],
            output=data["output"],
            error=data.get("error", ""),
            exit_code=data.get("exit_code", 0),
            execution_time=data.get("execution_time", 0.0),
            timestamp=data.get("timestamp", time.time()),
            memory_usage=data.get("memory_usage"),
            cpu_time=data.get("cpu_time")
        )


@runtime_checkable
class BuildOptionsProtocol(Protocol):
    """Protocol defining the interface for build options."""
    
    source_dir: Path
    build_dir: Path
    install_prefix: Optional[Path]
    build_type: str
    generator: Optional[str]
    options: List[str]
    env_vars: Dict[str, str]
    verbose: bool
    parallel: int
    target: Optional[str]


class BuildOptions(Dict[str, Any]):
    """
    Enhanced build options dictionary with type validation and defaults.
    
    Inherits from Dict for backward compatibility while adding type safety.
    """
    
    _REQUIRED_KEYS = {"source_dir", "build_dir"}
    _DEFAULT_VALUES = {
        "build_type": "Debug",
        "verbose": False,
        "parallel": 4,
        "options": [],
        "env_vars": {},
    }

    def __init__(self, data: Optional[Mapping[str, Any]] = None, **kwargs: Any) -> None:
        """Initialize BuildOptions with validation."""
        # Start with defaults
        super().__init__(self._DEFAULT_VALUES)
        
        # Update with provided data
        if data:
            self.update(data)
        if kwargs:
            self.update(kwargs)
            
        # Validate and normalize
        self._validate_and_normalize()

    def _validate_and_normalize(self) -> None:
        """Validate and normalize build options."""
        # Check required keys
        missing_keys = self._REQUIRED_KEYS - set(self.keys())
        if missing_keys:
            raise ValueError(f"Missing required build options: {missing_keys}")

        # Normalize paths
        for key in ["source_dir", "build_dir", "install_prefix"]:
            if key in self and self[key] is not None:
                self[key] = Path(self[key]).resolve()

        # Validate parallel value
        if "parallel" in self:
            parallel = self["parallel"]
            if not isinstance(parallel, int) or parallel < 1:
                raise ValueError(f"parallel must be a positive integer, got {parallel}")

        # Normalize options list
        if "options" in self and not isinstance(self["options"], list):
            raise ValueError("options must be a list")

        # Normalize env_vars dict
        if "env_vars" in self and not isinstance(self["env_vars"], dict):
            raise ValueError("env_vars must be a dictionary")

    @property
    def source_dir(self) -> Path:
        """Get source directory as Path."""
        return self["source_dir"]

    @property
    def build_dir(self) -> Path:
        """Get build directory as Path."""
        return self["build_dir"]

    @property
    def install_prefix(self) -> Optional[Path]:
        """Get install prefix as Path."""
        return self.get("install_prefix")

    @property
    def build_type(self) -> str:
        """Get build type."""
        return self["build_type"]

    @property
    def generator(self) -> Optional[str]:
        """Get generator."""
        return self.get("generator")

    @property
    def options(self) -> List[str]:
        """Get build options list."""
        return self["options"]

    @property
    def env_vars(self) -> Dict[str, str]:
        """Get environment variables."""
        return self["env_vars"]

    @property
    def verbose(self) -> bool:
        """Get verbose flag."""
        return self["verbose"]

    @property
    def parallel(self) -> int:
        """Get parallel jobs count."""
        return self["parallel"]

    @property
    def target(self) -> Optional[str]:
        """Get build target."""
        return self.get("target")

    def with_overrides(self, **overrides: Any) -> BuildOptions:
        """Create a new BuildOptions with specified overrides."""
        new_data = dict(self)
        new_data.update(overrides)
        return BuildOptions(new_data)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to plain dictionary with serializable values."""
        result = dict(self)
        # Convert Path objects to strings for serialization
        for key, value in result.items():
            if isinstance(value, Path):
                result[key] = str(value)
        return result

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> BuildOptions:
        """Create BuildOptions from dictionary."""
        return cls(data)


@dataclass(frozen=True, slots=True)
class BuildMetrics:
    """Performance metrics for build operations."""
    
    total_time: float
    configure_time: float = 0.0
    build_time: float = 0.0
    test_time: float = 0.0
    install_time: float = 0.0
    peak_memory_mb: float = 0.0
    cpu_usage_percent: float = 0.0
    artifacts_count: int = 0
    artifacts_size_mb: float = 0.0

    def __post_init__(self) -> None:
        """Validate metrics."""
        if self.total_time < 0:
            raise ValueError("total_time cannot be negative")
        if any(t < 0 for t in [self.configure_time, self.build_time, self.test_time, self.install_time]):
            raise ValueError("Individual operation times cannot be negative")

    def efficiency_ratio(self) -> float:
        """Calculate build efficiency as a ratio of useful work to total time."""
        if self.total_time == 0:
            return 0.0
        useful_time = self.configure_time + self.build_time + self.test_time + self.install_time
        return useful_time / self.total_time

    def to_dict(self) -> Dict[str, float]:
        """Convert metrics to dictionary."""
        return {
            "total_time": self.total_time,
            "configure_time": self.configure_time,
            "build_time": self.build_time,
            "test_time": self.test_time,
            "install_time": self.install_time,
            "peak_memory_mb": self.peak_memory_mb,
            "cpu_usage_percent": self.cpu_usage_percent,
            "artifacts_count": float(self.artifacts_count),
            "artifacts_size_mb": self.artifacts_size_mb,
            "efficiency_ratio": self.efficiency_ratio()
        }


@dataclass
class BuildSession:
    """Context manager for tracking an entire build session."""
    
    session_id: str
    start_time: float = field(default_factory=time.time)
    end_time: Optional[float] = None
    status: BuildStatus = BuildStatus.NOT_STARTED
    results: List[BuildResult] = field(default_factory=list)
    metrics: Optional[BuildMetrics] = None
    
    def __enter__(self) -> BuildSession:
        """Enter build session context."""
        self.start_time = time.time()
        logger.info(f"Starting build session {self.session_id}")
        return self

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """Exit build session context."""
        self.end_time = time.time()
        duration = self.end_time - self.start_time
        
        if exc_type is None:
            self.status = BuildStatus.COMPLETED
            logger.success(f"Build session {self.session_id} completed in {duration:.2f}s")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Build session {self.session_id} failed after {duration:.2f}s")

    def add_result(self, result: BuildResult) -> None:
        """Add a build result to this session."""
        self.results.append(result)

    @property
    def duration(self) -> Optional[float]:
        """Get session duration in seconds."""
        if self.end_time is None:
            return None
        return self.end_time - self.start_time

    @property
    def success_rate(self) -> float:
        """Calculate success rate of operations in this session."""
        if not self.results:
            return 0.0
        successful = sum(1 for r in self.results if r.success)
        return successful / len(self.results)