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
This package is released under the MIT License.
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
    "logger",
]
