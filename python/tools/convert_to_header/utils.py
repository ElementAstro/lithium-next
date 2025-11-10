#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: utils.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-06-08
Version: 2.0

Description:
------------
Utility functions and type definitions for the convert_to_header package.
"""

from typing import TypedDict, Optional, List, Dict, Union, Tuple, Any, Callable, Literal
from pathlib import Path
from enum import Enum, auto

from loguru import logger

# Type definitions
PathLike = Union[str, Path]
DataFormat = Literal["hex", "bin", "dec", "oct", "char"]
CommentStyle = Literal["C", "CPP"]
CompressionType = Literal["none", "zlib", "lzma", "bz2", "base64"]
ChecksumAlgo = Literal["md5", "sha1", "sha256", "sha512", "crc32"]


class HeaderInfo(TypedDict, total=False):
    """Type definition for header file information."""

    array_name: str
    size_name: str
    array_type: str
    data_format: str
    compression: str
    original_size: int
    compressed_size: int
    checksum: str
    timestamp: str
    original_filename: str
