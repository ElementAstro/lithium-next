"""
Compiler Output Parser Widget

This module provides functionality to parse, analyze, and convert compiler outputs from
various compilers (GCC, Clang, MSVC, CMake) into structured formats like JSON, CSV, or XML.
It supports both command-line usage and programmatic integration through a widget-based
architecture.

Features:
- Multi-compiler support (GCC, Clang, MSVC, CMake)
- Multiple output formats (JSON, CSV, XML)
- Concurrent file processing
- Detailed statistics and filtering capabilities
- Widget-based modular architecture
- Console formatting with colorized output
"""

from typing import Optional, List, Dict, Any, Union, Sequence

from .core import (
    CompilerType,
    OutputFormat,
    MessageSeverity,
    CompilerMessage,
    CompilerOutput,
)

from .parsers import CompilerOutputParser, ParserFactory

from .writers import OutputWriter, WriterFactory

from .widgets import (
    ConsoleFormatterWidget,
    CompilerProcessorWidget,
    CompilerParserWidget,
)

from .utils import parse_args, main_cli


def parse_compiler_output(
    compiler_type: str, output: str, filter_severities: Optional[Sequence[str]] = None
) -> Dict[str, Any]:
    """
    Parse compiler output and return structured data.

    This function is designed to be exported through pybind11 for use in C++ applications.

    Args:
        compiler_type: String identifier for the compiler (gcc, clang, msvc, cmake)
        output: The raw compiler output string to parse
        filter_severities: Optional list of severities to include (error, warning, info)

    Returns:
        Dictionary with parsed compiler output
    """
    widget = CompilerParserWidget()
    # Convert string severities to the expected type
    severities: Optional[List[Union[MessageSeverity, str]]] = None
    if filter_severities is not None:
        severities = [str(s) for s in filter_severities]  # Convert to list of strings

    compiler_output = widget.parse_from_string(compiler_type, output, severities)
    return compiler_output.to_dict()


def parse_compiler_file(
    compiler_type: str,
    file_path: str,
    filter_severities: Optional[Sequence[str]] = None,
) -> Dict[str, Any]:
    """
    Parse compiler output from a file and return structured data.

    This function is designed to be exported through pybind11 for use in C++ applications.

    Args:
        compiler_type: String identifier for the compiler (gcc, clang, msvc, cmake)
        file_path: Path to the file containing compiler output
        filter_severities: Optional list of severities to include (error, warning, info)

    Returns:
        Dictionary with parsed compiler output
    """
    widget = CompilerParserWidget()
    # Convert string severities to the expected type
    severities: Optional[List[Union[MessageSeverity, str]]] = None
    if filter_severities is not None:
        severities = [str(s) for s in filter_severities]  # Convert to list of strings

    compiler_output = widget.parse_from_file(compiler_type, file_path, severities)
    return compiler_output.to_dict()


__all__ = [
    "CompilerType",
    "OutputFormat",
    "MessageSeverity",
    "CompilerMessage",
    "CompilerOutput",
    "CompilerOutputParser",
    "ParserFactory",
    "OutputWriter",
    "WriterFactory",
    "ConsoleFormatterWidget",
    "CompilerProcessorWidget",
    "CompilerParserWidget",
    "parse_args",
    "main_cli",
    "parse_compiler_output",
    "parse_compiler_file",
]
