#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Advanced Build System Helper with Modern Python Features

A versatile, high-performance build system utility supporting CMake, Meson, and Bazel
with enhanced error handling, async operations, and comprehensive logging capabilities.

Features:
- Auto-detection of build systems
- Robust error handling with detailed context
- Asynchronous operations for better performance
- Structured logging with loguru
- Configuration file support (JSON, YAML, TOML, INI)
- Performance monitoring and metrics
- Type safety with modern Python features
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import List, Optional

from loguru import logger

# Core components
from .core.base import BuildHelperBase
from .core.models import (
    BuildStatus,
    BuildResult,
    BuildOptions,
    BuildMetrics,
    BuildSession,
)
from .core.errors import (
    BuildSystemError,
    ConfigurationError,
    BuildError,
    TestError,
    InstallationError,
    DependencyError,
    ErrorContext,
    handle_build_error,
)

# Builders
from .builders.cmake import CMakeBuilder
from .builders.meson import MesonBuilder
from .builders.bazel import BazelBuilder

# Utilities
from .utils.config import BuildConfig
from .utils.factory import BuilderFactory

# Package metadata
__version__ = "2.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"
__description__ = "Advanced Build System Helper with Modern Python Features"


def configure_default_logging(level: str = "INFO", enable_colors: bool = True) -> None:
    """
    Configure default logging for the build_helper package.

    Args:
        level: Log level (DEBUG, INFO, WARNING, ERROR, CRITICAL)
        enable_colors: Whether to enable colored output
    """
    logger.remove()  # Remove default handler

    log_format = (
        "<green>{time:HH:mm:ss}</green> | "
        "<level>{level: <8}</level> | "
        "<level>{message}</level>"
    )

    if level in ["DEBUG", "TRACE"]:
        log_format = (
            "<green>{time:YYYY-MM-DD HH:mm:ss.SSS}</green> | "
            "<level>{level: <8}</level> | "
            "<cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> | "
            "<level>{message}</level>"
        )

    logger.add(
        sys.stderr,
        level=level,
        format=log_format,
        colorize=enable_colors,
        enqueue=True,  # Thread-safe logging
    )


def get_version_info() -> dict[str, str]:
    """Get detailed version information."""
    return {
        "version": __version__,
        "author": __author__,
        "license": __license__,
        "description": __description__,
        "python_version": f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}",
        "supported_builders": BuilderFactory.get_available_builders(),
    }


def auto_build(
    source_dir: Optional[Path] = None,
    build_dir: Optional[Path] = None,
    *,
    clean: bool = False,
    test: bool = False,
    install: bool = False,
    verbose: bool = False,
) -> bool:
    """
    Convenience function for auto-detected build operations.

    Args:
        source_dir: Source directory (defaults to current directory)
        build_dir: Build directory (defaults to 'build')
        clean: Whether to clean before building
        test: Whether to run tests after building
        install: Whether to install after building
        verbose: Enable verbose output

    Returns:
        True if build was successful, False otherwise
    """
    import asyncio

    source_path = Path(source_dir or ".")
    build_path = Path(build_dir or "build")

    try:
        # Auto-detect build system
        builder_type = BuilderFactory.detect_build_system(source_path)
        if not builder_type:
            logger.error(f"No supported build system detected in {source_path}")
            return False

        # Create builder
        builder = BuilderFactory.create_builder(
            builder_type=builder_type,
            source_dir=source_path,
            build_dir=build_path,
            verbose=verbose,
        )

        # Execute build workflow
        async def run_build():
            return await builder.full_build_workflow(
                clean_first=clean, run_tests=test, install_after_build=install
            )

        results = asyncio.run(run_build())
        return all(result.success for result in results)

    except Exception as e:
        logger.error(f"Auto-build failed: {e}")
        return False


# Configure default logging on import
configure_default_logging()

# Public API
__all__ = [
    # Core classes
    "BuildHelperBase",
    "BuildStatus",
    "BuildResult",
    "BuildOptions",
    "BuildMetrics",
    "BuildSession",
    # Error classes
    "BuildSystemError",
    "ConfigurationError",
    "BuildError",
    "TestError",
    "InstallationError",
    "DependencyError",
    "ErrorContext",
    "handle_build_error",
    # Builder classes
    "CMakeBuilder",
    "MesonBuilder",
    "BazelBuilder",
    # Utility classes
    "BuilderFactory",
    "BuildConfig",
    # Convenience functions
    "auto_build",
    "configure_default_logging",
    "get_version_info",
    # Package metadata
    "__version__",
    "__author__",
    "__license__",
    "__description__",
]
