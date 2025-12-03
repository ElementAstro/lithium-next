#!/usr/bin/env python3
"""
Example Python script demonstrating lithium_bridge exports.

This script shows how to use the @expose_controller and @expose_command
decorators to expose Python functions as HTTP endpoints and commands
that can be automatically registered by the Lithium server.
"""

import sys
from pathlib import Path
from typing import Dict, List

# Add lithium_bridge to path if running standalone
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "python"))

from lithium_bridge import expose_controller, expose_command

__version__ = "1.0.0"
__author__ = "Max Qian"


# =============================================================================
# Controller Exports (HTTP Endpoints)
# =============================================================================


@expose_controller(
    endpoint="/api/math/add",
    method="POST",
    description="Add two numbers together",
    tags=["math", "basic"],
)
def add_numbers(a: float, b: float) -> Dict[str, float]:
    """Add two numbers and return the result.

    :param a: First number
    :param b: Second number
    :return: Dictionary containing the result
    """
    return {"a": a, "b": b, "result": a + b, "operation": "add"}


@expose_controller(
    endpoint="/api/math/multiply",
    method="POST",
    description="Multiply two numbers",
    tags=["math", "basic"],
)
def multiply_numbers(x: float, y: float) -> Dict[str, float]:
    """Multiply two numbers and return the result.

    :param x: First number
    :param y: Second number
    :return: Dictionary containing the result
    """
    return {"x": x, "y": y, "result": x * y, "operation": "multiply"}


@expose_controller(
    endpoint="/api/math/calculate",
    method="POST",
    description="Perform various mathematical operations",
    tags=["math", "advanced"],
)
def calculate(
    a: float,
    b: float,
    operation: str = "add",
) -> Dict[str, float]:
    """Perform a mathematical operation on two numbers.

    :param a: First operand
    :param b: Second operand
    :param operation: Operation to perform (add, subtract, multiply, divide)
    :return: Dictionary containing the result
    """
    operations = {
        "add": lambda x, y: x + y,
        "subtract": lambda x, y: x - y,
        "multiply": lambda x, y: x * y,
        "divide": lambda x, y: x / y if y != 0 else float("inf"),
    }

    if operation not in operations:
        return {"error": f"Unknown operation: {operation}"}

    result = operations[operation](a, b)
    return {"a": a, "b": b, "operation": operation, "result": result}


@expose_controller(
    endpoint="/api/text/reverse",
    method="POST",
    description="Reverse a string",
    tags=["text", "utility"],
)
def reverse_string(text: str) -> Dict[str, str]:
    """Reverse the input string.

    :param text: String to reverse
    :return: Dictionary containing original and reversed string
    """
    return {"original": text, "reversed": text[::-1]}


@expose_controller(
    endpoint="/api/list/stats",
    method="POST",
    description="Calculate statistics for a list of numbers",
    tags=["math", "statistics"],
)
def list_statistics(numbers: List[float]) -> Dict[str, float]:
    """Calculate basic statistics for a list of numbers.

    :param numbers: List of numbers to analyze
    :return: Dictionary containing min, max, sum, average, and count
    """
    if not numbers:
        return {"error": "Empty list provided"}

    return {
        "count": len(numbers),
        "sum": sum(numbers),
        "min": min(numbers),
        "max": max(numbers),
        "average": sum(numbers) / len(numbers),
    }


@expose_controller(
    endpoint="/api/info",
    method="GET",
    description="Get module information",
    tags=["info"],
)
def get_module_info() -> Dict[str, str]:
    """Get information about this module.

    :return: Dictionary containing module metadata
    """
    return {
        "name": "example_exports",
        "version": __version__,
        "author": __author__,
        "description": "Example Python script with lithium_bridge exports",
    }


# =============================================================================
# Command Exports (Command Dispatcher)
# =============================================================================


