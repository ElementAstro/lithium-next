"""
Lithium Bridge Exporter - Decorators and utilities for exposing Python functions

This module provides the core functionality for marking Python functions as
exportable controllers or commands that can be automatically registered by
the Lithium server.
"""

from __future__ import annotations

import functools
import inspect
import json
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import (
    Any,
    Callable,
    Dict,
    List,
    Optional,
    TypeVar,
    Union,
    get_type_hints,
)

# Module-level attribute name for storing exports
LITHIUM_EXPORTS_ATTR = "__lithium_exports__"

# Type variable for decorated functions
F = TypeVar("F", bound=Callable[..., Any])


class ExportType(Enum):
    """Type of export - controller (HTTP endpoint) or command (dispatcher)"""

    CONTROLLER = auto()
    COMMAND = auto()


class HttpMethod(Enum):
    """Supported HTTP methods for controllers"""

    GET = "GET"
    POST = "POST"
    PUT = "PUT"
    DELETE = "DELETE"
    PATCH = "PATCH"


@dataclass
class ParameterInfo:
    """Information about a function parameter"""

    name: str
    type_name: str
    required: bool = True
    default_value: Optional[Any] = None
    description: str = ""

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        result = {
            "name": self.name,
            "type": self.type_name,
            "required": self.required,
            "description": self.description,
        }
        if self.default_value is not None:
            result["default"] = self.default_value
        return result


@dataclass
class ExportInfo:
    """Complete information about an exported function"""

    name: str
    export_type: ExportType
    function: Callable[..., Any]
    description: str = ""
    parameters: List[ParameterInfo] = field(default_factory=list)
    return_type: str = "Any"

    # Controller-specific fields
    endpoint: Optional[str] = None
    method: HttpMethod = HttpMethod.POST
    content_type: str = "application/json"

    # Command-specific fields
    command_id: Optional[str] = None
    priority: int = 0
    timeout_ms: int = 5000

    # Common metadata
    tags: List[str] = field(default_factory=list)
    version: str = "1.0.0"
    deprecated: bool = False
    deprecation_message: str = ""

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        result = {
            "name": self.name,
            "export_type": self.export_type.name.lower(),
            "description": self.description,
            "parameters": [p.to_dict() for p in self.parameters],
            "return_type": self.return_type,
            "tags": self.tags,
            "version": self.version,
            "deprecated": self.deprecated,
        }

        if self.deprecated and self.deprecation_message:
            result["deprecation_message"] = self.deprecation_message

        if self.export_type == ExportType.CONTROLLER:
            result["endpoint"] = self.endpoint
            result["method"] = self.method.value
            result["content_type"] = self.content_type
        else:
            result["command_id"] = self.command_id
            result["priority"] = self.priority
            result["timeout_ms"] = self.timeout_ms

        return result


def _extract_parameters(func: Callable[..., Any]) -> List[ParameterInfo]:
    """Extract parameter information from a function signature"""
    parameters = []
    sig = inspect.signature(func)

    # Try to get type hints
    try:
        hints = get_type_hints(func)
    except Exception:
        hints = {}

    for param_name, param in sig.parameters.items():
        if param_name in ("self", "cls"):
            continue

        # Determine type name
        type_hint = hints.get(param_name, param.annotation)
        if type_hint is inspect.Parameter.empty:
            type_name = "Any"
        elif hasattr(type_hint, "__name__"):
            type_name = type_hint.__name__
        else:
            type_name = str(type_hint)

        # Determine if required and default value
        has_default = param.default is not inspect.Parameter.empty
        default_value = param.default if has_default else None

        # Make default JSON-serializable
        if default_value is not None:
            try:
                json.dumps(default_value)
            except (TypeError, ValueError):
                default_value = str(default_value)

        parameters.append(
            ParameterInfo(
                name=param_name,
                type_name=type_name,
                required=not has_default,
                default_value=default_value,
                description="",  # Can be enhanced with docstring parsing
            )
        )

    return parameters


