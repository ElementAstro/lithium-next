#!/usr/bin/env python3
"""
Tool Discovery and Registry for Lithium-Next Python Tools.

This module provides automatic discovery and registration of Python tools
with pybind11 adapters. It scans the tools directory, loads adapters,
and maintains a centralized registry accessible from C++.

Features:
- Automatic discovery of tools with pybind_adapter.py files
- Dynamic loading and introspection of adapters
- Thread-safe registry for concurrent access
- JSON-serializable registry export for C++ binding
- Hot-reload support for development
"""

import importlib
import importlib.util
import json
import os
import sys
import threading
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set, Type, Union

from loguru import logger

from .base_adapter import BasePybindAdapter, ToolInfo, FunctionInfo


@dataclass
class RegisteredTool:
    """Represents a discovered and registered tool."""

    name: str
    module_path: str
    adapter_class: Optional[Type[BasePybindAdapter]] = None
    adapter_instance: Optional[Any] = None
    tool_info: Optional[ToolInfo] = None
    functions: Dict[str, Callable] = field(default_factory=dict)
    is_loaded: bool = False
    load_error: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        result = {
            "name": self.name,
            "module_path": self.module_path,
            "is_loaded": self.is_loaded,
        }
        if self.tool_info:
            result["info"] = self.tool_info.to_dict()
        if self.load_error:
            result["error"] = self.load_error
        if self.functions:
            result["function_names"] = list(self.functions.keys())
        return result


class ToolRegistry:
    """
    Central registry for discovered Python tools.

    Thread-safe singleton that maintains a registry of all discovered
    tools and their pybind11 adapters. Provides methods for:
    - Registering tools manually or via discovery
    - Looking up tools by name
    - Invoking tool functions
    - Exporting registry for C++ consumption
    """

    _instance: Optional["ToolRegistry"] = None
    _lock = threading.Lock()

    def __new__(cls) -> "ToolRegistry":
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = super().__new__(cls)
                    cls._instance._initialized = False
        return cls._instance

    def __init__(self):
        if self._initialized:
            return

        self._tools: Dict[str, RegisteredTool] = {}
        self._categories: Dict[str, Set[str]] = {}  # category -> tool names
        self._tool_lock = threading.RLock()
        self._initialized = True

    def register(
        self,
        name: str,
        adapter_class: Type[BasePybindAdapter],
        module_path: str = "",
    ) -> bool:
        """
        Register a tool adapter.

        Args:
            name: Unique tool name
            adapter_class: Pybind adapter class
            module_path: Import path for the module

        Returns:
            True if registration succeeded
        """
        with self._tool_lock:
            try:
                # Get tool info
                tool_info = adapter_class.get_tool_info()

                # Get exported functions
                exported = adapter_class.get_exported_functions()

                # Build function map
                functions = {}
                for func_name, func_info in exported.items():
                    method = getattr(adapter_class, func_name, None)
                    if method and callable(method):
                        functions[func_name] = method

                tool = RegisteredTool(
                    name=name,
                    module_path=module_path,
                    adapter_class=adapter_class,
                    tool_info=tool_info,
                    functions=functions,
                    is_loaded=True,
                )

                self._tools[name] = tool

                # Index by categories
                for category in tool_info.categories:
                    if category not in self._categories:
                        self._categories[category] = set()
                    self._categories[category].add(name)

                logger.info(f"Registered tool: {name} with {len(functions)} functions")
                return True

            except Exception as e:
                logger.exception(f"Failed to register tool {name}: {e}")
                self._tools[name] = RegisteredTool(
                    name=name,
                    module_path=module_path,
                    is_loaded=False,
                    load_error=str(e),
                )
                return False

    def unregister(self, name: str) -> bool:
        """
        Unregister a tool.

        Args:
            name: Tool name to unregister

        Returns:
            True if the tool was found and unregistered
        """
        with self._tool_lock:
            if name in self._tools:
                tool = self._tools.pop(name)

                # Remove from category index
                if tool.tool_info:
                    for category in tool.tool_info.categories:
                        if category in self._categories:
                            self._categories[category].discard(name)

                logger.info(f"Unregistered tool: {name}")
                return True
            return False

    def get_tool(self, name: str) -> Optional[RegisteredTool]:
        """
        Get a registered tool by name.

        Args:
            name: Tool name

        Returns:
            RegisteredTool if found, None otherwise
        """
        with self._tool_lock:
            return self._tools.get(name)

    def get_all_tools(self) -> Dict[str, RegisteredTool]:
        """
        Get all registered tools.

        Returns:
            Dict mapping tool names to RegisteredTool objects
        """
        with self._tool_lock:
            return dict(self._tools)

    def get_tools_by_category(self, category: str) -> List[str]:
        """
        Get tool names in a category.

        Args:
            category: Category name

        Returns:
            List of tool names in the category
        """
        with self._tool_lock:
            return list(self._categories.get(category, set()))

    def invoke(
        self,
        tool_name: str,
        function_name: str,
        *args,
        **kwargs,
    ) -> Dict[str, Any]:
        """
        Invoke a function on a registered tool.

        This is the primary entry point for C++ to call Python tools.

        Args:
            tool_name: Name of the tool
            function_name: Name of the function to call
            *args: Positional arguments
            **kwargs: Keyword arguments

        Returns:
            Dict with success status and result/error
        """
        with self._tool_lock:
            tool = self._tools.get(tool_name)
            if not tool:
                return {
                    "success": False,
                    "error": f"Tool not found: {tool_name}",
                    "error_type": "ToolNotFoundError",
                }

            if not tool.is_loaded:
                return {
                    "success": False,
                    "error": f"Tool not loaded: {tool_name}. Error: {tool.load_error}",
                    "error_type": "ToolNotLoadedError",
                }

            func = tool.functions.get(function_name)
            if not func:
                return {
                    "success": False,
                    "error": f"Function not found: {function_name} in tool {tool_name}",
                    "error_type": "FunctionNotFoundError",
                }

        # Call outside the lock to avoid deadlocks
        try:
            result = func(*args, **kwargs)
            if isinstance(result, dict):
                return result
            return {"success": True, "data": result}
        except Exception as e:
            logger.exception(f"Error invoking {tool_name}.{function_name}: {e}")
            return {
                "success": False,
                "error": str(e),
                "error_type": type(e).__name__,
            }

    def export_registry(self) -> Dict[str, Any]:
        """
        Export the registry as a dictionary for C++ binding.

        Returns:
            Dict containing all tool information
        """
        with self._tool_lock:
            return {
                "tools": {name: tool.to_dict() for name, tool in self._tools.items()},
                "categories": {cat: list(names) for cat, names in self._categories.items()},
                "count": len(self._tools),
            }

    def export_json(self) -> str:
        """
        Export the registry as JSON string.

        Returns:
            JSON string representation of the registry
        """
        return json.dumps(self.export_registry(), indent=2)

    def clear(self) -> None:
        """Clear all registered tools."""
        with self._tool_lock:
            self._tools.clear()
            self._categories.clear()


