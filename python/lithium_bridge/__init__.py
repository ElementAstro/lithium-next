"""
Lithium Bridge - Python Script Export Framework

This module provides decorators and utilities for exposing Python functions
as HTTP controllers or commands that can be automatically registered by the
Lithium server.

Usage:
    from lithium_bridge import expose_controller, expose_command, get_exports

    @expose_controller(
        endpoint="/api/my_function",
        method="POST",
        description="My function description"
    )
    def my_function(arg1: str, arg2: int = 10) -> dict:
        return {"result": arg1 * arg2}

    @expose_command(
        command_id="my.command",
        description="My command description"
    )
    def my_command(data: dict) -> dict:
        return {"processed": True}
"""

from .exporter import (
    expose_controller,
    expose_command,
    get_exports,
    get_export_manifest,
    validate_exports,
    ExportType,
    ExportInfo,
    ParameterInfo,
    LITHIUM_EXPORTS_ATTR,
)

from .manifest import (
    load_manifest_file,
    save_manifest_file,
    merge_manifests,
    ManifestLoader,
)

from .loader import (
    discover_export_modules,
    load_all_exports,
    get_all_endpoints,
    get_all_commands,
)

from .module_info import (
    ModuleInfo,
    ModuleRegistry,
    ModuleCategory,
    ModuleStatus,
    PlatformSupport,
    EndpointInfo,
    CommandInfo,
)

from .registry_api import (
    get_registry,
    reload_registry,
)

__version__ = "1.0.0"
__all__ = [
    # Decorators
    "expose_controller",
    "expose_command",
    # Export utilities
    "get_exports",
    "get_export_manifest",
    "validate_exports",
    # Types
    "ExportType",
    "ExportInfo",
    "ParameterInfo",
    # Manifest utilities
    "load_manifest_file",
    "save_manifest_file",
    "merge_manifests",
    "ManifestLoader",
    # Constants
    "LITHIUM_EXPORTS_ATTR",
    # Loader utilities
    "discover_export_modules",
    "load_all_exports",
    "get_all_endpoints",
    "get_all_commands",
    # Module info types
    "ModuleInfo",
    "ModuleRegistry",
    "ModuleCategory",
    "ModuleStatus",
    "PlatformSupport",
    "EndpointInfo",
    "CommandInfo",
    # Registry API
    "get_registry",
    "reload_registry",
]
