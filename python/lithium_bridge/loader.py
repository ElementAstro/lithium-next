"""
Lithium Bridge - Export Module Loader

This module provides utilities for discovering and loading all
lithium_exports modules from the tools package.
"""

import importlib
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

from .exporter import get_export_manifest


def discover_export_modules(
    base_path: Optional[Path] = None,
    pattern: str = "lithium_exports.py"
) -> List[Tuple[str, Path]]:
    """
    Discover all modules containing lithium exports.

    Args:
        base_path: Base path to search (defaults to python directory)
        pattern: Filename pattern to search for

    Returns:
        List of tuples (module_name, file_path)
    """
    if base_path is None:
        base_path = Path(__file__).parent.parent

    discovered = []

    for exports_file in base_path.rglob(pattern):
        # Skip __pycache__ directories
        if "__pycache__" in str(exports_file):
            continue

        # Calculate module name from path
        rel_path = exports_file.relative_to(base_path)
        parts = list(rel_path.parts)
        parts[-1] = parts[-1].replace(".py", "")
        module_name = ".".join(parts)

        discovered.append((module_name, exports_file))

    return discovered


def load_export_module(module_name: str) -> Optional[Dict[str, Any]]:
    """
    Load a single export module and return its manifest.

    Args:
        module_name: Full module name to import

    Returns:
        Export manifest dictionary or None if failed
    """
    try:
        module = importlib.import_module(module_name)
        return get_export_manifest(module)
    except Exception as e:
        return {"error": str(e), "module": module_name}


def load_all_exports(
    base_path: Optional[Path] = None
) -> Dict[str, Dict[str, Any]]:
    """
    Load all export modules and return their manifests.

    Args:
        base_path: Base path to search

    Returns:
        Dictionary mapping module names to their manifests
    """
    modules = discover_export_modules(base_path)
    results = {}

    for module_name, file_path in modules:
        manifest = load_export_module(module_name)
        if manifest:
            results[module_name] = manifest

    return results


def get_all_endpoints(
    base_path: Optional[Path] = None
) -> List[Dict[str, Any]]:
    """
    Get all controller endpoints from all export modules.

    Args:
        base_path: Base path to search

    Returns:
        List of endpoint dictionaries
    """
    all_exports = load_all_exports(base_path)
    endpoints = []

    for module_name, manifest in all_exports.items():
        if "error" in manifest:
            continue

        exports = manifest.get("exports", {})
        for ctrl in exports.get("controllers", []):
            ctrl["source_module"] = module_name
            endpoints.append(ctrl)

    return endpoints


def get_all_commands(
    base_path: Optional[Path] = None
) -> List[Dict[str, Any]]:
    """
    Get all commands from all export modules.

    Args:
        base_path: Base path to search

    Returns:
        List of command dictionaries
    """
    all_exports = load_all_exports(base_path)
    commands = []

    for module_name, manifest in all_exports.items():
        if "error" in manifest:
            continue

        exports = manifest.get("exports", {})
        for cmd in exports.get("commands", []):
            cmd["source_module"] = module_name
            commands.append(cmd)

    return commands


def get_endpoint_handler(
    endpoint: str,
    base_path: Optional[Path] = None
) -> Optional[Callable]:
    """
    Get the handler function for a specific endpoint.

    Args:
        endpoint: The endpoint path
        base_path: Base path to search

    Returns:
        Handler function or None if not found
    """
    endpoints = get_all_endpoints(base_path)

    for ep in endpoints:
        if ep.get("endpoint") == endpoint:
            module_name = ep.get("source_module")
            func_name = ep.get("name")
            if module_name and func_name:
                try:
                    module = importlib.import_module(module_name)
                    return getattr(module, func_name, None)
                except Exception:
                    pass

    return None


def get_command_handler(
    command_id: str,
    base_path: Optional[Path] = None
) -> Optional[Callable]:
    """
    Get the handler function for a specific command.

    Args:
        command_id: The command ID
        base_path: Base path to search

    Returns:
        Handler function or None if not found
    """
    commands = get_all_commands(base_path)

    for cmd in commands:
        if cmd.get("command_id") == command_id:
            module_name = cmd.get("source_module")
            func_name = cmd.get("name")
            if module_name and func_name:
                try:
                    module = importlib.import_module(module_name)
                    return getattr(module, func_name, None)
                except Exception:
                    pass

    return None


def generate_openapi_spec(
    base_path: Optional[Path] = None,
    title: str = "Lithium Python API",
    version: str = "1.0.0",
) -> Dict[str, Any]:
    """
    Generate OpenAPI specification for all endpoints.

    Args:
        base_path: Base path to search
        title: API title
        version: API version

    Returns:
        OpenAPI specification dictionary
    """
    endpoints = get_all_endpoints(base_path)

    spec = {
        "openapi": "3.0.0",
        "info": {
            "title": title,
            "version": version,
        },
        "paths": {},
    }

    for ep in endpoints:
        endpoint = ep.get("endpoint", "")
        method = ep.get("method", "POST").lower()

        if endpoint not in spec["paths"]:
            spec["paths"][endpoint] = {}

        spec["paths"][endpoint][method] = {
            "summary": ep.get("description", ""),
            "tags": ep.get("tags", []),
            "operationId": ep.get("name", ""),
            "responses": {
                "200": {
                    "description": "Successful response",
                }
            },
        }

        # Add parameters
        params = ep.get("parameters", [])
        if params:
            if method == "get":
                spec["paths"][endpoint][method]["parameters"] = [
                    {
                        "name": p.get("name"),
                        "in": "query",
                        "required": p.get("required", True),
                        "schema": {"type": _map_type(p.get("type", "string"))},
                        "description": p.get("description", ""),
                    }
                    for p in params
                ]
            else:
                spec["paths"][endpoint][method]["requestBody"] = {
                    "content": {
                        "application/json": {
                            "schema": {
                                "type": "object",
                                "properties": {
                                    p.get("name"): {
                                        "type": _map_type(p.get("type", "string")),
                                        "description": p.get("description", ""),
                                    }
                                    for p in params
                                },
                                "required": [
                                    p.get("name") for p in params if p.get("required", True)
                                ],
                            }
                        }
                    }
                }

    return spec


def _map_type(python_type: str) -> str:
    """Map Python type to OpenAPI type."""
    type_map = {
        "str": "string",
        "int": "integer",
        "float": "number",
        "bool": "boolean",
        "list": "array",
        "dict": "object",
        "List": "array",
        "Dict": "object",
        "Any": "object",
    }
    return type_map.get(python_type, "string")
