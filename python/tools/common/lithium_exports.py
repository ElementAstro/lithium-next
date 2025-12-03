"""
Lithium export declarations for common module.

This module provides discovery and management functions for all
lithium_exports modules in the tools package.
"""

import importlib
from pathlib import Path

from lithium_bridge import expose_command, expose_controller
from lithium_bridge.exporter import get_export_manifest


@expose_controller(
    endpoint="/api/tools/discover",
    method="GET",
    description="Discover all available tool modules with exports",
    tags=["tools", "discovery"],
    version="1.0.0"
)
def discover_tools() -> dict:
    """
    Discover all available tool modules with lithium exports.

    Returns:
        Dictionary with discovered modules and their exports
    """
    tools_dir = Path(__file__).parent.parent
    discovered = {}

    for item in tools_dir.iterdir():
        if not item.is_dir():
            continue
        if item.name.startswith("_"):
            continue

        exports_file = item / "lithium_exports.py"
        if exports_file.exists():
            module_name = f"tools.{item.name}.lithium_exports"
            try:
                module = importlib.import_module(module_name)
                manifest = get_export_manifest(module)
                discovered[item.name] = {
                    "module": module_name,
                    "controllers": len(manifest.get("exports", {}).get("controllers", [])),
                    "commands": len(manifest.get("exports", {}).get("commands", [])),
                }
            except Exception as e:
                discovered[item.name] = {
                    "module": module_name,
                    "error": str(e),
                }

    return {
        "tools_count": len(discovered),
        "tools": discovered,
    }


@expose_controller(
    endpoint="/api/tools/manifest",
    method="GET",
    description="Get the full export manifest for a tool module",
    tags=["tools", "discovery"],
    version="1.0.0"
)
def get_tool_manifest(tool_name: str) -> dict:
    """
    Get the full export manifest for a specific tool module.

    Args:
        tool_name: Name of the tool module

    Returns:
        Dictionary with the tool's export manifest
    """
    module_name = f"tools.{tool_name}.lithium_exports"
    try:
        module = importlib.import_module(module_name)
        manifest = get_export_manifest(module)
        return {
            "tool": tool_name,
            "manifest": manifest,
            "success": True,
        }
    except Exception as e:
        return {
            "tool": tool_name,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/tools/all-exports",
    method="GET",
    description="Get all exports from all tool modules",
    tags=["tools", "discovery"],
    version="1.0.0"
)
def get_all_exports() -> dict:
    """
    Get all exports from all tool modules.

    Returns:
        Dictionary with all exports organized by tool
    """
    tools_dir = Path(__file__).parent.parent
    all_exports = {
        "controllers": [],
        "commands": [],
    }

    for item in tools_dir.iterdir():
        if not item.is_dir() or item.name.startswith("_"):
            continue

        exports_file = item / "lithium_exports.py"
        if not exports_file.exists():
            continue

        module_name = f"tools.{item.name}.lithium_exports"
        try:
            module = importlib.import_module(module_name)
            manifest = get_export_manifest(module)
            exports = manifest.get("exports", {})

            for ctrl in exports.get("controllers", []):
                ctrl["source_module"] = item.name
                all_exports["controllers"].append(ctrl)

            for cmd in exports.get("commands", []):
                cmd["source_module"] = item.name
                all_exports["commands"].append(cmd)

        except Exception:
            continue

    return {
        "total_controllers": len(all_exports["controllers"]),
        "total_commands": len(all_exports["commands"]),
        "exports": all_exports,
    }


@expose_command(
    command_id="tools.discover",
    description="Discover tool modules (command)",
    priority=5,
    timeout_ms=10000,
    tags=["tools"]
)
def cmd_discover_tools() -> dict:
    """Discover tools via command dispatcher."""
    return discover_tools()


@expose_command(
    command_id="tools.list_exports",
    description="List all exports (command)",
    priority=5,
    timeout_ms=10000,
    tags=["tools"]
)
def cmd_list_exports() -> dict:
    """List all exports via command dispatcher."""
    return get_all_exports()
