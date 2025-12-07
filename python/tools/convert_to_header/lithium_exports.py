"""
Lithium export declarations for convert_to_header module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from typing import List

from lithium_bridge import expose_command, expose_controller

from .converter import convert_file, convert_directory


@expose_controller(
    endpoint="/api/convert/file-to-header",
    method="POST",
    description="Convert a file to C++ header",
    tags=["convert", "header"],
    version="1.0.0",
)
def convert_to_header(
    input_file: str,
    output_file: str = None,
    variable_name: str = None,
    namespace: str = None,
    compress: bool = False,
) -> dict:
    """
    Convert a file to a C++ header with embedded data.

    Args:
        input_file: Path to the input file
        output_file: Path for the output header (auto-generated if None)
        variable_name: Name for the data variable
        namespace: C++ namespace to wrap the data
        compress: Whether to compress the data

    Returns:
        Dictionary with conversion result
    """
    try:
        result = convert_file(
            input_path=input_file,
            output_path=output_file,
            variable_name=variable_name,
            namespace=namespace,
            compress=compress,
        )
        return {
            "input_file": input_file,
            "output_file": str(result),
            "success": True,
        }
    except Exception as e:
        return {
            "input_file": input_file,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/convert/dir-to-headers",
    method="POST",
    description="Convert all files in a directory to C++ headers",
    tags=["convert", "header"],
    version="1.0.0",
)
def convert_dir_to_headers(
    input_dir: str,
    output_dir: str = None,
    extensions: List[str] = None,
    recursive: bool = False,
    namespace: str = None,
) -> dict:
    """
    Convert all files in a directory to C++ headers.

    Args:
        input_dir: Path to the input directory
        output_dir: Path for output headers (same as input if None)
        extensions: File extensions to convert (all if None)
        recursive: Whether to process subdirectories
        namespace: C++ namespace to wrap the data

    Returns:
        Dictionary with conversion results
    """
    try:
        results = convert_directory(
            input_dir=input_dir,
            output_dir=output_dir,
            extensions=extensions,
            recursive=recursive,
            namespace=namespace,
        )
        return {
            "input_dir": input_dir,
            "output_dir": output_dir or input_dir,
            "converted_count": len(results),
            "files": [str(r) for r in results],
            "success": True,
        }
    except Exception as e:
        return {
            "input_dir": input_dir,
            "success": False,
            "error": str(e),
        }


@expose_command(
    command_id="convert.file_to_header",
    description="Convert file to header (command)",
    priority=5,
    timeout_ms=30000,
    tags=["convert"],
)
def cmd_convert_to_header(input_file: str, output_file: str = None) -> dict:
    """Convert file to header via command dispatcher."""
    return convert_to_header(input_file, output_file)
