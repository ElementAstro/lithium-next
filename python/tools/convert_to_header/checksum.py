#!/usr/bin/env python3
"""
Enhanced checksum generation and verification utilities with robust error handling.
"""

from __future__ import annotations
import hashlib
import zlib
from typing import Protocol, runtime_checkable
from functools import lru_cache

from loguru import logger
from .utils import ChecksumAlgo
from .exceptions import ChecksumError


@runtime_checkable
class ChecksumProtocol(Protocol):
    """Protocol defining the interface for checksum implementations."""
    
    def calculate(self, data: bytes) -> str:
        """Calculate checksum for data and return as hex string."""
        ...


class Md5Checksum:
    """MD5 checksum implementation."""
    
    def calculate(self, data: bytes) -> str:
        return hashlib.md5(data).hexdigest()


class Sha1Checksum:
    """SHA-1 checksum implementation."""
    
    def calculate(self, data: bytes) -> str:
        return hashlib.sha1(data).hexdigest()


class Sha256Checksum:
    """SHA-256 checksum implementation."""
    
    def calculate(self, data: bytes) -> str:
        return hashlib.sha256(data).hexdigest()


class Sha512Checksum:
    """SHA-512 checksum implementation."""
    
    def calculate(self, data: bytes) -> str:
        return hashlib.sha512(data).hexdigest()


class Crc32Checksum:
    """CRC32 checksum implementation."""
    
    def calculate(self, data: bytes) -> str:
        return f"{zlib.crc32(data) & 0xFFFFFFFF:08x}"


@lru_cache(maxsize=8)
def _get_checksum_calculator(algorithm: ChecksumAlgo) -> ChecksumProtocol:
    """
    Get a checksum calculator for the specified algorithm.
    
    Args:
        algorithm: Checksum algorithm to use
        
    Returns:
        Checksum calculator implementing ChecksumProtocol
        
    Raises:
        ChecksumError: If algorithm is unsupported
    """
    calculators = {
        "md5": Md5Checksum(),
        "sha1": Sha1Checksum(),
        "sha256": Sha256Checksum(),
        "sha512": Sha512Checksum(),
        "crc32": Crc32Checksum(),
    }
    
    if algorithm not in calculators:
        raise ChecksumError(
            f"Unsupported checksum algorithm: {algorithm}",
            algorithm=algorithm
        )
    
    return calculators[algorithm]


def generate_checksum(data: bytes, algorithm: ChecksumAlgo) -> str:
    """
    Generate a checksum for the given data with enhanced error handling.
    
    Args:
        data: Data to generate checksum for
        algorithm: Checksum algorithm to use
        
    Returns:
        Checksum as hexadecimal string
        
    Raises:
        ChecksumError: If checksum generation fails
    """
    if not isinstance(data, bytes):
        raise ChecksumError(
            f"Data must be bytes, got {type(data).__name__}",
            algorithm=algorithm
        )
    
    try:
        calculator = _get_checksum_calculator(algorithm)
        
        logger.debug(
            f"Generating {algorithm} checksum for {len(data)} bytes",
            extra={"algorithm": algorithm, "data_size": len(data)}
        )
        
        checksum = calculator.calculate(data)
        
        logger.debug(
            f"Generated {algorithm} checksum: {checksum}",
            extra={"algorithm": algorithm, "checksum": checksum, "data_size": len(data)}
        )
        
        return checksum
        
    except Exception as e:
        logger.error(
            f"Checksum generation failed with {algorithm}: {e}",
            extra={"algorithm": algorithm, "data_size": len(data)}
        )
        raise ChecksumError(
            f"Failed to generate {algorithm} checksum: {e}",
            algorithm=algorithm
        ) from e


