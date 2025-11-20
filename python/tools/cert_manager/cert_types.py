#!/usr/bin/env python3
"""
Certificate types and data structures.

This module contains type definitions, enums, dataclasses, and custom exceptions
used throughout the certificate management tool.
"""

import datetime
from dataclasses import dataclass, field
from enum import Enum, auto
from pathlib import Path
from typing import List, Optional

from cryptography import x509
from loguru import logger


# Type definitions for enhanced type safety
class CertificateType(Enum):
    """Types of certificates that can be created."""
    SERVER = auto()
    CLIENT = auto()
    CA = auto()

    @classmethod
    def from_string(cls, value: str) -> 'CertificateType':
        """Convert string value to CertificateType."""
        return {
            "server": cls.SERVER,
            "client": cls.CLIENT,
            "ca": cls.CA
        }.get(value.lower(), cls.SERVER)


@dataclass
class CertificateOptions:
    """Options for certificate generation."""
    hostname: str
    cert_dir: Path
    key_size: int = 2048
    valid_days: int = 365
    san_list: List[str] = field(default_factory=list)
    cert_type: CertificateType = CertificateType.SERVER
    # Additional fields for enhanced certificates
    country: Optional[str] = None
    state: Optional[str] = None
    organization: Optional[str] = None
    organizational_unit: Optional[str] = None
    email: Optional[str] = None


@dataclass
class CertificateResult:
    """Result of certificate generation operations."""
    cert_path: Path
    key_path: Path
    success: bool = True
    message: str = ""


@dataclass
class RevokedCertInfo:
    """Information about a revoked certificate."""
    serial_number: int
    revocation_date: datetime.datetime
    reason: Optional[x509.ReasonFlags] = None


@dataclass
class CertificateDetails:
    """Detailed information about a certificate."""
    subject: str
    issuer: str
    serial_number: int
    not_valid_before: datetime.datetime
    not_valid_after: datetime.datetime
    public_key: str
    extensions: List[x509.Extension]
    is_ca: bool
    fingerprint: str


# Custom exceptions for better error handling
class CertificateError(Exception):
    """Base exception for certificate operations."""
    pass


class KeyGenerationError(CertificateError):
    """Raised when key generation fails."""
    pass


class CertificateGenerationError(CertificateError):
    """Raised when certificate generation fails."""
    pass


class CertificateNotFoundError(CertificateError):
    """Raised when a certificate is not found."""
    pass
