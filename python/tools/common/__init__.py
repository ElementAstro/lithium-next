#!/usr/bin/env python3
"""
Common utilities for Python tools in Lithium-Next.

This module provides base classes and discovery mechanisms for pybind11 adapters,
enabling standardized tool registration and management.
"""

from .base_adapter import (
    BasePybindAdapter,
    ToolInfo,
    FunctionInfo,
    ParameterInfo,
    AdapterResponse,
    pybind_method,
    async_pybind_method,
)

from .discovery import (
    ToolDiscovery,
    ToolRegistry,
    discover_tools,
    get_tool_registry,
)

__version__ = "1.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"

__all__ = [
    # Base adapter
    "BasePybindAdapter",
    "ToolInfo",
    "FunctionInfo",
    "ParameterInfo",
    "AdapterResponse",
    "pybind_method",
    "async_pybind_method",
    # Discovery
    "ToolDiscovery",
    "ToolRegistry",
    "discover_tools",
    "get_tool_registry",
]
