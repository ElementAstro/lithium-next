#!/usr/bin/env python3
"""
Enhanced core types and data models for the compiler helper module.

This module provides type-safe, performance-optimized data models using the latest
Python features including Pydantic v2, StrEnum, and comprehensive validation.
"""

from __future__ import annotations

import time
from enum import StrEnum, auto
from pathlib import Path
from typing import Any, Dict, List, Literal, Optional, Set, TypeAlias, Union
from dataclasses import dataclass, field

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator
from loguru import logger

# Type aliases for improved type hinting and performance
PathLike: TypeAlias = Union[str, Path]
CompilerName: TypeAlias = Literal["GCC", "Clang", "MSVC", "ICC", "MinGW", "Emscripten"]


class CppVersion(StrEnum):
    """
    C++ language standard versions using StrEnum for better serialization.

    Each version represents a published ISO C++ standard with its key features.
    """

    CPP98 = "c++98"  # First standardized version (1998)
    CPP03 = "c++03"  # Minor update with bug fixes (2003)
    CPP11 = "c++11"  # Major update: auto, lambda, move semantics (2011)
    CPP14 = "c++14"  # Generic lambdas, return type deduction (2014)
    CPP17 = "c++17"  # Structured bindings, if constexpr (2017)
    CPP20 = "c++20"  # Concepts, ranges, coroutines (2020)
    CPP23 = "c++23"  # Modules improvements, stacktrace (2023)
    CPP26 = "c++26"  # Upcoming standard (expected 2026)

    def __str__(self) -> str:
        """Return human-readable string representation."""
        descriptions = {
            self.CPP98: "C++98 (First Standard)",
            self.CPP03: "C++03 (Bug Fixes)",
            self.CPP11: "C++11 (Modern C++)",
            self.CPP14: "C++14 (Refinements)",
            self.CPP17: "C++17 (Structured Bindings)",
            self.CPP20: "C++20 (Concepts & Modules)",
            self.CPP23: "C++23 (Latest)",
            self.CPP26: "C++26 (Upcoming)",
        }
        return descriptions.get(self, self.value)

    @property
    def is_modern(self) -> bool:
        """Check if this is a modern C++ standard (C++11 or later)."""
        return self in {
            self.CPP11,
            self.CPP14,
            self.CPP17,
            self.CPP20,
            self.CPP23,
            self.CPP26,
        }

    @property
    def supports_modules(self) -> bool:
        """Check if this standard supports C++ modules."""
        return self in {self.CPP20, self.CPP23, self.CPP26}

    @property
    def supports_concepts(self) -> bool:
        """Check if this standard supports concepts."""
        return self in {self.CPP20, self.CPP23, self.CPP26}

    @classmethod
    def resolve_version(cls, version: Union[str, CppVersion]) -> CppVersion:
        """
        Resolve a version string or enum to a CppVersion with intelligent parsing.

        Args:
            version: Version string (e.g., "c++17", "cpp17", "17") or CppVersion enum

        Returns:
            Resolved CppVersion enum

        Raises:
            ValueError: If version cannot be resolved
        """
        if isinstance(version, CppVersion):
            return version

        # Normalize version string
        normalized = str(version).lower().strip()

        # Handle numeric versions (e.g., "17" -> "c++17")
        if normalized.isdigit():
            if len(normalized) == 2:
                normalized = f"c++{normalized}"
            elif len(normalized) == 4:  # e.g., "2017" -> "c++17"
                year_to_cpp = {
                    "1998": "98",
                    "2003": "03",
                    "2011": "11",
                    "2014": "14",
                    "2017": "17",
                    "2020": "20",
                    "2023": "23",
                    "2026": "26",
                }
                if normalized in year_to_cpp:
                    normalized = f"c++{year_to_cpp[normalized]}"
                else:
                    normalized = f"c++{normalized[-2:]}"

        # Handle variations like "cpp17", "C++17"
        if "cpp" in normalized and not normalized.startswith("c++"):
            normalized = normalized.replace("cpp", "c++")

        # Remove extra + signs
        if "++" in normalized and normalized.count("+") > 2:
            normalized = normalized.replace("+++", "++")

        # Ensure c++ prefix
        if not normalized.startswith("c++") and normalized.isdigit():
            normalized = f"c++{normalized}"

        try:
            return cls(normalized)
        except ValueError:
            valid_versions = ", ".join(v.value for v in cls)
            raise ValueError(
                f"Invalid C++ version: {version}. Valid versions: {valid_versions}"
            ) from None


