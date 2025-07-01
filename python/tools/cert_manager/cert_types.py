#!/usr/bin/env python3
"""
Certificate types and data structures.
"""

import datetime
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Optional

from cryptography import x509


class CertificateType(str, Enum):
    """Types of certificates that can be created."""
    SERVER = "server"
    CLIENT = "client"
    CA = "ca"

    @classmethod
    def from_string(cls, value: str) -> 'CertificateType':
        return cls(value.lower())


class RevocationReason(str, Enum):
    """CRL revocation reasons."""
    unspecified = "unspecified"
    key_compromise = "keyCompromise"
    ca_compromise = "cACompromise"
    affiliation_changed = "affiliationChanged"
    superseded = "superseded"
    cessation_of_operation = "cessationOfOperation"
    certificate_hold = "certificateHold"
    remove_from_crl = "removeFromCRL"
    privilege_withdrawn = "privilegeWithdrawn"
    aa_compromise = "aACompromise"

    def to_crypto_reason(self) -> x509.ReasonFlags:
        """Converts string reason to cryptography's ReasonFlags enum."""
        return getattr(x509.ReasonFlags, self.value)


@dataclass
class CertificateOptions:
    """Options for certificate or CSR generation."""
    hostname: str
    cert_dir: Path
    key_size: int = 2048
    valid_days: int = 365
    san_list: List[str] = field(default_factory=list)
    cert_type: CertificateType = CertificateType.SERVER
    country: Optional[str] = None
    state: Optional[str] = None
    organization: Optional[str] = None
    organizational_unit: Optional[str] = None
    email: Optional[str] = None


@dataclass
class CertificateResult:
    """Result of certificate generation."""
    cert_path: Path
    key_path: Path


@dataclass
class CSRResult:
    """Result of CSR generation."""
    csr_path: Path
    key_path: Path


@dataclass
class SignOptions:
    """Options for signing a CSR."""
    csr_path: Path
    ca_cert_path: Path
    ca_key_path: Path
    output_dir: Path
    valid_days: int = 365


@dataclass
class RevokeOptions:
    """Options for revoking a certificate."""
    cert_to_revoke_path: Path
    ca_cert_path: Path
    ca_key_path: Path
    crl_path: Path
    reason: RevocationReason


@dataclass
class RevokedCertInfo:
    """Information about a revoked certificate for CRL generation."""
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


# --- Custom Exceptions ---

class CertificateError(Exception):
    """Base exception for certificate operations."""


class KeyGenerationError(CertificateError):
    """Raised when key generation fails."""


class CertificateGenerationError(CertificateError):
    """Raised when certificate generation fails."""


class CertificateNotFoundError(CertificateError, FileNotFoundError):
    """Raised when a certificate file is not found."""