@expose_command(
    command_id="data.process",
    description="Process input data with various transformations",
    priority=5,
    timeout_ms=10000,
    tags=["data", "processing"],
)
def process_data(
    data: Dict,
    transform: str = "none",
) -> Dict:
    """Process input data with optional transformation.

    :param data: Input data dictionary
    :param transform: Transformation to apply (none, uppercase, lowercase)
    :return: Processed data
    """
    result = {"original": data, "transform": transform, "processed": {}}

    for key, value in data.items():
        if isinstance(value, str):
            if transform == "uppercase":
                result["processed"][key] = value.upper()
            elif transform == "lowercase":
                result["processed"][key] = value.lower()
            else:
                result["processed"][key] = value
        else:
            result["processed"][key] = value

    return result


@expose_command(
    command_id="file.analyze",
    description="Analyze file path and return metadata",
    priority=0,
    timeout_ms=5000,
    tags=["file", "utility"],
)
def analyze_file_path(path: str) -> Dict[str, str]:
    """Analyze a file path and return its components.

    :param path: File path to analyze
    :return: Dictionary containing path components
    """
    from pathlib import Path as P

    p = P(path)
    return {
        "path": str(p),
        "name": p.name,
        "stem": p.stem,
        "suffix": p.suffix,
        "parent": str(p.parent),
        "is_absolute": p.is_absolute(),
    }


@expose_command(
    command_id="batch.execute",
    description="Execute a batch of operations",
    priority=10,
    timeout_ms=30000,
    tags=["batch", "processing"],
)
def batch_execute(
    operations: List[Dict],
) -> Dict:
    """Execute a batch of operations and return results.

    :param operations: List of operation dictionaries with 'type' and 'args'
    :return: Dictionary containing results for each operation
    """
    results = []

    for i, op in enumerate(operations):
        op_type = op.get("type", "unknown")
        args = op.get("args", {})

        try:
            if op_type == "add":
                result = args.get("a", 0) + args.get("b", 0)
            elif op_type == "multiply":
                result = args.get("a", 0) * args.get("b", 0)
            elif op_type == "echo":
                result = args.get("message", "")
            else:
                result = {"error": f"Unknown operation type: {op_type}"}

            results.append({"index": i, "type": op_type, "result": result})
        except Exception as e:
            results.append({"index": i, "type": op_type, "error": str(e)})

    return {"total": len(operations), "results": results}


# =============================================================================
# Dual Export (Both Controller and Command)
# =============================================================================


@expose_controller(
    endpoint="/api/echo",
    method="POST",
    description="Echo a message back",
    tags=["utility"],
)
@expose_command(
    command_id="util.echo",
    description="Echo a message back (command version)",
    priority=0,
    timeout_ms=1000,
    tags=["utility"],
)
def echo(message: str, prefix: str = "") -> Dict[str, str]:
    """Echo a message back with optional prefix.

    :param message: Message to echo
    :param prefix: Optional prefix to add
    :return: Dictionary containing the echoed message
    """
    full_message = f"{prefix}{message}" if prefix else message
    return {"message": full_message, "length": len(full_message)}


# =============================================================================
# Deprecated Export Example
# =============================================================================


@expose_controller(
    endpoint="/api/legacy/greet",
    method="GET",
    description="Legacy greeting endpoint",
    deprecated=True,
    deprecation_message="Use /api/echo instead",
    tags=["legacy"],
)
def legacy_greet(name: str = "World") -> str:
    """Legacy greeting function.

    :param name: Name to greet
    :return: Greeting message
    """
    return f"Hello, {name}!"


# =============================================================================
# Main (for testing)
# =============================================================================

if __name__ == "__main__":
    from lithium_bridge import get_export_manifest, validate_exports
    import json

    # Get and print the manifest
    import types

    module = types.ModuleType("example_exports")
    module.__dict__.update(globals())

    manifest = get_export_manifest(module)
    print("Export Manifest:")
    print(json.dumps(manifest, indent=2))

    print("\nValidation Errors:")
    errors = validate_exports(module)
    if errors:
        for error in errors:
            print(f"  - {error}")
    else:
        print("  No errors found!")

    # Test some functions
    print("\nTesting functions:")
    print(f"  add_numbers(3, 5) = {add_numbers(3, 5)}")
    print(f"  calculate(10, 3, 'subtract') = {calculate(10, 3, 'subtract')}")
    print(f"  echo('Hello', 'PREFIX: ') = {echo('Hello', 'PREFIX: ')}")
