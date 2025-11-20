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
from cli import main
import sys
from loguru import logger

# Configure default logging
logger.remove()  # Remove default handler
logger.add(
    sys.stderr,
    format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
    level="INFO"
)

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

    # API functions
    'get_compiler',
    'compile_file',
    'build_project',
    'load_json',
    'save_json',

    # Instances
    'compiler_manager',

    'main'
]

__version__ = '0.1.0'