class ToolDiscovery:
    """
    Discovers and loads Python tools from the tools directory.

    Scans subdirectories for pybind_adapter.py files, loads them,
    and registers any BasePybindAdapter subclasses found.
    """

    def __init__(
        self,
        tools_dir: Optional[Path] = None,
        registry: Optional[ToolRegistry] = None,
    ):
        """
        Initialize the discovery system.

        Args:
            tools_dir: Directory containing tool subdirectories
            registry: Registry to register discovered tools (uses singleton if None)
        """
        if tools_dir is None:
            # Default to python/tools directory relative to this file
            tools_dir = Path(__file__).parent.parent
        self.tools_dir = Path(tools_dir)
        self.registry = registry or get_tool_registry()
        self._discovered: Set[str] = set()

    def discover_all(self) -> List[str]:
        """
        Discover and register all tools in the tools directory.

        Returns:
            List of successfully registered tool names
        """
        registered = []

        if not self.tools_dir.exists():
            logger.warning(f"Tools directory not found: {self.tools_dir}")
            return registered

        for item in self.tools_dir.iterdir():
            if not item.is_dir():
                continue

            # Skip __pycache__ and common
            if item.name.startswith("_") or item.name == "common":
                continue

            result = self.discover_tool(item.name)
            if result:
                registered.append(item.name)

        return registered

    def discover_tool(self, tool_name: str) -> bool:
        """
        Discover and register a single tool.

        Args:
            tool_name: Name of the tool (directory name)

        Returns:
            True if the tool was successfully discovered and registered
        """
        tool_dir = self.tools_dir / tool_name

        # Check for pybind_adapter.py
        adapter_path = tool_dir / "pybind_adapter.py"
        if not adapter_path.exists():
            logger.debug(f"No pybind_adapter.py found in {tool_dir}")
            return False

        try:
            # Load the adapter module
            module_name = f"python.tools.{tool_name}.pybind_adapter"
            spec = importlib.util.spec_from_file_location(module_name, adapter_path)
            if spec is None or spec.loader is None:
                logger.error(f"Failed to create spec for {adapter_path}")
                return False

            module = importlib.util.module_from_spec(spec)
            sys.modules[module_name] = module
            spec.loader.exec_module(module)

            # Find adapter classes
            adapter_classes = []
            for attr_name in dir(module):
                attr = getattr(module, attr_name)
                if (
                    isinstance(attr, type)
                    and issubclass(attr, BasePybindAdapter)
                    and attr is not BasePybindAdapter
                ):
                    adapter_classes.append(attr)

            # Also check for legacy adapter pattern (class with get_tool_info method)
            if not adapter_classes:
                for attr_name in dir(module):
                    attr = getattr(module, attr_name)
                    if (
                        isinstance(attr, type)
                        and hasattr(attr, "get_tool_info")
                        and attr_name.endswith("Adapter")
                    ):
                        adapter_classes.append(attr)

            if not adapter_classes:
                logger.warning(f"No adapter class found in {adapter_path}")
                return False

            # Register the first (or only) adapter
            adapter_class = adapter_classes[0]
            success = self.registry.register(
                name=tool_name,
                adapter_class=adapter_class,
                module_path=module_name,
            )

            if success:
                self._discovered.add(tool_name)

            return success

        except Exception as e:
            logger.exception(f"Error discovering tool {tool_name}: {e}")
            return False

    def reload_tool(self, tool_name: str) -> bool:
        """
        Reload a tool's adapter (for hot-reload during development).

        Args:
            tool_name: Name of the tool to reload

        Returns:
            True if reload succeeded
        """
        # Unregister existing
        self.registry.unregister(tool_name)
        self._discovered.discard(tool_name)

        # Unload module from sys.modules
        module_name = f"python.tools.{tool_name}.pybind_adapter"
        if module_name in sys.modules:
            del sys.modules[module_name]

        # Re-discover
        return self.discover_tool(tool_name)

    def get_discovered(self) -> Set[str]:
        """Get set of discovered tool names."""
        return self._discovered.copy()


