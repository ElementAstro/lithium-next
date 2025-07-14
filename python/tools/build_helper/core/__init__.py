#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Core components for build system helpers with modern Python features.

This module provides the foundational classes and utilities for the build system helper,
including base classes, data models, error handling, and session management.
"""

from __future__ import annotations

from .base import BuildHelperBase
from .models import (
    BuildStatus, BuildResult, BuildOptions, 
    BuildMetrics, BuildSession, BuildOptionsProtocol
)
from .errors import (
    BuildSystemError, ConfigurationError, BuildError,
    TestError, InstallationError, DependencyError,
    ErrorContext, handle_build_error
)

__all__ = [
    # Base classes
    "BuildHelperBase",
    
    # Data models
    "BuildStatus", 
    "BuildResult", 
    "BuildOptions",
    "BuildOptionsProtocol",
    "BuildMetrics",
    "BuildSession",
    
    # Error handling
    "BuildSystemError", 
    "ConfigurationError", 
    "BuildError",
    "TestError", 
    "InstallationError",
    "DependencyError",
    "ErrorContext",
    "handle_build_error",
]