# Lithium Bridge - Python Script Export Framework

This module provides decorators and utilities for exposing Python functions as HTTP controllers or commands that can be automatically registered by the Lithium server.

## Features

- **Decorator-based exports**: Use `@expose_controller` and `@expose_command` decorators to mark functions for export
- **JSON manifest support**: Define exports in JSON files for non-decorator workflows
- **Automatic parameter extraction**: Function signatures are automatically parsed for parameter info
- **Docstring parsing**: Descriptions are extracted from docstrings
- **Validation**: Built-in validation for export configurations
- **OpenAPI generation**: Automatic OpenAPI spec generation for registered endpoints

## Quick Start

### Using Decorators

```python
from lithium_bridge import expose_controller, expose_command

@expose_controller(
    endpoint="/api/calculate",
    method="POST",
    description="Perform a calculation"
)
def calculate(x: int, y: int, operation: str = "add") -> dict:
    """Perform a mathematical operation.

    :param x: First operand
    :param y: Second operand
    :param operation: Operation to perform
    :return: Result dictionary
    """
    if operation == "add":
        return {"result": x + y}
    elif operation == "multiply":
        return {"result": x * y}
    return {"error": "Unknown operation"}

@expose_command(
    command_id="data.process",
    description="Process data",
    priority=5,
    timeout_ms=10000
)
def process_data(data: dict) -> dict:
    """Process input data.

    :param data: Input data
    :return: Processed data
    """
    return {"processed": True, "data": data}
```

### Using JSON Manifest

Create a `lithium_manifest.json` file:

```json
{
    "module_name": "my_module",
    "version": "1.0.0",
    "exports": {
        "controllers": [
            {
                "name": "calculate",
                "function": "calculate",
                "endpoint": "/api/calculate",
                "method": "POST",
                "description": "Perform a calculation",
                "parameters": [
                    {"name": "x", "type": "int", "required": true},
                    {"name": "y", "type": "int", "required": true},
                    {"name": "operation", "type": "str", "required": false, "default": "add"}
                ],
                "return_type": "dict",
                "tags": ["math"]
            }
        ],
        "commands": [
            {
                "name": "process_data",
                "function": "process_data",
                "command_id": "data.process",
                "description": "Process data",
                "priority": 5,
                "timeout_ms": 10000,
                "parameters": [
                    {"name": "data", "type": "dict", "required": true}
                ],
                "return_type": "dict"
            }
        ]
    }
}
```

## API Reference

### Decorators

#### `@expose_controller`

Expose a function as an HTTP endpoint.

**Parameters:**
- `endpoint` (str): HTTP endpoint path (e.g., "/api/my_function")
- `method` (str): HTTP method (GET, POST, PUT, DELETE, PATCH)
- `description` (str): Human-readable description
- `content_type` (str): Response content type (default: "application/json")
- `tags` (List[str]): Tags for categorization
- `version` (str): API version
- `deprecated` (bool): Whether this endpoint is deprecated
- `deprecation_message` (str): Message explaining deprecation

#### `@expose_command`

Expose a function as a command for the dispatcher.

**Parameters:**
- `command_id` (str): Unique command identifier (e.g., "my.module.command")
- `description` (str): Human-readable description
- `priority` (int): Execution priority (higher = first)
- `timeout_ms` (int): Command timeout in milliseconds
- `tags` (List[str]): Tags for categorization
- `version` (str): Command version
- `deprecated` (bool): Whether this command is deprecated
- `deprecation_message` (str): Message explaining deprecation

### Utility Functions

#### `get_exports(module)`

Get all exports from a module.

```python
from lithium_bridge import get_exports

exports = get_exports(my_module)
for export in exports:
    print(f"{export.name}: {export.export_type}")
```

#### `get_export_manifest(module)`

Get the complete export manifest for a module as a dictionary.

```python
from lithium_bridge import get_export_manifest
import json

manifest = get_export_manifest(my_module)
print(json.dumps(manifest, indent=2))
```

#### `validate_exports(module)`

Validate all exports in a module.

```python
from lithium_bridge import validate_exports

errors = validate_exports(my_module)
if errors:
    for error in errors:
        print(f"Error: {error}")
else:
    print("All exports are valid!")
```

### Manifest Utilities

#### `load_manifest_file(path)`

Load a manifest from a JSON file.

```python
from lithium_bridge.manifest import load_manifest_file

manifest = load_manifest_file("lithium_manifest.json")
```

#### `save_manifest_file(manifest, path)`

Save a manifest to a JSON file.

```python
from lithium_bridge.manifest import save_manifest_file

save_manifest_file(manifest, "output_manifest.json")
```

#### `ManifestLoader`

Utility class for loading manifests from directories.

```python
from lithium_bridge.manifest import ManifestLoader

loader = ManifestLoader()
manifests = loader.load_from_directory("/path/to/scripts", recursive=True)
```

## Integration with C++ Server

The Lithium server automatically discovers and registers exports when loading Python scripts:

1. **Load Script**: Use `PythonScriptManager::loadScript()` or the `/registry/load` HTTP endpoint
2. **Auto-Discovery**: The manager scans for decorated functions and manifest files
3. **Registration**: Controllers are registered as HTTP routes, commands are registered with the dispatcher
4. **Invocation**: Endpoints can be called via HTTP, commands via the command dispatcher

### HTTP Endpoints

After loading a script, the following endpoints are available:

- `POST /registry/load` - Load a script and register exports
- `POST /registry/unload` - Unload a script and unregister exports
- `POST /registry/reload` - Reload a script and re-register exports
- `GET /registry/endpoints` - List all registered endpoints
- `GET /registry/commands` - List all registered commands
- `POST /registry/invoke/endpoint` - Invoke a registered endpoint
- `POST /registry/invoke/command` - Invoke a registered command
- `GET /registry/openapi` - Get OpenAPI specification

## Best Practices

1. **Always provide descriptions**: Both for the export and its parameters
2. **Use type hints**: They're used for parameter validation and documentation
3. **Set appropriate timeouts**: For commands that may take time
4. **Use tags**: For better organization and filtering
5. **Mark deprecated exports**: Instead of removing them immediately
6. **Validate before deployment**: Use `validate_exports()` to catch issues early

## Example

See `example/python_scripts/example_exports.py` for a complete example demonstrating all features.