class CompilerType(StrEnum):
    """Supported compiler types using StrEnum for better serialization."""

    GCC = "gcc"  # GNU Compiler Collection
    CLANG = "clang"  # LLVM Clang Compiler
    MSVC = "msvc"  # Microsoft Visual C++ Compiler
    ICC = "icc"  # Intel C++ Compiler
    MINGW = "mingw"  # MinGW (GCC for Windows)
    EMSCRIPTEN = "emscripten"  # Emscripten for WebAssembly

    def __str__(self) -> str:
        """Return human-readable string representation."""
        descriptions = {
            self.GCC: "GNU Compiler Collection",
            self.CLANG: "LLVM Clang",
            self.MSVC: "Microsoft Visual C++",
            self.ICC: "Intel C++ Compiler",
            self.MINGW: "MinGW-w64",
            self.EMSCRIPTEN: "Emscripten (WebAssembly)",
        }
        return descriptions.get(self, self.value)

    @property
    def is_gnu_compatible(self) -> bool:
        """Check if compiler is GNU-compatible (uses GCC-style flags)."""
        return self in {self.GCC, self.CLANG, self.MINGW, self.EMSCRIPTEN}

    @property
    def supports_sanitizers(self) -> bool:
        """Check if compiler supports runtime sanitizers."""
        return self in {self.GCC, self.CLANG}

    @property
    def default_executable(self) -> str:
        """Get the default executable name for this compiler type."""
        defaults = {
            self.GCC: "g++",
            self.CLANG: "clang++",
            self.MSVC: "cl.exe",
            self.ICC: "icpc",
            self.MINGW: "x86_64-w64-mingw32-g++",
            self.EMSCRIPTEN: "em++",
        }
        return defaults.get(self, "g++")


class OptimizationLevel(StrEnum):
    """Compiler optimization levels using StrEnum."""

    NONE = "O0"  # No optimization
    BASIC = "O1"  # Basic optimization
    STANDARD = "O2"  # Standard optimization
    AGGRESSIVE = "O3"  # Aggressive optimization
    SIZE = "Os"  # Optimize for size
    FAST = "Ofast"  # Optimize for speed (may break standards compliance)
    DEBUG = "Og"  # Optimize for debugging

    def __str__(self) -> str:
        descriptions = {
            self.NONE: "No Optimization (O0)",
            self.BASIC: "Basic Optimization (O1)",
            self.STANDARD: "Standard Optimization (O2)",
            self.AGGRESSIVE: "Aggressive Optimization (O3)",
            self.SIZE: "Size Optimization (Os)",
            self.FAST: "Fast Optimization (Ofast)",
            self.DEBUG: "Debug Optimization (Og)",
        }
        return descriptions.get(self, self.value)