def _extract_return_type(func: Callable[..., Any]) -> str:
    """Extract return type from function signature"""
    try:
        hints = get_type_hints(func)
        return_hint = hints.get("return")
        if return_hint is None:
            return "None"
        elif hasattr(return_hint, "__name__"):
            return return_hint.__name__
        else:
            return str(return_hint)
    except Exception:
        return "Any"


def _parse_docstring(func: Callable[..., Any]) -> Dict[str, str]:
    """Parse docstring to extract description and parameter descriptions"""
    result = {"description": "", "params": {}}

    docstring = inspect.getdoc(func)
    if not docstring:
        return result

    lines = docstring.strip().split("\n")
    description_lines = []
    current_param = None

    for line in lines:
        stripped = line.strip()

        # Check for parameter documentation
        if stripped.startswith(":param ") or stripped.startswith("@param "):
            # Parse :param name: description format
            parts = (
                stripped.split(":", 2)
                if stripped.startswith(":")
                else stripped.split(" ", 2)
            )
            if len(parts) >= 2:
                param_part = parts[1].strip()
                param_name = param_part.split()[0] if param_part else ""
                param_desc = parts[2].strip() if len(parts) > 2 else ""
                result["params"][param_name] = param_desc
                current_param = param_name
        elif stripped.startswith(":return") or stripped.startswith("@return"):
            current_param = None
        elif current_param and stripped:
            # Continuation of parameter description
            result["params"][current_param] += " " + stripped
        elif not stripped.startswith(":") and not stripped.startswith("@"):
            description_lines.append(stripped)

    result["description"] = " ".join(description_lines).strip()
    return result


def expose_controller(
    endpoint: str,
    method: Union[str, HttpMethod] = HttpMethod.POST,
    description: str = "",
    content_type: str = "application/json",
    tags: Optional[List[str]] = None,
    version: str = "1.0.0",
    deprecated: bool = False,
    deprecation_message: str = "",
) -> Callable[[F], F]:
    """
    Decorator to expose a Python function as an HTTP controller endpoint.

    Args:
        endpoint: The HTTP endpoint path (e.g., "/api/my_function")
        method: HTTP method (GET, POST, PUT, DELETE, PATCH)
        description: Human-readable description of the endpoint
        content_type: Response content type
        tags: List of tags for categorization
        version: API version
        deprecated: Whether this endpoint is deprecated
        deprecation_message: Message explaining deprecation

    Returns:
        Decorated function with export metadata attached

    Example:
        @expose_controller(
            endpoint="/api/calculate",
            method="POST",
            description="Perform calculation"
        )
        def calculate(x: int, y: int) -> dict:
            return {"result": x + y}
    """
    if isinstance(method, str):
        method = HttpMethod(method.upper())

    def decorator(func: F) -> F:
        # Extract function metadata
        parameters = _extract_parameters(func)
        return_type = _extract_return_type(func)
        docstring_info = _parse_docstring(func)

        # Update parameter descriptions from docstring
        for param in parameters:
            if param.name in docstring_info["params"]:
                param.description = docstring_info["params"][param.name]

        # Use docstring description if not provided
        final_description = description or docstring_info["description"]

        # Create export info
        export_info = ExportInfo(
            name=func.__name__,
            export_type=ExportType.CONTROLLER,
            function=func,
            description=final_description,
            parameters=parameters,
            return_type=return_type,
            endpoint=endpoint,
            method=method,
            content_type=content_type,
            tags=tags or [],
            version=version,
            deprecated=deprecated,
            deprecation_message=deprecation_message,
        )

        # Attach export info to function
        if not hasattr(func, LITHIUM_EXPORTS_ATTR):
            setattr(func, LITHIUM_EXPORTS_ATTR, [])
        getattr(func, LITHIUM_EXPORTS_ATTR).append(export_info)

        @functools.wraps(func)
        def wrapper(*args: Any, **kwargs: Any) -> Any:
            return func(*args, **kwargs)

        # Copy export info to wrapper
        setattr(wrapper, LITHIUM_EXPORTS_ATTR, getattr(func, LITHIUM_EXPORTS_ATTR))

        return wrapper  # type: ignore

    return decorator


