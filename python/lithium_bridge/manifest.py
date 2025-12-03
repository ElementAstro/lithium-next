"""
Lithium Bridge Manifest - JSON manifest loading and saving utilities

This module provides utilities for working with JSON manifest files that
define script exports without using decorators.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Union


def load_manifest_file(path: Union[str, Path]) -> Dict[str, Any]:
    """
    Load a manifest from a JSON file.
    
    Args:
        path: Path to the manifest JSON file
    
    Returns:
        Parsed manifest dictionary
    
    Raises:
        FileNotFoundError: If the manifest file doesn't exist
        json.JSONDecodeError: If the file contains invalid JSON
    """
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"Manifest file not found: {path}")
    
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save_manifest_file(
    manifest: Dict[str, Any],
    path: Union[str, Path],
    indent: int = 2,
) -> None:
    """
    Save a manifest to a JSON file.
    
    Args:
        manifest: The manifest dictionary to save
        path: Path to save the manifest to
        indent: JSON indentation level
    """
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=indent, ensure_ascii=False)


def merge_manifests(*manifests: Dict[str, Any]) -> Dict[str, Any]:
    """
    Merge multiple manifests into one.
    
    Args:
        *manifests: Variable number of manifest dictionaries
    
    Returns:
        Merged manifest dictionary
    """
    result = {
        "module_name": "merged",
        "version": "1.0.0",
        "exports": {
            "controllers": [],
            "commands": [],
        },
    }
    
    for manifest in manifests:
        exports = manifest.get("exports", {})
        result["exports"]["controllers"].extend(
            exports.get("controllers", [])
        )
        result["exports"]["commands"].extend(
            exports.get("commands", [])
        )
    
    return result


class ManifestLoader:
    """
    Utility class for loading and managing manifests from directories.
    
    This class can scan directories for manifest files and load them
    automatically.
    """
    
    def __init__(
        self,
        manifest_filename: str = "lithium_manifest.json",
        auto_discover: bool = True,
    ):
        """
        Initialize the manifest loader.
        
        Args:
            manifest_filename: Name of manifest files to look for
            auto_discover: Whether to auto-discover manifests in script directories
        """
        self.manifest_filename = manifest_filename
        self.auto_discover = auto_discover
        self._manifests: Dict[str, Dict[str, Any]] = {}
    
    def load_from_directory(
        self,
        directory: Union[str, Path],
        recursive: bool = True,
    ) -> List[Dict[str, Any]]:
        """
        Load all manifests from a directory.
        
        Args:
            directory: Directory to scan for manifests
            recursive: Whether to scan subdirectories
        
        Returns:
            List of loaded manifests
        """
        directory = Path(directory)
        manifests = []
        
        if not directory.exists():
            return manifests
        
        pattern = f"**/{self.manifest_filename}" if recursive else self.manifest_filename
        
        for manifest_path in directory.glob(pattern):
            try:
                manifest = load_manifest_file(manifest_path)
                manifest["_source_path"] = str(manifest_path)
                manifests.append(manifest)
                self._manifests[str(manifest_path)] = manifest
            except (json.JSONDecodeError, FileNotFoundError) as e:
                # Log error but continue loading other manifests
                print(f"Warning: Failed to load manifest {manifest_path}: {e}")
        
        return manifests
    
    def load_for_script(
        self,
        script_path: Union[str, Path],
    ) -> Optional[Dict[str, Any]]:
        """
        Load manifest for a specific script file.
        
        Looks for a manifest file in the same directory as the script,
        or a manifest file with the same name as the script.
        
        Args:
            script_path: Path to the Python script
        
        Returns:
            Manifest dictionary if found, None otherwise
        """
        script_path = Path(script_path)
        script_dir = script_path.parent
        script_name = script_path.stem
        
        # Try script-specific manifest first
        specific_manifest = script_dir / f"{script_name}.manifest.json"
        if specific_manifest.exists():
            return load_manifest_file(specific_manifest)
        
        # Try directory manifest
        dir_manifest = script_dir / self.manifest_filename
        if dir_manifest.exists():
            manifest = load_manifest_file(dir_manifest)
            # Filter to only include exports for this script
            return self._filter_manifest_for_script(manifest, script_name)
        
        return None
    
    def _filter_manifest_for_script(
        self,
        manifest: Dict[str, Any],
        script_name: str,
    ) -> Dict[str, Any]:
        """
        Filter a manifest to only include exports for a specific script.
        
        Args:
            manifest: The full manifest
            script_name: Name of the script to filter for
        
        Returns:
            Filtered manifest
        """
        result = {
            "module_name": script_name,
            "version": manifest.get("version", "1.0.0"),
            "exports": {
                "controllers": [],
                "commands": [],
            },
        }
        
        exports = manifest.get("exports", {})
        
        for ctrl in exports.get("controllers", []):
            if ctrl.get("script") == script_name or "script" not in ctrl:
                result["exports"]["controllers"].append(ctrl)
        
        for cmd in exports.get("commands", []):
            if cmd.get("script") == script_name or "script" not in cmd:
                result["exports"]["commands"].append(cmd)
        
        return result
    
    def get_all_manifests(self) -> Dict[str, Dict[str, Any]]:
        """
        Get all loaded manifests.
        
        Returns:
            Dictionary mapping file paths to manifests
        """
        return self._manifests.copy()
    
    def clear(self) -> None:
        """Clear all loaded manifests."""
        self._manifests.clear()


def create_manifest_template(
    module_name: str = "my_module",
    version: str = "1.0.0",
) -> Dict[str, Any]:
    """
    Create a template manifest dictionary.
    
    Args:
        module_name: Name of the module
        version: Module version
    
    Returns:
        Template manifest dictionary
    """
    return {
        "module_name": module_name,
        "version": version,
        "description": "Module description",
        "exports": {
            "controllers": [
                {
                    "name": "example_controller",
                    "function": "example_function",
                    "endpoint": "/api/example",
                    "method": "POST",
                    "description": "Example controller endpoint",
                    "parameters": [
                        {
                            "name": "param1",
                            "type": "str",
                            "required": True,
                            "description": "First parameter",
                        },
                        {
                            "name": "param2",
                            "type": "int",
                            "required": False,
                            "default": 10,
                            "description": "Second parameter",
                        },
                    ],
                    "return_type": "dict",
                    "tags": ["example"],
                    "version": "1.0.0",
                },
            ],
            "commands": [
                {
                    "name": "example_command",
                    "function": "example_command_func",
                    "command_id": "module.example",
                    "description": "Example command",
                    "priority": 0,
                    "timeout_ms": 5000,
                    "parameters": [
                        {
                            "name": "data",
                            "type": "dict",
                            "required": True,
                            "description": "Command data",
                        },
                    ],
                    "return_type": "dict",
                    "tags": ["example"],
                    "version": "1.0.0",
                },
            ],
        },
    }


def validate_manifest(manifest: Dict[str, Any]) -> List[str]:
    """
    Validate a manifest dictionary.
    
    Args:
        manifest: The manifest to validate
    
    Returns:
        List of validation error messages (empty if valid)
    """
    errors = []
    
    if "module_name" not in manifest:
        errors.append("Missing 'module_name' field")
    
    if "exports" not in manifest:
        errors.append("Missing 'exports' field")
        return errors
    
    exports = manifest["exports"]
    
    if not isinstance(exports, dict):
        errors.append("'exports' must be a dictionary")
        return errors
    
    seen_endpoints = set()
    seen_command_ids = set()
    
    # Validate controllers
    for i, ctrl in enumerate(exports.get("controllers", [])):
        prefix = f"controllers[{i}]"
        
        if "name" not in ctrl and "function" not in ctrl:
            errors.append(f"{prefix}: Missing 'name' or 'function'")
        
        if "endpoint" not in ctrl:
            errors.append(f"{prefix}: Missing 'endpoint'")
        else:
            endpoint = ctrl["endpoint"]
            if not endpoint.startswith("/"):
                errors.append(f"{prefix}: Endpoint must start with '/'")
            if endpoint in seen_endpoints:
                errors.append(f"{prefix}: Duplicate endpoint '{endpoint}'")
            seen_endpoints.add(endpoint)
        
        method = ctrl.get("method", "POST").upper()
        if method not in ("GET", "POST", "PUT", "DELETE", "PATCH"):
            errors.append(f"{prefix}: Invalid method '{method}'")
    
    # Validate commands
    for i, cmd in enumerate(exports.get("commands", [])):
        prefix = f"commands[{i}]"
        
        if "name" not in cmd and "function" not in cmd:
            errors.append(f"{prefix}: Missing 'name' or 'function'")
        
        if "command_id" not in cmd:
            errors.append(f"{prefix}: Missing 'command_id'")
        else:
            command_id = cmd["command_id"]
            if command_id in seen_command_ids:
                errors.append(f"{prefix}: Duplicate command_id '{command_id}'")
            seen_command_ids.add(command_id)
        
        timeout = cmd.get("timeout_ms", 5000)
        if not isinstance(timeout, int) or timeout <= 0:
            errors.append(f"{prefix}: Invalid timeout_ms")
    
    return errors