class CompilerFeatures(BaseModel):
    """
    Enhanced compiler capabilities and features using Pydantic v2.
    """

    model_config = ConfigDict(
        extra="forbid", validate_assignment=True, use_enum_values=True
    )

    supports_parallel: bool = Field(
        default=False, description="Compiler supports parallel compilation"
    )
    supports_pch: bool = Field(
        default=False, description="Supports precompiled headers"
    )
    supports_modules: bool = Field(default=False, description="Supports C++20 modules")
    supports_coroutines: bool = Field(
        default=False, description="Supports C++20 coroutines"
    )
    supports_concepts: bool = Field(
        default=False, description="Supports C++20 concepts"
    )

    supported_cpp_versions: Set[CppVersion] = Field(
        default_factory=set, description="Set of supported C++ standards"
    )
    supported_sanitizers: Set[str] = Field(
        default_factory=set, description="Set of supported runtime sanitizers"
    )
    supported_optimizations: Set[OptimizationLevel] = Field(
        default_factory=set, description="Set of supported optimization levels"
    )
    feature_flags: Dict[str, str] = Field(
        default_factory=dict, description="Compiler-specific feature flags"
    )
    max_parallel_jobs: int = Field(
        default=1, ge=1, description="Maximum parallel compilation jobs"
    )


class CompileOptions(BaseModel):
    """Enhanced compiler options with comprehensive validation using Pydantic v2."""

    model_config = ConfigDict(
        extra="forbid", validate_assignment=True, str_strip_whitespace=True
    )

    include_paths: List[PathLike] = Field(
        default_factory=list, description="Directories to search for include files"
    )
    defines: Dict[str, Optional[str]] = Field(
        default_factory=dict, description="Preprocessor definitions"
    )
    warnings: List[str] = Field(
        default_factory=list, description="Warning flags to enable"
    )
    optimization: OptimizationLevel = Field(
        default=OptimizationLevel.STANDARD, description="Optimization level"
    )
    debug: bool = Field(
        default=False, description="Enable debug information generation"
    )
    position_independent: bool = Field(
        default=False, description="Generate position-independent code"
    )
    standard_library: Optional[str] = Field(
        default=None, description="Standard library implementation (libc++, libstdc++)"
    )
    sanitizers: List[str] = Field(
        default_factory=list, description="Runtime sanitizers to enable"
    )
    extra_flags: List[str] = Field(
        default_factory=list, description="Additional compiler flags"
    )
    parallel_jobs: int = Field(
        default=1, ge=1, le=64, description="Number of parallel compilation jobs"
    )

    @field_validator("sanitizers")
    @classmethod
    def validate_sanitizers(cls, v: List[str]) -> List[str]:
        """Validate sanitizer names."""
        valid_sanitizers = {
            "address",
            "thread",
            "memory",
            "undefined",
            "leak",
            "dataflow",
            "cfi",
            "safe-stack",
            "bounds",
        }

        for sanitizer in v:
            if sanitizer not in valid_sanitizers:
                logger.warning(
                    f"Unknown sanitizer '{sanitizer}', valid options: {valid_sanitizers}"
                )

        return v


class LinkOptions(BaseModel):
    """Enhanced linker options with comprehensive validation using Pydantic v2."""

    model_config = ConfigDict(
        extra="forbid", validate_assignment=True, str_strip_whitespace=True
    )

    library_paths: List[PathLike] = Field(
        default_factory=list, description="Directories to search for libraries"
    )
    libraries: List[str] = Field(
        default_factory=list, description="Libraries to link against"
    )
    runtime_library_paths: List[PathLike] = Field(
        default_factory=list, description="Runtime library search paths (rpath)"
    )
    shared: bool = Field(default=False, description="Create shared library (.so/.dll)")
    static: bool = Field(default=False, description="Prefer static linking")
    strip_symbols: bool = Field(
        default=False, description="Strip debug symbols from output"
    )
    generate_map: bool = Field(default=False, description="Generate linker map file")
    map_file: Optional[PathLike] = Field(
        default=None, description="Custom map file path"
    )
    extra_flags: List[str] = Field(
        default_factory=list, description="Additional linker flags"
    )

    @model_validator(mode="after")
    def validate_link_options(self) -> LinkOptions:
        """Validate linker option combinations."""
        if self.shared and self.static:
            raise ValueError("Cannot specify both shared and static linking")

        if self.generate_map and not self.map_file:
            # Auto-generate map file name
            self.map_file = "output.map"

        return self


