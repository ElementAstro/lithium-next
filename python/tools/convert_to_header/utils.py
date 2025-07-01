#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: utils.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-07-01
Version: 2.1

Description:
------------
Utility functions and type definitions for the convert_to_header package.
"""

from typing import TypedDict, Optional, Union, Literal
from pathlib import Path

# Type definitions
PathLike = Union[str, Path]
DataFormat = Literal["hex", "bin", "dec", "oct", "char"]
CommentStyle = Literal["C", "CPP"]
CompressionType = Literal["none", "zlib", "gzip", "lzma", "bz2", "base64"]
ChecksumAlgo = Literal["md5", "sha1", "sha256", "sha512", "crc32"]


class HeaderInfo(TypedDict, total=False):
    """Type definition for header file information."""
    array_name: str
    array_type: str
    compression: CompressionType
    checksum: str
    checksum_algorithm: ChecksumAlgo
