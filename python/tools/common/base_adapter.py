#!/usr/bin/env python3
"""
Base Pybind11 Adapter for Lithium-Next Python Tools.

This module provides abstract base classes and decorators for creating
standardized pybind11 adapters that can be automatically discovered
and registered with the C++ tool registry.

Features:
- Abstract base class with common interface
- Decorator-based error handling
- Standardized response format
- Type introspection for C++ binding generation
- Async method support
"""

import asyncio
import functools
import inspect
import sys
import traceback
from abc import ABC, abstractmethod
from dataclasses import dataclass, field, asdict
from enum import Enum, auto
from typing import (
    Any,
    Callable,
    Dict,
    List,
    Optional,
    Type,
    TypeVar,
    Union,
    get_type_hints,
    Tuple,
)

from loguru import logger


class ParameterType(Enum):
    """Supported parameter types for C++ binding."""

    STRING = auto()
    INTEGER = auto()
    FLOAT = auto()
    BOOLEAN = auto()
    LIST = auto()
    DICT = auto()
    BYTES = auto()
    PATH = auto()
    OPTIONAL = auto()
    ANY = auto()


@dataclass
class ParameterInfo:
    """Describes a function parameter for C++ binding."""

    name: str
    param_type: ParameterType
    description: str = ""
    required: bool = True
    default_value: Any = None
    element_type: Optional[ParameterType] = None  # For LIST/DICT types

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        result = {
            "name": self.name,
            "type": self.param_type.name.lower(),
            "description": self.description,
            "required": self.required,
        }
        if self.default_value is not None:
            result["default"] = self.default_value
        if self.element_type is not None:
            result["element_type"] = self.element_type.name.lower()
        return result


@dataclass
class FunctionInfo:
    """Describes an exported function for C++ binding."""

    name: str
    description: str
    parameters: List[ParameterInfo] = field(default_factory=list)
    return_type: str = "dict"
    is_async: bool = False
    is_static: bool = True
    category: str = ""
    tags: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "name": self.name,
            "description": self.description,
            "parameters": [p.to_dict() for p in self.parameters],
            "return_type": self.return_type,
            "is_async": self.is_async,
            "is_static": self.is_static,
            "category": self.category,
            "tags": self.tags,
        }


@dataclass
class ToolInfo:
    """Comprehensive metadata about a Python tool."""

    name: str
    version: str
    description: str
    author: str = "Max Qian"
    license: str = "GPL-3.0-or-later"
    supported: bool = True
    platforms: List[str] = field(default_factory=lambda: ["linux", "windows", "darwin"])
    functions: List[FunctionInfo] = field(default_factory=list)
    requirements: List[str] = field(default_factory=list)
    capabilities: List[str] = field(default_factory=list)
    categories: List[str] = field(default_factory=list)
    module_path: str = ""

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "author": self.author,
            "license": self.license,
            "supported": self.supported,
            "platforms": self.platforms,
            "functions": [f.to_dict() for f in self.functions],
            "requirements": self.requirements,
            "capabilities": self.capabilities,
            "categories": self.categories,
            "module_path": self.module_path,
        }