@dataclass(frozen=True, slots=True)
class CommandResult:
    """
    Immutable result of a command execution with enhanced error context.

    Uses slots for memory efficiency and frozen=True for immutability.
    """

    success: bool
    stdout: str = ""
    stderr: str = ""
    return_code: int = 0
    command: List[str] = field(default_factory=list)
    execution_time: float = 0.0
    timestamp: float = field(default_factory=time.time)

    def __post_init__(self) -> None:
        """Validate command result data."""
        if self.execution_time < 0:
            raise ValueError("execution_time cannot be negative")

    @property
    def output(self) -> str:
        """Get combined output (stdout + stderr)."""
        return f"{self.stdout}\n{self.stderr}".strip()

    @property
    def failed(self) -> bool:
        """Check if the command failed."""
        return not self.success

    @property
    def command_str(self) -> str:
        """Get command as a single string."""
        return " ".join(self.command)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "success": self.success,
            "stdout": self.stdout,
            "stderr": self.stderr,
            "return_code": self.return_code,
            "command": self.command,
            "execution_time": self.execution_time,
            "timestamp": self.timestamp,
        }


class CompilationResult(BaseModel):
    """
    Enhanced compilation result with comprehensive tracking using Pydantic v2.
    """

    model_config = ConfigDict(extra="forbid", validate_assignment=True)

    success: bool = Field(description="Whether compilation succeeded")
    output_file: Optional[Path] = Field(
        default=None, description="Path to generated output file"
    )
    duration_ms: float = Field(
        default=0.0, ge=0.0, description="Compilation duration in milliseconds"
    )
    command_line: Optional[List[str]] = Field(
        default=None, description="Full command line used for compilation"
    )
    errors: List[str] = Field(default_factory=list, description="Compilation errors")
    warnings: List[str] = Field(
        default_factory=list, description="Compilation warnings"
    )
    notes: List[str] = Field(
        default_factory=list, description="Additional notes and information"
    )
    artifacts: List[Path] = Field(
        default_factory=list,
        description="Additional files generated during compilation",
    )

    @property
    def has_errors(self) -> bool:
        """Check if compilation has errors."""
        return len(self.errors) > 0

    @property
    def has_warnings(self) -> bool:
        """Check if compilation has warnings."""
        return len(self.warnings) > 0

    @property
    def duration_seconds(self) -> float:
        """Get duration in seconds."""
        return self.duration_ms / 1000.0

    def add_error(self, error: str) -> None:
        """Add an error message."""
        self.errors.append(error)
        if self.success:
            self.success = False

    def add_warning(self, warning: str) -> None:
        """Add a warning message."""
        self.warnings.append(warning)

    def add_note(self, note: str) -> None:
        """Add an informational note."""
        self.notes.append(note)


# Custom exceptions with enhanced error context
class CompilerException(Exception):
    """Base exception for compiler-related errors."""

    def __init__(
        self, message: str, *, error_code: Optional[str] = None, **kwargs: Any
    ):
        super().__init__(message)
        self.error_code = error_code
        self.context = kwargs

        # Log the exception with context
        logger.error(
            f"CompilerException: {message}",
            extra={"error_code": error_code, "context": kwargs},
        )


class CompilationError(CompilerException):
    """Exception raised when compilation fails."""

    def __init__(
        self,
        message: str,
        command: Optional[List[str]] = None,
        return_code: Optional[int] = None,
        stderr: Optional[str] = None,
        **kwargs: Any,
    ):
        super().__init__(
            message, command=command, return_code=return_code, stderr=stderr, **kwargs
        )
        self.command = command
        self.return_code = return_code
        self.stderr = stderr


class CompilerNotFoundError(CompilerException):
    """Exception raised when a requested compiler is not available."""

    pass


class InvalidConfigurationError(CompilerException):
    """Exception raised when configuration is invalid."""

    pass


class BuildError(CompilerException):
    """Exception raised when build process fails."""

    pass