def expose_command(
    command_id: str,
    description: str = "",
    priority: int = 0,
    timeout_ms: int = 5000,
    tags: Optional[List[str]] = None,
    version: str = "1.0.0",
    deprecated: bool = False,
    deprecation_message: str = "",
) -> Callable[[F], F]:
    """
    Decorator to expose a Python function as a command for the dispatcher.

    Args:
        command_id: Unique command identifier (e.g., "my.module.command")
        description: Human-readable description of the command
        priority: Execution priority (higher values execute first)
        timeout_ms: Command timeout in milliseconds
        tags: List of tags for categorization
        version: Command version
        deprecated: Whether this command is deprecated
        deprecation_message: Message explaining deprecation

    Returns:
        Decorated function with export metadata attached

    Example:
        @expose_command(
            command_id="image.process",
            description="Process an image",
            timeout_ms=10000
        )
        def process_image(image_path: str, options: dict) -> dict:
            return {"processed": True}
    """

    def decorator(func: F) -> F:
        # Extract function metadata
        parameters = _extract_parameters(func)
        return_type = _extract_return_type(func)
        docstring_info = _parse_docstring(func)

        # Update parameter descriptions from docstring
        for param in parameters:
            if param.name in docstring_info["params"]:
                param.description = docstring_info["params"][param.name]

        # Use docstring description if not provided
        final_description = description or docstring_info["description"]

        # Create export info
        export_info = ExportInfo(
            name=func.__name__,
            export_type=ExportType.COMMAND,
            function=func,
            description=final_description,
            parameters=parameters,
            return_type=return_type,
            command_id=command_id,
            priority=priority,
            timeout_ms=timeout_ms,
            tags=tags or [],
            version=version,
            deprecated=deprecated,
            deprecation_message=deprecation_message,
        )

        # Attach export info to function
        if not hasattr(func, LITHIUM_EXPORTS_ATTR):
            setattr(func, LITHIUM_EXPORTS_ATTR, [])
        getattr(func, LITHIUM_EXPORTS_ATTR).append(export_info)

        @functools.wraps(func)
        def wrapper(*args: Any, **kwargs: Any) -> Any:
            return func(*args, **kwargs)

        # Copy export info to wrapper
        setattr(wrapper, LITHIUM_EXPORTS_ATTR, getattr(func, LITHIUM_EXPORTS_ATTR))

        return wrapper  # type: ignore

    return decorator


def get_exports(module: Any) -> List[ExportInfo]:
    """
    Get all exports from a module.

    This function scans a module for functions decorated with @expose_controller
    or @expose_command and returns their export information.

    Args:
        module: The module to scan for exports

    Returns:
        List of ExportInfo objects for all exported functions
    """
    exports = []

    # Check for module-level __lithium_exports__ attribute (from JSON manifest)
    if hasattr(module, LITHIUM_EXPORTS_ATTR):
        module_exports = getattr(module, LITHIUM_EXPORTS_ATTR)
        if isinstance(module_exports, list):
            exports.extend(module_exports)

    # Scan all module members for decorated functions
    for name in dir(module):
        if name.startswith("_"):
            continue

        try:
            obj = getattr(module, name)
        except AttributeError:
            continue

        if callable(obj) and hasattr(obj, LITHIUM_EXPORTS_ATTR):
            func_exports = getattr(obj, LITHIUM_EXPORTS_ATTR)
            if isinstance(func_exports, list):
                for export in func_exports:
                    if export not in exports:
                        exports.append(export)

    return exports


def get_export_manifest(module: Any) -> Dict[str, Any]:
    """
    Get the complete export manifest for a module as a dictionary.

    This is useful for serialization and transmission to the C++ side.

    Args:
        module: The module to get manifest for

    Returns:
        Dictionary containing all export information
    """
    exports = get_exports(module)

    manifest = {
        "module_name": getattr(module, "__name__", "unknown"),
        "module_file": getattr(module, "__file__", ""),
        "version": getattr(module, "__version__", "1.0.0"),
        "exports": {
            "controllers": [],
            "commands": [],
        },
    }

    for export in exports:
        export_dict = export.to_dict()
        if export.export_type == ExportType.CONTROLLER:
            manifest["exports"]["controllers"].append(export_dict)
        else:
            manifest["exports"]["commands"].append(export_dict)

    return manifest


