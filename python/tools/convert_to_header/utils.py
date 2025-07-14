#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: utils.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-07-12
Version: 2.2

Description:
------------
Utility functions and type definitions for the convert_to_header package.
Enhanced with modern Python features and performance optimizations.
"""

from __future__ import annotations
from typing import TypedDict, Union, Literal, Protocol, runtime_checkable
from pathlib import Path
from functools import lru_cache
import re

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
    const_qualifier: str
    compression: CompressionType
    checksum: str
    checksum_algorithm: ChecksumAlgo
    original_size: int
    file_size: int
    timestamp: str


@runtime_checkable
class Compressor(Protocol):
    """Protocol for compression implementations."""
    
    def compress(self, data: bytes) -> bytes: ...
    def decompress(self, data: bytes) -> bytes: ...


@runtime_checkable
class Formatter(Protocol):
    """Protocol for data formatting implementations."""
    
    def format_byte(self, value: int) -> str: ...
    def format_array(self, data: bytes) -> list[str]: ...


# Utility functions with caching for performance
@lru_cache(maxsize=128)
def sanitize_identifier(name: str) -> str:
    """
    Sanitize a string to be a valid C/C++ identifier.
    
    Args:
        name: Input string to sanitize
        
    Returns:
        Valid C/C++ identifier
    """
    # Replace non-alphanumeric characters with underscores
    sanitized = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    
    # Ensure it starts with a letter or underscore
    if sanitized and not (sanitized[0].isalpha() or sanitized[0] == '_'):
        sanitized = f"_{sanitized}"
    
    # Handle empty string case
    if not sanitized:
        sanitized = "_generated"
    
    return sanitized


@lru_cache(maxsize=64)
def generate_include_guard(filename: str) -> str:
    """
    Generate an include guard name from a filename.
    
    Args:
        filename: Name of the header file
        
    Returns:
        Include guard macro name
    """
    # Extract stem and create guard name
    stem = Path(filename).stem.upper()
    guard_name = sanitize_identifier(stem)
    return f"{guard_name}_H"


def validate_data_format(fmt: str) -> DataFormat:
    """
    Validate and normalize data format.
    
    Args:
        fmt: Data format string to validate
        
    Returns:
        Validated DataFormat
        
    Raises:
        ValueError: If format is invalid
    """
    valid_formats: set[DataFormat] = {"hex", "bin", "dec", "oct", "char"}
    
    if fmt not in valid_formats:
        raise ValueError(
            f"Invalid data format '{fmt}'. Valid formats: {', '.join(valid_formats)}"
        )
    
    return fmt  # type: ignore


def validate_compression_type(comp: str) -> CompressionType:
    """
    Validate and normalize compression type.
    
    Args:
        comp: Compression type string to validate
        
    Returns:
        Validated CompressionType
        
    Raises:
        ValueError: If compression type is invalid
    """
    valid_types: set[CompressionType] = {"none", "zlib", "gzip", "lzma", "bz2", "base64"}
    
    if comp not in valid_types:
        raise ValueError(
            f"Invalid compression type '{comp}'. Valid types: {', '.join(valid_types)}"
        )
    
    return comp  # type: ignore


def validate_checksum_algorithm(algo: str) -> ChecksumAlgo:
    """
    Validate and normalize checksum algorithm.
    
    Args:
        algo: Checksum algorithm string to validate
        
    Returns:
        Validated ChecksumAlgo
        
    Raises:
        ValueError: If algorithm is invalid
    """
    valid_algos: set[ChecksumAlgo] = {"md5", "sha1", "sha256", "sha512", "crc32"}
    
    if algo not in valid_algos:
        raise ValueError(
            f"Invalid checksum algorithm '{algo}'. Valid algorithms: {', '.join(valid_algos)}"
        )
    
    return algo  # type: ignore


def format_file_size(size_bytes: int) -> str:
    """
    Format file size in human-readable format.
    
    Args:
        size_bytes: Size in bytes
        
    Returns:
        Formatted size string
    """
    if size_bytes == 0:
        return "0 B"
    
    units = ["B", "KB", "MB", "GB", "TB"]
    unit_index = 0
    size = float(size_bytes)
    
    while size >= 1024.0 and unit_index < len(units) - 1:
        size /= 1024.0
        unit_index += 1
    
    if unit_index == 0:
        return f"{int(size)} {units[unit_index]}"
    else:
        return f"{size:.1f} {units[unit_index]}"


def calculate_compression_ratio(original_size: int, compressed_size: int) -> float:
    """
    Calculate compression ratio.
    
    Args:
        original_size: Original data size in bytes
        compressed_size: Compressed data size in bytes
        
    Returns:
        Compression ratio as a percentage (0.0 to 1.0)
    """
    if original_size == 0:
        return 0.0
    
    return compressed_size / original_size


class ByteFormatter:
    """High-performance byte formatter with caching."""
    
    def __init__(self, data_format: DataFormat) -> None:
        self.data_format = data_format
        self._format_cache: dict[int, str] = {}
    
    def format_byte(self, byte_value: int) -> str:
        """
        Format a byte value according to the configured format.
        
        Args:
            byte_value: Byte value (0-255)
            
        Returns:
            Formatted string representation
        """
        if byte_value in self._format_cache:
            return self._format_cache[byte_value]
        
        match self.data_format:
            case "hex":
                result = f"0x{byte_value:02X}"
            case "bin":
                result = f"0b{byte_value:08b}"
            case "dec":
                result = str(byte_value)
            case "oct":
                result = f"0{byte_value:o}"
            case "char":
                if 32 <= byte_value <= 126:  # Printable ASCII
                    char = chr(byte_value)
                    if char in "'\\": 
                        result = f"'\\{char}'"
                    else:
                        result = f"'{char}'"
                else:
                    result = f"0x{byte_value:02X}"  # Non-printable fallback
            case _:
                result = f"0x{byte_value:02X}"  # Default to hex
        
        # Cache the result for future use
        if len(self._format_cache) < 256:  # Limit cache size
            self._format_cache[byte_value] = result
        
        return result
    
    def format_array(self, data: bytes) -> list[str]:
        """
        Format entire byte array efficiently.
        
        Args:
            data: Byte array to format
            
        Returns:
            List of formatted byte strings
        """
        return [self.format_byte(b) for b in data]
