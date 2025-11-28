#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Advanced Build System Helper

A versatile build system utility supporting CMake, Meson, and Bazel with both
command-line and pybind11 embedding capabilities.
"""

from .utils.config import BuildConfig
from .utils.factory import BuilderFactory
from .builders.bazel import BazelBuilder
from .builders.meson import MesonBuilder
from .builders.cmake import CMakeBuilder
from .core.errors import (
    BuildSystemError, ConfigurationError, BuildError,
    TestError, InstallationError
)
from .core.models import BuildStatus, BuildResult, BuildOptions
from .core.base import BuildHelperBase
import sys
from loguru import logger

# Configure loguru with defaults
logger.remove()  # Remove default handler
logger.add(
    sys.stderr,
    format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
    colorize=True
)

# Package metadata
__version__ = "2.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"


def get_tool_info() -> dict:
    """
    Get comprehensive metadata about the build_helper module.

    Returns:
        dict: Module metadata including name, version, description, author,
              license, supported platforms, available functions, requirements,
              capabilities, and classes.
    """
    return {
        "name": "build_helper",
        "version": __version__,
        "description": "Advanced build system helper supporting CMake, Meson, and Bazel with command-line and pybind11 embedding capabilities",
        "author": __author__,
        "license": __license__,
        "supported": True,
        "platform": ["windows", "linux", "macos"],
        "functions": [
            "configure",
            "build",
            "install",
            "test",
            "generate_docs",
            "clean",
            "get_status",
            "get_last_result",
            "get_tool_info"
        ],
        "requirements": [
            "cmake>=3.20",
            "meson",
            "bazel",
            "python>=3.7",
            "loguru",
            "pybind11"
        ],
        "capabilities": [
            "multi-build-system-support",
            "parallel-building",
            "async-execution",
            "caching",
            "environment-variables",
            "custom-generators",
            "documentation-generation",
            "testing-integration"
        ],
        "classes": {
            "BuildHelperBase": "Abstract base class for build helpers",
            "CMakeBuilder": "CMake-specific build system implementation",
            "MesonBuilder": "Meson-specific build system implementation",
            "BazelBuilder": "Bazel-specific build system implementation",
            "BuilderFactory": "Factory for creating appropriate builder instances",
            "BuildConfig": "Configuration management for build systems",
            "BuildResult": "Data class for storing build operation results",
            "BuildStatus": "Enumeration of build status values",
            "BuildOptions": "Type definition for build options dictionary"
        }
    }


# Import core components for easy access

# Import builders

# Import utilities

__all__ = [
    'BuildHelperBase', 'BuildStatus', 'BuildResult', 'BuildOptions',
    'BuildSystemError', 'ConfigurationError', 'BuildError',
    'TestError', 'InstallationError',
    'CMakeBuilder', 'MesonBuilder', 'BazelBuilder',
    'BuilderFactory', 'BuildConfig',
    'get_tool_info',
    '__version__', '__author__', '__license__'
]