@dataclass
class AdapterResponse:
    """Standardized response format for pybind11 methods."""

    success: bool
    data: Any = None
    error: Optional[str] = None
    error_type: Optional[str] = None
    traceback_info: Optional[str] = None
    metadata: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for pybind11 serialization."""
        result = {"success": self.success}
        if self.data is not None:
            result["data"] = self.data
        if self.error is not None:
            result["error"] = self.error
        if self.error_type is not None:
            result["error_type"] = self.error_type
        if self.traceback_info is not None:
            result["traceback"] = self.traceback_info
        if self.metadata:
            result["metadata"] = self.metadata
        return result

    @classmethod
    def success_response(cls, data: Any = None, **metadata) -> "AdapterResponse":
        """Create a success response."""
        return cls(success=True, data=data, metadata=metadata)

    @classmethod
    def error_response(
        cls, error: str, error_type: str = None, include_traceback: bool = False
    ) -> "AdapterResponse":
        """Create an error response."""
        tb = None
        if include_traceback:
            tb = traceback.format_exc()
        return cls(
            success=False,
            error=error,
            error_type=error_type or "Error",
            traceback_info=tb,
        )


def _python_type_to_param_type(
    py_type: Any,
) -> Tuple[ParameterType, Optional[ParameterType]]:
    """Convert Python type hint to ParameterType."""
    origin = getattr(py_type, "__origin__", None)

    # Handle Optional[X] = Union[X, None]
    if origin is Union:
        args = py_type.__args__
        # Check if it's Optional (Union with None)
        non_none_args = [a for a in args if a is not type(None)]
        if len(non_none_args) == 1:
            inner_type, element = _python_type_to_param_type(non_none_args[0])
            return inner_type, element

    # Handle List[X]
    if origin is list:
        args = getattr(py_type, "__args__", ())
        element_type = None
        if args:
            element_type, _ = _python_type_to_param_type(args[0])
        return ParameterType.LIST, element_type

    # Handle Dict[K, V]
    if origin is dict:
        return ParameterType.DICT, None

    # Handle basic types
    type_mapping = {
        str: ParameterType.STRING,
        int: ParameterType.INTEGER,
        float: ParameterType.FLOAT,
        bool: ParameterType.BOOLEAN,
        bytes: ParameterType.BYTES,
        list: ParameterType.LIST,
        dict: ParameterType.DICT,
    }

    if py_type in type_mapping:
        return type_mapping[py_type], None

    # Check for pathlib.Path
    if hasattr(py_type, "__mro__"):
        for base in py_type.__mro__:
            if base.__name__ in ("Path", "PurePath"):
                return ParameterType.PATH, None

    return ParameterType.ANY, None


def pybind_method(
    description: str = "",
    category: str = "",
    tags: List[str] = None,
):
    """
    Decorator for marking methods as pybind11-exportable.

    This decorator:
    - Wraps the method with standardized error handling
    - Returns AdapterResponse format on success/failure
    - Extracts parameter info from type hints
    - Logs method calls for debugging

    Args:
        description: Human-readable description of the method
        category: Category for grouping (e.g., "file_operations")
        tags: Tags for filtering (e.g., ["read", "sync"])

    Example:
        @pybind_method(description="Clone a git repository")
        def clone(url: str, path: str) -> Dict[str, Any]:
            ...
    """
    tags = tags or []

    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapper(*args, **kwargs) -> Dict[str, Any]:
            method_name = func.__name__
            logger.debug(f"pybind call: {method_name}({args}, {kwargs})")

            try:
                result = func(*args, **kwargs)

                # If result is already an AdapterResponse or dict, use it
                if isinstance(result, AdapterResponse):
                    return result.to_dict()
                if isinstance(result, dict):
                    if "success" in result:
                        return result
                    return AdapterResponse.success_response(result).to_dict()

                # Otherwise wrap the result
                return AdapterResponse.success_response(result).to_dict()

            except Exception as e:
                logger.exception(f"Error in pybind method {method_name}: {e}")
                return AdapterResponse.error_response(
                    error=str(e),
                    error_type=type(e).__name__,
                    include_traceback=True,
                ).to_dict()

        # Store metadata on the function
        wrapper._pybind_info = {
            "description": description or func.__doc__ or "",
            "category": category,
            "tags": tags,
            "is_async": False,
            "original_func": func,
        }

        return wrapper

    return decorator


def async_pybind_method(
    description: str = "",
    category: str = "",
    tags: List[str] = None,
):
    """
    Decorator for marking async methods as pybind11-exportable.

    Similar to pybind_method but for async functions. Creates both
    async and sync versions of the method.

    Args:
        description: Human-readable description of the method
        category: Category for grouping
        tags: Tags for filtering

    Example:
        @async_pybind_method(description="Start hotspot asynchronously")
        async def start_async(name: str) -> Dict[str, Any]:
            ...
    """
    tags = tags or []

    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        async def async_wrapper(*args, **kwargs) -> Dict[str, Any]:
            method_name = func.__name__
            logger.debug(f"async pybind call: {method_name}({args}, {kwargs})")

            try:
                result = await func(*args, **kwargs)

                if isinstance(result, AdapterResponse):
                    return result.to_dict()
                if isinstance(result, dict):
                    if "success" in result:
                        return result
                    return AdapterResponse.success_response(result).to_dict()

                return AdapterResponse.success_response(result).to_dict()

            except Exception as e:
                logger.exception(f"Error in async pybind method {method_name}: {e}")
                return AdapterResponse.error_response(
                    error=str(e),
                    error_type=type(e).__name__,
                    include_traceback=True,
                ).to_dict()

        # Create sync wrapper that runs the async function
        @functools.wraps(func)
        def sync_wrapper(*args, **kwargs) -> Dict[str, Any]:
            try:
                loop = asyncio.get_event_loop()
            except RuntimeError:
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)

            return loop.run_until_complete(async_wrapper(*args, **kwargs))

        # Store metadata
        async_wrapper._pybind_info = {
            "description": description or func.__doc__ or "",
            "category": category,
            "tags": tags,
            "is_async": True,
            "original_func": func,
            "sync_wrapper": sync_wrapper,
        }

        sync_wrapper._pybind_info = async_wrapper._pybind_info

        return async_wrapper

    return decorator


class BasePybindAdapter(ABC):
    """
    Abstract base class for pybind11 adapters.

    All tool adapters should inherit from this class and implement
    the required abstract methods. The base class provides:

    - Standardized error handling
    - Platform compatibility checking
    - Automatic function discovery for C++ binding
    - Metadata extraction for tool registration

    Example:
        class GitUtilsAdapter(BasePybindAdapter):
            @classmethod
            def get_tool_info(cls) -> ToolInfo:
                return ToolInfo(
                    name="git_utils",
                    version="1.0.0",
                    description="Git utilities for repository management"
                )

            @staticmethod
            @pybind_method(description="Clone a repository")
            def clone(url: str, path: str) -> Dict[str, Any]:
                ...
    """

    # Override in subclass for platform restrictions
    SUPPORTED_PLATFORMS: List[str] = ["linux", "windows", "darwin"]

    @classmethod
    @abstractmethod
    def get_tool_info(cls) -> ToolInfo:
        """
        Get comprehensive metadata about the tool.

        Must be implemented by all subclasses.

        Returns:
            ToolInfo containing name, version, description, and capabilities
        """
        pass

    @classmethod
    def is_platform_supported(cls) -> bool:
        """
        Check if the current platform is supported.

        Returns:
            True if the current platform is in SUPPORTED_PLATFORMS
        """
        current_platform = sys.platform
        for platform in cls.SUPPORTED_PLATFORMS:
            if current_platform.startswith(platform):
                return True
        return False

    @classmethod
    def check_platform(cls) -> Dict[str, Any]:
        """
        Check platform support and return structured response.

        Returns:
            Dict with success flag and platform info
        """
        supported = cls.is_platform_supported()
        if not supported:
            return {
                "success": False,
                "supported": False,
                "error": f"This tool only supports: {', '.join(cls.SUPPORTED_PLATFORMS)}",
                "current_platform": sys.platform,
            }
        return {"success": True, "supported": True, "current_platform": sys.platform}

    @classmethod
    def get_exported_functions(cls) -> Dict[str, FunctionInfo]:
        """
        Discover all pybind-exportable methods on this adapter.

        Scans the class for methods decorated with @pybind_method or
        @async_pybind_method and extracts their metadata.

        Returns:
            Dict mapping function names to FunctionInfo objects
        """
        exported = {}

        for name in dir(cls):
            if name.startswith("_"):
                continue

            method = getattr(cls, name)
            if not callable(method):
                continue

            # Check for pybind decorator metadata
            pybind_info = getattr(method, "_pybind_info", None)
            if pybind_info is None:
                continue

            # Extract parameter info from type hints
            original_func = pybind_info.get("original_func", method)
            params = cls._extract_parameters(original_func)

            func_info = FunctionInfo(
                name=name,
                description=pybind_info.get("description", ""),
                parameters=params,
                is_async=pybind_info.get("is_async", False),
                category=pybind_info.get("category", ""),
                tags=pybind_info.get("tags", []),
            )
            exported[name] = func_info

        return exported

    @classmethod
    def _extract_parameters(cls, func: Callable) -> List[ParameterInfo]:
        """Extract parameter information from a function's signature and type hints."""
        params = []
        sig = inspect.signature(func)
        hints = get_type_hints(func) if hasattr(func, "__annotations__") else {}

        for param_name, param in sig.parameters.items():
            if param_name in ("self", "cls"):
                continue

            # Get type hint
            py_type = hints.get(param_name, Any)
            param_type, element_type = _python_type_to_param_type(py_type)

            # Determine if required
            required = param.default is inspect.Parameter.empty
            default_value = None if required else param.default

            params.append(
                ParameterInfo(
                    name=param_name,
                    param_type=param_type,
                    required=required,
                    default_value=default_value,
                    element_type=element_type,
                )
            )

        return params

    @classmethod
    def get_info_dict(cls) -> Dict[str, Any]:
        """
        Get tool info as a dictionary (for pybind11 serialization).

        Returns:
            Dict containing all tool metadata
        """
        try:
            info = cls.get_tool_info()
            # Add discovered functions if not already present
            if not info.functions:
                exported = cls.get_exported_functions()
                info.functions = list(exported.values())
            return {"success": True, "data": info.to_dict()}
        except Exception as e:
            logger.exception(f"Error getting tool info: {e}")
            return {"success": False, "error": str(e), "data": {}}

    @classmethod
    def get_function_list(cls) -> Dict[str, Any]:
        """
        Get list of exported functions.

        Returns:
            Dict containing success status and function list
        """
        try:
            exported = cls.get_exported_functions()
            return {
                "success": True,
                "functions": [f.to_dict() for f in exported.values()],
                "count": len(exported),
            }
        except Exception as e:
            logger.exception(f"Error getting function list: {e}")
            return {"success": False, "error": str(e), "functions": [], "count": 0}
