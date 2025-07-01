#!/usr/bin/env python3
"""Compression and decompression utilities."""
import zlib
import lzma
import bz2
import base64
import gzip
from .utils import CompressionType
from .exceptions import CompressionError


def compress_data(data: bytes, compression: CompressionType) -> bytes:
    """Compress data using the specified algorithm."""
    try:
        match compression:
            case "none":
                return data
            case "zlib":
                return zlib.compress(data)
            case "gzip":
                return gzip.compress(data)
            case "lzma":
                return lzma.compress(data)
            case "bz2":
                return bz2.compress(data)
            case "base64":
                return base64.b64encode(data)
            case _:
                raise CompressionError(
                    f"Unknown compression type: {compression}")
    except Exception as e:
        raise CompressionError(
            f"Failed to compress data with {compression}: {e}") from e


def decompress_data(data: bytes, compression: CompressionType) -> bytes:
    """Decompress data using the specified algorithm."""
    try:
        match compression:
            case "none":
                return data
            case "zlib":
                return zlib.decompress(data)
            case "gzip":
                return gzip.decompress(data)
            case "lzma":
                return lzma.decompress(data)
            case "bz2":
                return bz2.decompress(data)
            case "base64":
                return base64.b64decode(data)
            case _:
                raise CompressionError(
                    f"Unknown compression type: {compression}")
    except Exception as e:
        raise CompressionError(
            f"Failed to decompress data with {compression}: {e}") from e
