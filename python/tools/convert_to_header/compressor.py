#!/usr/bin/env python3
"""
Enhanced compression and decompression utilities with robust error handling.
"""

from __future__ import annotations
import zlib
import lzma
import bz2
import base64
import gzip
from typing import Protocol, runtime_checkable
from functools import lru_cache

from loguru import logger
from .utils import CompressionType
from .exceptions import CompressionError


@runtime_checkable
class CompressorProtocol(Protocol):
    """Protocol defining the interface for compression implementations."""

    def compress(self, data: bytes) -> bytes:
        """Compress data and return compressed bytes."""
        ...

    def decompress(self, data: bytes) -> bytes:
        """Decompress data and return original bytes."""
        ...


class ZlibCompressor:
    """Zlib compression implementation."""

    def __init__(self, level: int = 6) -> None:
        self.level = level

    def compress(self, data: bytes) -> bytes:
        return zlib.compress(data, level=self.level)

    def decompress(self, data: bytes) -> bytes:
        return zlib.decompress(data)


class GzipCompressor:
    """Gzip compression implementation."""

    def __init__(self, level: int = 6) -> None:
        self.level = level

    def compress(self, data: bytes) -> bytes:
        return gzip.compress(data, compresslevel=self.level)

    def decompress(self, data: bytes) -> bytes:
        return gzip.decompress(data)


class LzmaCompressor:
    """LZMA compression implementation."""

    def __init__(self, preset: int = 6) -> None:
        self.preset = preset

    def compress(self, data: bytes) -> bytes:
        return lzma.compress(data, preset=self.preset)

    def decompress(self, data: bytes) -> bytes:
        return lzma.decompress(data)


class Bz2Compressor:
    """Bzip2 compression implementation."""

    def __init__(self, level: int = 9) -> None:
        self.level = level

    def compress(self, data: bytes) -> bytes:
        return bz2.compress(data, compresslevel=self.level)

    def decompress(self, data: bytes) -> bytes:
        return bz2.decompress(data)


class Base64Compressor:
    """Base64 encoding implementation (not compression, but encoding)."""

    def compress(self, data: bytes) -> bytes:
        return base64.b64encode(data)

    def decompress(self, data: bytes) -> bytes:
        return base64.b64decode(data)


class NoopCompressor:
    """No-operation compressor that returns data unchanged."""

    def compress(self, data: bytes) -> bytes:
        return data

    def decompress(self, data: bytes) -> bytes:
        return data


@lru_cache(maxsize=8)
def _get_compressor(compression: CompressionType) -> CompressorProtocol:
    """
    Get a compressor instance for the specified compression type.

    Args:
        compression: Type of compression to use

    Returns:
        Compressor instance implementing CompressorProtocol

    Raises:
        CompressionError: If compression type is unsupported
    """
    compressors = {
        "none": NoopCompressor(),
        "zlib": ZlibCompressor(),
        "gzip": GzipCompressor(),
        "lzma": LzmaCompressor(),
        "bz2": Bz2Compressor(),
        "base64": Base64Compressor(),
    }

    if compression not in compressors:
        raise CompressionError(
            f"Unsupported compression type: {compression}", compression_type=compression
        )

    return compressors[compression]


def compress_data(data: bytes, compression: CompressionType) -> bytes:
    """
    Compress data using the specified algorithm with enhanced error handling.

    Args:
        data: Raw data to compress
        compression: Compression algorithm to use

    Returns:
        Compressed data

    Raises:
        CompressionError: If compression fails
    """
    if not isinstance(data, bytes):
        raise CompressionError(
            f"Data must be bytes, got {type(data).__name__}",
            compression_type=compression,
            data_size=len(data) if hasattr(data, "__len__") else None,
        )

    if not data:
        logger.warning("Compressing empty data")
        return data

    try:
        compressor = _get_compressor(compression)

        logger.debug(
            f"Compressing {len(data)} bytes using {compression}",
            extra={"compression_type": compression, "input_size": len(data)},
        )

        compressed = compressor.compress(data)

        ratio = len(compressed) / len(data) if len(data) > 0 else 0.0
        logger.debug(
            f"Compression complete: {len(compressed)} bytes (ratio: {ratio:.3f})",
            extra={
                "compression_type": compression,
                "input_size": len(data),
                "output_size": len(compressed),
                "compression_ratio": ratio,
            },
        )

        return compressed

    except Exception as e:
        logger.error(
            f"Compression failed with {compression}: {e}",
            extra={"compression_type": compression, "data_size": len(data)},
        )
        raise CompressionError(
            f"Failed to compress data with {compression}: {e}",
            compression_type=compression,
            data_size=len(data),
            original_error=e,
        ) from e


def decompress_data(data: bytes, compression: CompressionType) -> bytes:
    """
    Decompress data using the specified algorithm with enhanced error handling.

    Args:
        data: Compressed data to decompress
        compression: Compression algorithm that was used

    Returns:
        Decompressed data

    Raises:
        CompressionError: If decompression fails
    """
    if not isinstance(data, bytes):
        raise CompressionError(
            f"Data must be bytes, got {type(data).__name__}",
            compression_type=compression,
            data_size=len(data) if hasattr(data, "__len__") else None,
        )

    if not data:
        logger.warning("Decompressing empty data")
        return data

    try:
        compressor = _get_compressor(compression)

        logger.debug(
            f"Decompressing {len(data)} bytes using {compression}",
            extra={"compression_type": compression, "compressed_size": len(data)},
        )

        decompressed = compressor.decompress(data)

        ratio = len(data) / len(decompressed) if len(decompressed) > 0 else 0.0
        logger.debug(
            f"Decompression complete: {len(decompressed)} bytes (ratio: {ratio:.3f})",
            extra={
                "compression_type": compression,
                "compressed_size": len(data),
                "output_size": len(decompressed),
                "compression_ratio": ratio,
            },
        )

        return decompressed

    except Exception as e:
        logger.error(
            f"Decompression failed with {compression}: {e}",
            extra={"compression_type": compression, "data_size": len(data)},
        )
        raise CompressionError(
            f"Failed to decompress data with {compression}: {e}",
            compression_type=compression,
            data_size=len(data),
            original_error=e,
        ) from e


def get_compression_info(compression: CompressionType) -> dict[str, str]:
    """
    Get information about a compression algorithm.

    Args:
        compression: Compression type to get info for

    Returns:
        Dictionary with compression information
    """
    info = {
        "none": {
            "name": "No Compression",
            "description": "Data is stored without compression",
            "typical_ratio": "1.0",
            "speed": "Very Fast",
        },
        "zlib": {
            "name": "Zlib",
            "description": "Standard zlib compression (RFC 1950)",
            "typical_ratio": "0.3-0.7",
            "speed": "Fast",
        },
        "gzip": {
            "name": "Gzip",
            "description": "Gzip compression (RFC 1952)",
            "typical_ratio": "0.3-0.7",
            "speed": "Fast",
        },
        "lzma": {
            "name": "LZMA",
            "description": "LZMA compression (high ratio)",
            "typical_ratio": "0.2-0.5",
            "speed": "Slow",
        },
        "bz2": {
            "name": "Bzip2",
            "description": "Bzip2 compression",
            "typical_ratio": "0.25-0.6",
            "speed": "Medium",
        },
        "base64": {
            "name": "Base64",
            "description": "Base64 encoding (increases size)",
            "typical_ratio": "1.33",
            "speed": "Very Fast",
        },
    }

    return info.get(
        compression, {"name": "Unknown", "description": "Unknown compression type"}
    )
