#!/usr/bin/env python3
"""Checksum generation and verification utilities."""
import hashlib
import zlib
from .utils import ChecksumAlgo


def generate_checksum(data: bytes, algorithm: ChecksumAlgo) -> str:
    """Generate a checksum for the given data."""
    match algorithm:
        case "md5":
            return hashlib.md5(data).hexdigest()
        case "sha1":
            return hashlib.sha1(data).hexdigest()
        case "sha256":
            return hashlib.sha256(data).hexdigest()
        case "sha512":
            return hashlib.sha512(data).hexdigest()
        case "crc32":
            return f"{zlib.crc32(data) & 0xFFFFFFFF:08x}"
        case _:
            raise ValueError(f"Unknown checksum algorithm: {algorithm}")


def verify_checksum(data: bytes, expected_checksum: str, algorithm: ChecksumAlgo) -> bool:
    """Verify that the data matches the expected checksum."""
    actual_checksum = generate_checksum(data, algorithm)
    return actual_checksum.lower() == expected_checksum.lower()