def validate_exports(module: Any) -> List[str]:
    """
    Validate all exports in a module and return any errors.

    Args:
        module: The module to validate

    Returns:
        List of error messages (empty if valid)
    """
    errors = []
    exports = get_exports(module)

    seen_endpoints = set()
    seen_command_ids = set()

    for export in exports:
        # Validate common fields
        if not export.name:
            errors.append("Export missing name")

        if not export.description:
            errors.append(f"Export '{export.name}' missing description")

        # Validate controller-specific fields
        if export.export_type == ExportType.CONTROLLER:
            if not export.endpoint:
                errors.append(f"Controller '{export.name}' missing endpoint")
            elif export.endpoint in seen_endpoints:
                errors.append(f"Duplicate endpoint '{export.endpoint}'")
            else:
                seen_endpoints.add(export.endpoint)

            if not export.endpoint.startswith("/"):
                errors.append(
                    f"Controller '{export.name}' endpoint must start with '/'"
                )

        # Validate command-specific fields
        if export.export_type == ExportType.COMMAND:
            if not export.command_id:
                errors.append(f"Command '{export.name}' missing command_id")
            elif export.command_id in seen_command_ids:
                errors.append(f"Duplicate command_id '{export.command_id}'")
            else:
                seen_command_ids.add(export.command_id)

            if export.timeout_ms <= 0:
                errors.append(f"Command '{export.name}' has invalid timeout_ms")

    return errors


# Convenience function for registering exports from JSON manifest
def register_from_manifest(
    module: Any,
    manifest: Dict[str, Any],
) -> None:
    """
    Register exports from a JSON manifest into a module.

    This allows scripts to define exports in a JSON file instead of using
    decorators.

    Args:
        module: The module to register exports into
        manifest: The manifest dictionary
    """
    if not hasattr(module, LITHIUM_EXPORTS_ATTR):
        setattr(module, LITHIUM_EXPORTS_ATTR, [])

    exports_list = getattr(module, LITHIUM_EXPORTS_ATTR)

    # Process controllers
    for ctrl in manifest.get("exports", {}).get("controllers", []):
        func_name = ctrl.get("function", ctrl.get("name"))
        if not hasattr(module, func_name):
            continue

        func = getattr(module, func_name)
        parameters = _extract_parameters(func)

        export_info = ExportInfo(
            name=ctrl.get("name", func_name),
            export_type=ExportType.CONTROLLER,
            function=func,
            description=ctrl.get("description", ""),
            parameters=parameters,
            return_type=_extract_return_type(func),
            endpoint=ctrl.get("endpoint"),
            method=HttpMethod(ctrl.get("method", "POST").upper()),
            content_type=ctrl.get("content_type", "application/json"),
            tags=ctrl.get("tags", []),
            version=ctrl.get("version", "1.0.0"),
            deprecated=ctrl.get("deprecated", False),
            deprecation_message=ctrl.get("deprecation_message", ""),
        )
        exports_list.append(export_info)

    # Process commands
    for cmd in manifest.get("exports", {}).get("commands", []):
        func_name = cmd.get("function", cmd.get("name"))
        if not hasattr(module, func_name):
            continue

        func = getattr(module, func_name)
        parameters = _extract_parameters(func)

        export_info = ExportInfo(
            name=cmd.get("name", func_name),
            export_type=ExportType.COMMAND,
            function=func,
            description=cmd.get("description", ""),
            parameters=parameters,
            return_type=_extract_return_type(func),
            command_id=cmd.get("command_id"),
            priority=cmd.get("priority", 0),
            timeout_ms=cmd.get("timeout_ms", 5000),
            tags=cmd.get("tags", []),
            version=cmd.get("version", "1.0.0"),
            deprecated=cmd.get("deprecated", False),
            deprecation_message=cmd.get("deprecation_message", ""),
        )
        exports_list.append(export_info)