# Global registry instance
_global_registry: Optional[ToolRegistry] = None
_registry_lock = threading.Lock()


def get_tool_registry() -> ToolRegistry:
    """
    Get the global tool registry singleton.

    Returns:
        The global ToolRegistry instance
    """
    global _global_registry
    if _global_registry is None:
        with _registry_lock:
            if _global_registry is None:
                _global_registry = ToolRegistry()
    return _global_registry


def discover_tools(
    tools_dir: Optional[Path] = None,
    auto_register: bool = True,
) -> Dict[str, Any]:
    """
    Discover all Python tools and optionally register them.

    This is the main entry point for C++ to discover available tools.

    Args:
        tools_dir: Directory containing tool subdirectories
        auto_register: Whether to automatically register discovered tools

    Returns:
        Dict containing discovered tools and registration status
    """
    try:
        discovery = ToolDiscovery(tools_dir)

        if auto_register:
            registered = discovery.discover_all()
            registry = get_tool_registry()
            return {
                "success": True,
                "discovered": list(discovery.get_discovered()),
                "registered": registered,
                "registry": registry.export_registry(),
            }
        else:
            # Just scan for available tools without loading
            tools_dir = discovery.tools_dir
            available = []
            for item in tools_dir.iterdir():
                if item.is_dir() and not item.name.startswith("_"):
                    adapter_path = item / "pybind_adapter.py"
                    if adapter_path.exists():
                        available.append(item.name)
            return {
                "success": True,
                "available": available,
                "count": len(available),
            }

    except Exception as e:
        logger.exception(f"Error discovering tools: {e}")
        return {
            "success": False,
            "error": str(e),
            "discovered": [],
        }


# Pybind11-compatible functions for C++ binding
def invoke_tool(
    tool_name: str,
    function_name: str,
    args_json: str = "{}",
) -> Dict[str, Any]:
    """
    Invoke a tool function (C++ entry point).

    Args:
        tool_name: Name of the tool
        function_name: Name of the function
        args_json: JSON string of keyword arguments

    Returns:
        Dict with result or error
    """
    try:
        kwargs = json.loads(args_json)
        registry = get_tool_registry()
        return registry.invoke(tool_name, function_name, **kwargs)
    except json.JSONDecodeError as e:
        return {
            "success": False,
            "error": f"Invalid JSON arguments: {e}",
            "error_type": "JSONDecodeError",
        }


def get_registry_json() -> str:
    """
    Get the tool registry as JSON (C++ entry point).

    Returns:
        JSON string of the registry
    """
    return get_tool_registry().export_json()


def get_tool_info_json(tool_name: str) -> str:
    """
    Get info for a specific tool as JSON (C++ entry point).

    Args:
        tool_name: Name of the tool

    Returns:
        JSON string with tool info
    """
    registry = get_tool_registry()
    tool = registry.get_tool(tool_name)
    if tool:
        return json.dumps(tool.to_dict())
    return json.dumps({"error": f"Tool not found: {tool_name}"})
