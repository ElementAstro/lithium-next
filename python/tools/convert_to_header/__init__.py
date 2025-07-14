#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: __init__.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-07-12
Version: 2.2

Description:
------------
This Python package provides functionality to convert binary files into C/C++ header files
containing array data, and vice versa, with extensive customization options and features.
Enhanced with modern Python features and robust error handling.
"""

# Configure loguru logger
from .utils import HeaderInfo, DataFormat, CommentStyle, CompressionType, ChecksumAlgo
from .exceptions import ConversionError, FileFormatError, CompressionError, ChecksumError, ValidationError
from .options import ConversionOptions
from .converter import Converter, convert_to_header, convert_to_file, get_header_info
from loguru import logger
import sys

logger.remove()
logger.add(
    sys.stderr, 
    format="{time:YYYY-MM-DD HH:mm:ss} | {level: <8} | {name}:{function}:{line} | {message}", 
    level="INFO",
    filter=lambda record: record["name"].startswith("convert_to_header")
)

# Public API

__all__ = [
    # Core
    'Converter',
    'convert_to_header',
    'convert_to_file',
    'get_header_info',
    # Options & Types
    'ConversionOptions',
    'HeaderInfo',
    'DataFormat',
    'CommentStyle',
    'CompressionType',
    'ChecksumAlgo',
    # Exceptions
    'ConversionError',
    'FileFormatError',
    'CompressionError',
    'ChecksumError',
    'ValidationError',
    # Logger
    'logger',
]

__version__ = "2.2.0"
__author__ = "Max Qian <astro_air@126.com>"
