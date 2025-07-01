#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Core types and exceptions for the compiler helper module.
"""
from enum import Enum, auto
from typing import List, Dict, Optional, Union, Set, Any, TypedDict, Literal
from dataclasses import dataclass, field
from pathlib import Path

# Type definitions for improved type hinting
PathLike = Union[str, Path]
CompilerName = Literal["GCC", "Clang", "MSVC"]
CommandResult = tuple[int, str, str]  # return_code, stdout, stderr


class CppVersion(Enum):
    """
    Enum representing supported C++ language standard versions.
    """
    CPP98 = "c++98"   # Published in 1998, first standardized version
    CPP03 = "c++03"   # Published in 2003, minor update to 98
    # Major update published in 2011 (auto, lambda, move semantics)
    CPP11 = "c++11"
    # Published in 2014 (generic lambdas, return type deduction)
    CPP14 = "c++14"
    CPP17 = "c++17"   # Published in 2017 (structured bindings, if constexpr)
    CPP20 = "c++20"   # Published in 2020 (concepts, ranges, coroutines)
    CPP23 = "c++23"   # Latest standard (modules improvements, stacktrace)

    @staticmethod
    def resolve_cpp_version(version: Union[str, 'CppVersion']) -> 'CppVersion':
        if isinstance(version, CppVersion):
            return version
        try:
            return CppVersion(version)
        except ValueError:
            # Handle common variations like "c++17", "cpp17", "C++17"
            normalized_version = version.lower().replace("c++", "cpp").upper()
            if not normalized_version.startswith("CPP"):
                normalized_version = "CPP" + normalized_version
            return CppVersion[normalized_version]



class CompilerType(Enum):
    """Enum representing supported compiler types."""
    GCC = auto()      # GNU Compiler Collection
    CLANG = auto()    # LLVM Clang Compiler
    MSVC = auto()     # Microsoft Visual C++ Compiler
    ICC = auto()      # Intel C++ Compiler
    MINGW = auto()    # MinGW (GCC for Windows)
    EMSCRIPTEN = auto()  # Emscripten for WebAssembly


class CompileOptions(TypedDict, total=False):
    """TypedDict for compiler options with optional fields."""
    include_paths: List[PathLike]      # Directories to search for include files
    defines: Dict[str, Optional[str]]  # Preprocessor definitions
    warnings: List[str]                # Warning flags
    optimization: str                  # Optimization level
    debug: bool                        # Enable debug information
    position_independent: bool         # Generate position-independent code
    # Specify standard library implementation
    standard_library: Optional[str]
    # Enable sanitizers (e.g., address, undefined)
    sanitizers: List[str]
    extra_flags: List[str]             # Additional compiler flags


class LinkOptions(TypedDict, total=False):
    """TypedDict for linker options with optional fields."""
    library_paths: List[PathLike]      # Directories to search for libraries
    libraries: List[str]               # Libraries to link against
    runtime_library_paths: List[PathLike]  # Runtime library search paths
    shared: bool                       # Create shared library
    static: bool                       # Prefer static linking
    strip: bool                        # Strip debug symbols
    map_file: Optional[PathLike]       # Generate map file
    extra_flags: List[str]             # Additional linker flags


class CompilationError(Exception):
    """Exception raised when compilation fails."""

    def __init__(self, message: str, command: List[str], return_code: int, stderr: str):
        self.message = message
        self.command = command
        self.return_code = return_code
        self.stderr = stderr
        super().__init__(
            f"{message} (Return code: {return_code})\nCommand: {' '.join(command)}\nError: {stderr}")


class CompilerNotFoundError(Exception):
    """Exception raised when a requested compiler is not available."""
    pass


@dataclass
class CompilationResult:
    """Represents the result of a compilation operation."""
    success: bool
    output_file: Optional[Path] = None
    duration_ms: float = 0.0
    command_line: Optional[List[str]] = None
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)


@dataclass
class CompilerFeatures:
    """Represents capabilities and features of a specific compiler."""
    supports_parallel: bool = False
    supports_pch: bool = False  # Precompiled headers
    supports_modules: bool = False
    supported_cpp_versions: Set[CppVersion] = field(default_factory=set)
    supported_sanitizers: Set[str] = field(default_factory=set)
    supported_optimizations: Set[str] = field(default_factory=set)
    feature_flags: Dict[str, str] = field(default_factory=dict)