def verify_checksum(data: bytes, expected_checksum: str, algorithm: ChecksumAlgo) -> bool:
    """
    Verify that the data matches the expected checksum with enhanced error handling.
    
    Args:
        data: Data to verify
        expected_checksum: Expected checksum as hexadecimal string
        algorithm: Checksum algorithm that was used
        
    Returns:
        True if checksums match, False otherwise
        
    Raises:
        ChecksumError: If verification process fails
    """
    if not isinstance(data, bytes):
        raise ChecksumError(
            f"Data must be bytes, got {type(data).__name__}",
            algorithm=algorithm,
            expected_checksum=expected_checksum
        )
    
    if not isinstance(expected_checksum, str):
        raise ChecksumError(
            f"Expected checksum must be string, got {type(expected_checksum).__name__}",
            algorithm=algorithm,
            expected_checksum=str(expected_checksum)
        )
    
    try:
        actual_checksum = generate_checksum(data, algorithm)
        
        # Normalize checksums for comparison (case insensitive)
        expected_normalized = expected_checksum.lower().strip()
        actual_normalized = actual_checksum.lower().strip()
        
        matches = actual_normalized == expected_normalized
        
        logger.debug(
            f"Checksum verification: {'PASS' if matches else 'FAIL'}",
            extra={
                "algorithm": algorithm,
                "expected": expected_normalized,
                "actual": actual_normalized,
                "data_size": len(data)
            }
        )
        
        if not matches:
            logger.warning(
                f"Checksum mismatch: expected {expected_normalized}, got {actual_normalized}"
            )
        
        return matches
        
    except ChecksumError:
        # Re-raise ChecksumError as-is
        raise
    except Exception as e:
        logger.error(
            f"Checksum verification failed: {e}",
            extra={
                "algorithm": algorithm,
                "expected_checksum": expected_checksum,
                "data_size": len(data)
            }
        )
        raise ChecksumError(
            f"Failed to verify {algorithm} checksum: {e}",
            algorithm=algorithm,
            expected_checksum=expected_checksum
        ) from e


def get_checksum_info(algorithm: ChecksumAlgo) -> dict[str, str]:
    """
    Get information about a checksum algorithm.
    
    Args:
        algorithm: Checksum algorithm to get info for
        
    Returns:
        Dictionary with algorithm information
    """
    info = {
        "md5": {
            "name": "MD5",
            "description": "128-bit cryptographic hash (deprecated for security)",
            "output_length": "32 hex chars",
            "security": "Weak (collisions possible)",
            "speed": "Very Fast"
        },
        "sha1": {
            "name": "SHA-1",
            "description": "160-bit cryptographic hash (deprecated for security)",
            "output_length": "40 hex chars",
            "security": "Weak (collisions possible)",
            "speed": "Fast"
        },
        "sha256": {
            "name": "SHA-256",
            "description": "256-bit cryptographic hash (recommended)",
            "output_length": "64 hex chars",
            "security": "Strong",
            "speed": "Medium"
        },
        "sha512": {
            "name": "SHA-512",
            "description": "512-bit cryptographic hash",
            "output_length": "128 hex chars",
            "security": "Very Strong",
            "speed": "Medium"
        },
        "crc32": {
            "name": "CRC-32",
            "description": "32-bit cyclic redundancy check (not cryptographic)",
            "output_length": "8 hex chars",
            "security": "None (error detection only)",
            "speed": "Very Fast"
        }
    }
    
    return info.get(algorithm, {"name": "Unknown", "description": "Unknown algorithm"})


def is_valid_checksum_format(checksum: str, algorithm: ChecksumAlgo) -> bool:
    """
    Check if a checksum string has the correct format for the algorithm.
    
    Args:
        checksum: Checksum string to validate
        algorithm: Algorithm the checksum should be for
        
    Returns:
        True if format is valid, False otherwise
    """
    if not isinstance(checksum, str):
        return False
    
    # Expected lengths for each algorithm (in hex characters)
    expected_lengths = {
        "md5": 32,
        "sha1": 40,
        "sha256": 64,
        "sha512": 128,
        "crc32": 8
    }
    
    if algorithm not in expected_lengths:
        return False
    
    # Check length and that all characters are valid hex
    expected_length = expected_lengths[algorithm]
    return (
        len(checksum) == expected_length and
        all(c in "0123456789abcdefABCDEF" for c in checksum)
    )
