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

# Package version
__version__ = "2.0.0"

# Import core components for easy access

# Import builders

# Import utilities

__all__ = [
    'BuildHelperBase', 'BuildStatus', 'BuildResult', 'BuildOptions',
    'BuildSystemError', 'ConfigurationError', 'BuildError',
    'TestError', 'InstallationError',
    'CMakeBuilder', 'MesonBuilder', 'BazelBuilder',
    'BuilderFactory', 'BuildConfig',
    '__version__'
]