#!/usr/bin/env python3
"""
Tests for the lithium_bridge module.

This module contains comprehensive tests for the Python exporter decorators
and manifest utilities.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Dict

# Add the python directory to the path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "python"))

from lithium_bridge import (
    expose_controller,
    expose_command,
    get_exports,
    get_export_manifest,
    validate_exports,
    ExportType,
    ParameterInfo,
    LITHIUM_EXPORTS_ATTR,
)
from lithium_bridge.manifest import (
    load_manifest_file,
    save_manifest_file,
    merge_manifests,
    ManifestLoader,
    create_manifest_template,
    validate_manifest,
)


class TestExposeControllerDecorator(unittest.TestCase):
    """Tests for the @expose_controller decorator."""

    def test_basic_decoration(self):
        """Test basic controller decoration."""

        @expose_controller(
            endpoint="/api/test",
            method="POST",
            description="Test endpoint",
        )
        def test_func(x: int, y: int) -> int:
            return x + y

        self.assertTrue(hasattr(test_func, LITHIUM_EXPORTS_ATTR))
        exports = getattr(test_func, LITHIUM_EXPORTS_ATTR)
        self.assertEqual(len(exports), 1)

        export = exports[0]
        self.assertEqual(export.name, "test_func")
        self.assertEqual(export.export_type, ExportType.CONTROLLER)
        self.assertEqual(export.endpoint, "/api/test")
        self.assertEqual(export.description, "Test endpoint")

    def test_http_methods(self):
        """Test different HTTP methods."""
        methods = ["GET", "POST", "PUT", "DELETE", "PATCH"]

        for method in methods:

            @expose_controller(endpoint=f"/api/{method.lower()}", method=method)
            def func():
                pass

            exports = getattr(func, LITHIUM_EXPORTS_ATTR)
            self.assertEqual(exports[0].method.value, method)

    def test_parameter_extraction(self):
        """Test that parameters are correctly extracted."""

        @expose_controller(endpoint="/api/params")
        def func_with_params(
            required_param: str,
            optional_param: int = 10,
            another_optional: float = 3.14,
        ) -> dict:
            pass

        exports = getattr(func_with_params, LITHIUM_EXPORTS_ATTR)
        params = exports[0].parameters

        self.assertEqual(len(params), 3)

        # Check required param
        self.assertEqual(params[0].name, "required_param")
        self.assertEqual(params[0].type_name, "str")
        self.assertTrue(params[0].required)

        # Check optional params
        self.assertEqual(params[1].name, "optional_param")
        self.assertFalse(params[1].required)
        self.assertEqual(params[1].default_value, 10)

        self.assertEqual(params[2].name, "another_optional")
        self.assertFalse(params[2].required)
        self.assertEqual(params[2].default_value, 3.14)

    def test_return_type_extraction(self):
        """Test that return type is correctly extracted."""

        @expose_controller(endpoint="/api/return")
        def func_with_return() -> Dict[str, int]:
            return {}

        exports = getattr(func_with_return, LITHIUM_EXPORTS_ATTR)
        # The return type should be extracted
        self.assertIn("Dict", exports[0].return_type)

    def test_docstring_parsing(self):
        """Test that docstrings are parsed for descriptions."""

        @expose_controller(endpoint="/api/doc")
        def documented_func(x: int, y: int) -> int:
            """Add two numbers together.

            :param x: First number to add
            :param y: Second number to add
            :return: Sum of x and y
            """
            return x + y

        exports = getattr(documented_func, LITHIUM_EXPORTS_ATTR)
        export = exports[0]

        # Description should come from docstring
        self.assertIn("Add two numbers", export.description)

        # Parameter descriptions should be extracted
        x_param = next(p for p in export.parameters if p.name == "x")
        self.assertIn("First number", x_param.description)

    def test_tags_and_version(self):
        """Test tags and version metadata."""

        @expose_controller(
            endpoint="/api/tagged",
            tags=["math", "utility"],
            version="2.0.0",
        )
        def tagged_func():
            pass

        exports = getattr(tagged_func, LITHIUM_EXPORTS_ATTR)
        export = exports[0]

        self.assertEqual(export.tags, ["math", "utility"])
        self.assertEqual(export.version, "2.0.0")

    def test_deprecated_flag(self):
        """Test deprecated flag and message."""

        @expose_controller(
            endpoint="/api/deprecated",
            deprecated=True,
            deprecation_message="Use /api/new instead",
        )
        def deprecated_func():
            pass

        exports = getattr(deprecated_func, LITHIUM_EXPORTS_ATTR)
        export = exports[0]

        self.assertTrue(export.deprecated)
        self.assertEqual(export.deprecation_message, "Use /api/new instead")

    def test_function_still_callable(self):
        """Test that decorated function is still callable."""

        @expose_controller(endpoint="/api/callable")
        def add(x: int, y: int) -> int:
            return x + y

        result = add(3, 5)
        self.assertEqual(result, 8)


class TestExposeCommandDecorator(unittest.TestCase):
    """Tests for the @expose_command decorator."""

    def test_basic_decoration(self):
        """Test basic command decoration."""

        @expose_command(
            command_id="test.command",
            description="Test command",
        )
        def test_cmd(data: dict) -> dict:
            return data

        self.assertTrue(hasattr(test_cmd, LITHIUM_EXPORTS_ATTR))
        exports = getattr(test_cmd, LITHIUM_EXPORTS_ATTR)
        self.assertEqual(len(exports), 1)

        export = exports[0]
        self.assertEqual(export.name, "test_cmd")
        self.assertEqual(export.export_type, ExportType.COMMAND)
        self.assertEqual(export.command_id, "test.command")

    def test_priority_and_timeout(self):
        """Test priority and timeout settings."""

        @expose_command(
            command_id="priority.command",
            priority=10,
            timeout_ms=30000,
        )
        def priority_cmd():
            pass

        exports = getattr(priority_cmd, LITHIUM_EXPORTS_ATTR)
        export = exports[0]

        self.assertEqual(export.priority, 10)
        self.assertEqual(export.timeout_ms, 30000)

    def test_default_values(self):
        """Test default values for command."""

        @expose_command(command_id="default.command")
        def default_cmd():
            pass

        exports = getattr(default_cmd, LITHIUM_EXPORTS_ATTR)
        export = exports[0]

        self.assertEqual(export.priority, 0)
        self.assertEqual(export.timeout_ms, 5000)
        self.assertEqual(export.version, "1.0.0")
        self.assertFalse(export.deprecated)


class TestExportInfo(unittest.TestCase):
    """Tests for ExportInfo class."""

    def test_to_dict_controller(self):
        """Test converting controller export to dict."""

        @expose_controller(
            endpoint="/api/test",
            method="POST",
            description="Test",
            tags=["test"],
        )
        def test_func(x: int) -> int:
            return x

        exports = getattr(test_func, LITHIUM_EXPORTS_ATTR)
        export_dict = exports[0].to_dict()

        self.assertEqual(export_dict["name"], "test_func")
        self.assertEqual(export_dict["export_type"], "controller")
        self.assertEqual(export_dict["endpoint"], "/api/test")
        self.assertEqual(export_dict["method"], "POST")
        self.assertIn("parameters", export_dict)
        self.assertIn("tags", export_dict)

    def test_to_dict_command(self):
        """Test converting command export to dict."""

        @expose_command(
            command_id="test.cmd",
            priority=5,
            timeout_ms=10000,
        )
        def test_cmd():
            pass

        exports = getattr(test_cmd, LITHIUM_EXPORTS_ATTR)
        export_dict = exports[0].to_dict()

        self.assertEqual(export_dict["name"], "test_cmd")
        self.assertEqual(export_dict["export_type"], "command")
        self.assertEqual(export_dict["command_id"], "test.cmd")
        self.assertEqual(export_dict["priority"], 5)
        self.assertEqual(export_dict["timeout_ms"], 10000)


class TestModuleExports(unittest.TestCase):
    """Tests for module-level export functions."""

    def setUp(self):
        """Create a test module with exports."""
        # Create a simple module-like object
        import types

        self.test_module = types.ModuleType("test_module")
        self.test_module.__name__ = "test_module"
        self.test_module.__version__ = "1.0.0"

        @expose_controller(endpoint="/api/func1")
        def func1():
            pass

        @expose_command(command_id="cmd1")
        def cmd1():
            pass

        @expose_controller(endpoint="/api/func2")
        @expose_command(command_id="cmd2")
        def multi_export():
            pass

        self.test_module.func1 = func1
        self.test_module.cmd1 = cmd1
        self.test_module.multi_export = multi_export

    def test_get_exports(self):
        """Test getting all exports from a module."""
        exports = get_exports(self.test_module)

        # Should have 4 exports (func1, cmd1, and 2 from multi_export)
        self.assertEqual(len(exports), 4)

        # Check export types
        controllers = [e for e in exports if e.export_type == ExportType.CONTROLLER]
        commands = [e for e in exports if e.export_type == ExportType.COMMAND]

        self.assertEqual(len(controllers), 2)
        self.assertEqual(len(commands), 2)

    def test_get_export_manifest(self):
        """Test getting export manifest."""
        manifest = get_export_manifest(self.test_module)

        self.assertEqual(manifest["module_name"], "test_module")
        self.assertEqual(manifest["version"], "1.0.0")
        self.assertIn("exports", manifest)
        self.assertIn("controllers", manifest["exports"])
        self.assertIn("commands", manifest["exports"])

        self.assertEqual(len(manifest["exports"]["controllers"]), 2)
        self.assertEqual(len(manifest["exports"]["commands"]), 2)

    def test_validate_exports_valid(self):
        """Test validation of valid exports."""
        # Add descriptions to make them valid
        for attr in dir(self.test_module):
            obj = getattr(self.test_module, attr)
            if hasattr(obj, LITHIUM_EXPORTS_ATTR):
                for export in getattr(obj, LITHIUM_EXPORTS_ATTR):
                    export.description = "Valid description"

        errors = validate_exports(self.test_module)
        # Should have no errors now
        self.assertEqual(len(errors), 0)

    def test_validate_exports_missing_description(self):
        """Test validation catches missing descriptions."""
        errors = validate_exports(self.test_module)

        # Should have errors for missing descriptions
        description_errors = [e for e in errors if "description" in e.lower()]
        self.assertGreater(len(description_errors), 0)


class TestManifestLoader(unittest.TestCase):
    """Tests for manifest file loading."""

    def setUp(self):
        """Create temporary directory for test files."""
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        """Clean up temporary directory."""
        import shutil

        shutil.rmtree(self.temp_dir)

    def test_load_manifest_file(self):
        """Test loading a manifest file."""
        manifest = {
            "module_name": "test",
            "version": "1.0.0",
            "exports": {"controllers": [], "commands": []},
        }

        path = Path(self.temp_dir) / "manifest.json"
        with open(path, "w") as f:
            json.dump(manifest, f)

        loaded = load_manifest_file(path)
        self.assertEqual(loaded["module_name"], "test")

    def test_load_manifest_file_not_found(self):
        """Test loading non-existent manifest file."""
        with self.assertRaises(FileNotFoundError):
            load_manifest_file(Path(self.temp_dir) / "nonexistent.json")

    def test_save_manifest_file(self):
        """Test saving a manifest file."""
        manifest = {
            "module_name": "saved",
            "version": "2.0.0",
            "exports": {"controllers": [], "commands": []},
        }

        path = Path(self.temp_dir) / "saved_manifest.json"
        save_manifest_file(manifest, path)

        self.assertTrue(path.exists())

        with open(path) as f:
            loaded = json.load(f)
        self.assertEqual(loaded["module_name"], "saved")

    def test_merge_manifests(self):
        """Test merging multiple manifests."""
        manifest1 = {
            "module_name": "m1",
            "exports": {
                "controllers": [{"name": "ctrl1", "endpoint": "/api/1"}],
                "commands": [],
            },
        }
        manifest2 = {
            "module_name": "m2",
            "exports": {
                "controllers": [{"name": "ctrl2", "endpoint": "/api/2"}],
                "commands": [{"name": "cmd1", "command_id": "cmd.1"}],
            },
        }

        merged = merge_manifests(manifest1, manifest2)

        self.assertEqual(len(merged["exports"]["controllers"]), 2)
        self.assertEqual(len(merged["exports"]["commands"]), 1)

    def test_manifest_loader_directory(self):
        """Test ManifestLoader with directory scanning."""
        # Create manifest files
        manifest1 = {
            "module_name": "module1",
            "exports": {"controllers": [], "commands": []},
        }
        manifest2 = {
            "module_name": "module2",
            "exports": {"controllers": [], "commands": []},
        }

        subdir = Path(self.temp_dir) / "subdir"
        subdir.mkdir()

        with open(Path(self.temp_dir) / "lithium_manifest.json", "w") as f:
            json.dump(manifest1, f)
        with open(subdir / "lithium_manifest.json", "w") as f:
            json.dump(manifest2, f)

        loader = ManifestLoader()
        manifests = loader.load_from_directory(self.temp_dir, recursive=True)

        self.assertEqual(len(manifests), 2)

    def test_create_manifest_template(self):
        """Test creating a manifest template."""
        template = create_manifest_template("my_module", "1.0.0")

        self.assertEqual(template["module_name"], "my_module")
        self.assertEqual(template["version"], "1.0.0")
        self.assertIn("exports", template)
        self.assertIn("controllers", template["exports"])
        self.assertIn("commands", template["exports"])

    def test_validate_manifest_valid(self):
        """Test validating a valid manifest."""
        manifest = {
            "module_name": "valid",
            "exports": {
                "controllers": [
                    {"name": "ctrl", "endpoint": "/api/test", "method": "POST"}
                ],
                "commands": [{"name": "cmd", "command_id": "test.cmd"}],
            },
        }

        errors = validate_manifest(manifest)
        self.assertEqual(len(errors), 0)

    def test_validate_manifest_missing_fields(self):
        """Test validating manifest with missing fields."""
        manifest = {"exports": {"controllers": [{}], "commands": [{}]}}

        errors = validate_manifest(manifest)
        self.assertGreater(len(errors), 0)

    def test_validate_manifest_duplicate_endpoints(self):
        """Test validating manifest with duplicate endpoints."""
        manifest = {
            "module_name": "dup",
            "exports": {
                "controllers": [
                    {"name": "ctrl1", "endpoint": "/api/same"},
                    {"name": "ctrl2", "endpoint": "/api/same"},
                ],
                "commands": [],
            },
        }

        errors = validate_manifest(manifest)
        duplicate_errors = [e for e in errors if "Duplicate" in e]
        self.assertGreater(len(duplicate_errors), 0)


class TestParameterInfo(unittest.TestCase):
    """Tests for ParameterInfo class."""

    def test_to_dict(self):
        """Test converting ParameterInfo to dict."""
        param = ParameterInfo(
            name="test_param",
            type_name="int",
            required=True,
            default_value=None,
            description="A test parameter",
        )

        d = param.to_dict()

        self.assertEqual(d["name"], "test_param")
        self.assertEqual(d["type"], "int")
        self.assertTrue(d["required"])
        self.assertEqual(d["description"], "A test parameter")
        self.assertNotIn("default", d)

    def test_to_dict_with_default(self):
        """Test converting ParameterInfo with default to dict."""
        param = ParameterInfo(
            name="optional",
            type_name="str",
            required=False,
            default_value="hello",
            description="Optional param",
        )

        d = param.to_dict()

        self.assertFalse(d["required"])
        self.assertEqual(d["default"], "hello")


class TestMultipleDecorators(unittest.TestCase):
    """Tests for functions with multiple decorators."""

    def test_controller_and_command(self):
        """Test function decorated as both controller and command."""

        @expose_controller(endpoint="/api/dual")
        @expose_command(command_id="dual.cmd")
        def dual_function(x: int) -> int:
            return x * 2

        exports = getattr(dual_function, LITHIUM_EXPORTS_ATTR)
        self.assertEqual(len(exports), 2)

        types = {e.export_type for e in exports}
        self.assertIn(ExportType.CONTROLLER, types)
        self.assertIn(ExportType.COMMAND, types)

        # Function should still work
        self.assertEqual(dual_function(5), 10)


if __name__ == "__main__":
    unittest.main()
