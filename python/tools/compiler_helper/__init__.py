#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Compiler Helper Module - Enhanced Version

This module provides utilities for C++ compiler management, source compilation,
and project building with support for both command-line and programmatic usage.

Features:
- Cross-platform compiler detection (GCC, Clang, MSVC)
- Multiple C++ standard support (C++98 through C++23)
- Advanced compilation and linking capabilities
- Configuration via JSON, YAML, or command-line
- Dependency tracking and incremental builds
- Extensive error handling and logging
- Parallel compilation support
"""

from .api import (
    get_compiler,
    compile_file,
    build_project,
    compiler_manager,  # singleton instance
)
from .utils import load_json, save_json
from .build_manager import BuildManager
from .compiler_manager import CompilerManager
from .compiler import Compiler
from .core_types import (
    CppVersion,
    CompilerType,
    CompilationResult,
    CompileOptions,
    LinkOptions,
    CompilationError,
    CompilerNotFoundError,
)
from .pybind_adapter import CompilerHelperPyBindAdapter
from cli import main
import sys
from loguru import logger

# Module metadata
__version__ = '0.1.0'
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"

# Configure default logging
logger.remove()  # Remove default handler
logger.add(
    sys.stderr,
    format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
    level="INFO"
)


def get_tool_info() -> dict:
    """
    Return metadata about this tool for discovery by PythonWrapper.

    This function follows the Lithium-Next Python tool discovery convention,
    allowing the C++ PythonWrapper to introspect and catalog this module's
    capabilities.

    Returns:
        Dict containing tool metadata including name, version, description,
        available functions, requirements, and platform compatibility.
    """
    return {
        "name": "compiler_helper",
        "version": __version__,
        "description": "Cross-platform C++ compiler management and compilation utilities with support for GCC, Clang, and MSVC",
        "author": __author__,
        "license": __license__,
        "supported": True,
        "platform": ["windows", "linux", "macos"],
        "functions": [
            # High-level API functions
            "get_compiler",
            "compile_file",
            "build_project",
            # Compiler detection and info
            "detect_compilers",
            "get_compiler_info",
            # Compilation operations
            "compile",
            "link",
            # Build management
            "build",
            "clean",
            # Utility functions
            "load_json",
            "save_json",
        ],
        "requirements": ["loguru"],
        "capabilities": [
            "cross_platform_compilation",
            "multi_compiler_support",
            "incremental_build",
            "dependency_tracking",
            "parallel_compilation",
            "sanitizer_support",
            "debug_symbol_generation",
            "optimization_control",
            "preprocessor_definitions",
            "include_path_management",
            "library_linking",
        ],
        "classes": {
            "Compiler": "Individual compiler abstraction with compilation and linking capabilities",
            "CompilerManager": "Compiler detection and management on the system",
            "BuildManager": "High-level project building and incremental compilation",
            "CompilerHelperPyBindAdapter": "Simplified pybind11 interface for C++ integration",
        }
    }


# Export public API


__all__ = [
    # Core types
    'CppVersion',
    'CompilerType',
    'CompilationResult',
    'CompileOptions',
    'LinkOptions',
    'CompilationError',
    'CompilerNotFoundError',

    # Classes
    'Compiler',
    'CompilerManager',
    'BuildManager',
    'CompilerHelperPyBindAdapter',

    # API functions
    'get_compiler',
    'compile_file',
    'build_project',
    'load_json',
    'save_json',
    'get_tool_info',

    # Instances
    'compiler_manager',

    'main'
]
