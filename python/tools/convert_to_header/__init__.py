#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: __init__.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-06-08
Version: 2.0

Description:
------------
This Python package provides functionality to convert binary files into C/C++ header files
containing array data, and vice versa, with extensive customization options and features.

The package supports two primary operations:
    1. Converting binary files to C/C++ header files with array data
    2. Converting C/C++ header files back to binary files

License:
--------
This package is released under the GPL-3.0-or-later License.
"""

from .converter import Converter
from .options import (
    ConversionOptions,
    ConversionMode,
    DataFormat,
    CommentStyle,
    CompressionType,
    ChecksumAlgo,
)
from .exceptions import (
    ConversionError,
    FileFormatError,
    CompressionError,
    ChecksumError,
)
from .utils import HeaderInfo
from .converter import convert_to_header, convert_to_file, get_header_info
from .pybind_adapter import ConvertToHeaderPyBindAdapter

# Module metadata
__version__ = "2.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"

# Configure loguru logger
from loguru import logger
import sys

# Remove default handler and add custom one
logger.remove()
logger.add(
    sys.stderr,
    format="{time:YYYY-MM-DD HH:mm:ss} | {level: <8} | {message}",
    level="INFO",
)


def get_tool_info() -> dict:
    """
    Return metadata about this tool for discovery by PythonWrapper.

    This function follows the Lithium-Next Python tool discovery convention,
    allowing the C++ PythonWrapper to introspect and catalog this module's
    capabilities.

    Returns:
        Dict containing tool metadata including name, version, description,
        available functions, requirements, and platform compatibility.
    """
    return {
        "name": "convert_to_header",
        "version": __version__,
        "description": "Convert binary files to C/C++ headers and vice versa with customizable options",
        "author": __author__,
        "license": __license__,
        "supported": True,
        "platform": ["windows", "linux", "macos"],
        "functions": [
            "convert_to_header",
            "convert_to_file",
            "get_header_info",
        ],
        "requirements": ["loguru"],
        "capabilities": [
            "binary_to_header_conversion",
            "header_to_binary_conversion",
            "data_compression",
            "checksum_verification",
            "header_inspection",
            "multi_file_splitting",
        ],
        "classes": {
            "Converter": "Main binary/header conversion interface",
            "ConversionOptions": "Configuration options for conversions",
            "ConvertToHeaderPyBindAdapter": "Simplified pybind11 interface",
        },
    }


# Public API
__all__ = [
    "Converter",
    "ConversionOptions",
    "ConversionMode",
    "HeaderInfo",
    "ConversionError",
    "FileFormatError",
    "CompressionError",
    "ChecksumError",
    "convert_to_header",
    "convert_to_file",
    "get_header_info",
    "get_tool_info",
    "ConvertToHeaderPyBindAdapter",
    "logger",
]
