#!/usr/bin/env python3
"""
Enhanced certificate types and data structures with modern Python features.

This module provides type-safe, performance-optimized data models using the latest
Python features including Pydantic v2, StrEnum, and comprehensive validation.
"""

from __future__ import annotations

import datetime
import time
from enum import StrEnum
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, TypeAlias

from cryptography import x509
from loguru import logger
from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator

# Type aliases for improved type hinting
PathLike: TypeAlias = str | Path
SerialNumber: TypeAlias = int


class CertificateType(StrEnum):
    """
    Types of certificates that can be created using StrEnum for better serialization.
    
    Each type represents a different use case for X.509 certificates with specific
    extensions and key usage patterns.
    """
    SERVER = "server"      # TLS server authentication certificates
    CLIENT = "client"      # TLS client authentication certificates  
    CA = "ca"              # Certificate Authority certificates
    INTERMEDIATE = "intermediate"  # Intermediate CA certificates
    CODE_SIGNING = "code_signing"  # Code signing certificates
    EMAIL = "email"        # S/MIME email certificates
    
    def __str__(self) -> str:
        """Return human-readable string representation."""
        descriptions = {
            self.SERVER: "TLS Server Authentication",
            self.CLIENT: "TLS Client Authentication",
            self.CA: "Certificate Authority",
            self.INTERMEDIATE: "Intermediate Certificate Authority",
            self.CODE_SIGNING: "Code Signing",
            self.EMAIL: "S/MIME Email"
        }
        return descriptions.get(self, self.value)
    
    @property
    def is_ca_type(self) -> bool:
        """Check if this certificate type is a Certificate Authority."""
        return self in {self.CA, self.INTERMEDIATE}
    
    @property
    def requires_key_usage(self) -> Set[str]:
        """Get required key usage extensions for this certificate type."""
        key_usage_map = {
            self.SERVER: {"digital_signature", "key_encipherment"},
            self.CLIENT: {"digital_signature", "key_agreement"},
            self.CA: {"key_cert_sign", "crl_sign"},
            self.INTERMEDIATE: {"key_cert_sign", "crl_sign"},
            self.CODE_SIGNING: {"digital_signature"},
            self.EMAIL: {"digital_signature", "key_encipherment"}
        }
        return key_usage_map.get(self, set())
    
    @classmethod
    def from_string(cls, value: str) -> CertificateType:
        """Create certificate type from string with case-insensitive matching."""
        try:
            return cls(value.lower())
        except ValueError:
            valid_types = ", ".join(cert_type.value for cert_type in cls)
            raise ValueError(
                f"Invalid certificate type: {value}. Valid types: {valid_types}"
            ) from None


class RevocationReason(StrEnum):
    """CRL revocation reasons using StrEnum for better serialization."""
    UNSPECIFIED = "unspecified"
    KEY_COMPROMISE = "keyCompromise"
    CA_COMPROMISE = "cACompromise" 
    AFFILIATION_CHANGED = "affiliationChanged"
    SUPERSEDED = "superseded"
    CESSATION_OF_OPERATION = "cessationOfOperation"
    CERTIFICATE_HOLD = "certificateHold"
    REMOVE_FROM_CRL = "removeFromCRL"
    PRIVILEGE_WITHDRAWN = "privilegeWithdrawn"
    AA_COMPROMISE = "aACompromise"
    
    def __str__(self) -> str:
        """Return human-readable string representation."""
        descriptions = {
            self.UNSPECIFIED: "Unspecified",
            self.KEY_COMPROMISE: "Key Compromise",
            self.CA_COMPROMISE: "CA Compromise",
            self.AFFILIATION_CHANGED: "Affiliation Changed",
            self.SUPERSEDED: "Superseded",
            self.CESSATION_OF_OPERATION: "Cessation of Operation",
            self.CERTIFICATE_HOLD: "Certificate Hold",
            self.REMOVE_FROM_CRL: "Remove from CRL",
            self.PRIVILEGE_WITHDRAWN: "Privilege Withdrawn",
            self.AA_COMPROMISE: "Attribute Authority Compromise"
        }
        return descriptions.get(self, self.value)
    
    def to_crypto_reason(self) -> x509.ReasonFlags:
        """Convert string reason to cryptography's ReasonFlags enum."""
        reason_map = {
            self.UNSPECIFIED: x509.ReasonFlags.unspecified,
            self.KEY_COMPROMISE: x509.ReasonFlags.key_compromise,
            self.CA_COMPROMISE: x509.ReasonFlags.ca_compromise,
            self.AFFILIATION_CHANGED: x509.ReasonFlags.affiliation_changed,
            self.SUPERSEDED: x509.ReasonFlags.superseded,
            self.CESSATION_OF_OPERATION: x509.ReasonFlags.cessation_of_operation,
            self.CERTIFICATE_HOLD: x509.ReasonFlags.certificate_hold,
            self.REMOVE_FROM_CRL: x509.ReasonFlags.remove_from_crl,
            self.PRIVILEGE_WITHDRAWN: x509.ReasonFlags.privilege_withdrawn,
            self.AA_COMPROMISE: x509.ReasonFlags.aa_compromise
        }
        return reason_map[self]


class KeySize(StrEnum):
    """Supported RSA key sizes using StrEnum."""
    SIZE_1024 = "1024"   # Not recommended for new certificates
    SIZE_2048 = "2048"   # Standard size
    SIZE_3072 = "3072"   # Higher security
    SIZE_4096 = "4096"   # Maximum security
    
    @property
    def bits(self) -> int:
        """Get key size as integer."""
        return int(self.value)
    
    @property
    def is_secure(self) -> bool:
        """Check if key size meets current security standards."""
        return self.bits >= 2048
    
    @property
    def security_level(self) -> str:
        """Get security level description."""
        if self.bits < 2048:
            return "Weak (not recommended)"
        elif self.bits == 2048:
            return "Standard"
        elif self.bits == 3072:
            return "High"
        else:
            return "Maximum"


class HashAlgorithm(StrEnum):
    """Supported hash algorithms using StrEnum."""
    SHA256 = "sha256"
    SHA384 = "sha384"
    SHA512 = "sha512"
    SHA3_256 = "sha3_256"
    SHA3_384 = "sha3_384"
    SHA3_512 = "sha3_512"
    
    @property
    def is_secure(self) -> bool:
        """Check if hash algorithm meets current security standards."""
        # All listed algorithms are considered secure
        return True
    
    @property
    def bit_length(self) -> int:
        """Get hash algorithm bit length."""
        bit_lengths = {
            self.SHA256: 256,
            self.SHA384: 384,
            self.SHA512: 512,
            self.SHA3_256: 256,
            self.SHA3_384: 384,
            self.SHA3_512: 512
        }
        return bit_lengths[self]


class CertificateOptions(BaseModel):
    """
    Enhanced certificate generation options with comprehensive validation using Pydantic v2.
    """
    
    model_config = ConfigDict(
        extra='forbid',
        validate_assignment=True,
        str_strip_whitespace=True,
        use_enum_values=True
    )
    
    hostname: str = Field(
        description="Primary hostname for the certificate",
        min_length=1,
        max_length=253
    )
    cert_dir: Path = Field(
        description="Directory to store certificate files"
    )
    key_size: KeySize = Field(
        default=KeySize.SIZE_2048,
        description="RSA key size in bits"
    )
    hash_algorithm: HashAlgorithm = Field(
        default=HashAlgorithm.SHA256,
        description="Hash algorithm for certificate signing"
    )
    valid_days: int = Field(
        default=365,
        ge=1,
        le=7300,  # ~20 years maximum
        description="Certificate validity period in days"
    )
    san_list: List[str] = Field(
        default_factory=list,
        description="Subject Alternative Names"
    )
    cert_type: CertificateType = Field(
        default=CertificateType.SERVER,
        description="Type of certificate to generate"
    )
    
    # Distinguished Name fields
    country: Optional[str] = Field(
        default=None,
        min_length=2,
        max_length=2,
        description="Two-letter country code (ISO 3166-1 alpha-2)"
    )
    state: Optional[str] = Field(
        default=None,
        min_length=1,
        max_length=128,
        description="State or province name"
    )
    locality: Optional[str] = Field(
        default=None,
        min_length=1,
        max_length=128,
        description="Locality or city name"
    )
    organization: Optional[str] = Field(
        default=None,
        min_length=1,
        max_length=128,
        description="Organization name"
    )
    organizational_unit: Optional[str] = Field(
        default=None,
        min_length=1,
        max_length=128,
        description="Organizational unit name"
    )
    email: Optional[str] = Field(
        default=None,
        pattern=r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$',
        description="Email address"
    )
    
    # Advanced options
    path_length: Optional[int] = Field(
        default=None,
        ge=0,
        le=10,
        description="Path length constraint for CA certificates"
    )
    
    @field_validator('hostname')
    @classmethod
    def validate_hostname(cls, v: str) -> str:
        """Validate hostname format."""
        import re
        
        # Basic hostname validation
        hostname_pattern = re.compile(
            r'^(?!-)[A-Za-z0-9-]{1,63}(?<!-)(?:\.(?!-)[A-Za-z0-9-]{1,63}(?<!-))*$'
        )
        
        if not hostname_pattern.match(v):
            raise ValueError(f"Invalid hostname format: {v}")
        
        # Check for valid TLD if it looks like a FQDN
        if '.' in v:
            parts = v.split('.')
            if len(parts[-1]) < 2:
                logger.warning(f"Hostname '{v}' has a short TLD, which may not be valid")
        
        return v
    
    @field_validator('san_list')
    @classmethod
    def validate_san_list(cls, v: List[str]) -> List[str]:
        """Validate Subject Alternative Names."""
        import ipaddress
        import re
        
        validated_sans = []
        
        for san in v:
            san = san.strip()
            if not san:
                continue
                
            # Check if it's an IP address
            try:
                ipaddress.ip_address(san)
                validated_sans.append(san)
                continue
            except ValueError:
                pass
            
            # Check if it's a valid hostname/domain
            hostname_pattern = re.compile(
                r'^(?!-)[A-Za-z0-9-*]{1,63}(?<!-)(?:\.(?!-)[A-Za-z0-9-*]{1,63}(?<!-))*$'
            )
            
            if hostname_pattern.match(san):
                validated_sans.append(san)
            else:
                logger.warning(f"Skipping invalid SAN entry: {san}")
        
        return validated_sans
    
    @field_validator('country')
    @classmethod
    def validate_country_code(cls, v: Optional[str]) -> Optional[str]:
        """Validate country code format."""
        if v is None:
            return v
        
        v = v.upper()
        if len(v) != 2 or not v.isalpha():
            raise ValueError("Country code must be exactly 2 letters (ISO 3166-1 alpha-2)")
        
        return v
    
    @model_validator(mode='after')
    def validate_certificate_options(self) -> CertificateOptions:
        """Validate certificate option combinations."""
        # CA certificates should have path length constraint
        if self.cert_type.is_ca_type and self.path_length is None:
            self.path_length = 0 if self.cert_type == CertificateType.INTERMEDIATE else None
        
        # Non-CA certificates should not have path length constraint
        if not self.cert_type.is_ca_type and self.path_length is not None:
            raise ValueError("Path length constraint is only valid for CA certificates")
        
        # Warn about weak key sizes
        if not self.key_size.is_secure:
            logger.warning(
                f"Key size {self.key_size.value} is below recommended minimum (2048 bits)"
            )
        
        # Warn about very long validity periods
        if self.valid_days > 825:  # More than ~2.3 years
            logger.warning(
                f"Validity period of {self.valid_days} days exceeds recommended maximum (825 days)"
            )
        
        return self


class CertificateResult(BaseModel):
    """Enhanced result of certificate generation with validation."""
    
    model_config = ConfigDict(
        extra='forbid',
        validate_assignment=True
    )
    
    cert_path: Path = Field(description="Path to generated certificate file")
    key_path: Path = Field(description="Path to generated private key file")
    serial_number: Optional[SerialNumber] = Field(
        default=None,
        description="Certificate serial number"
    )
    fingerprint: Optional[str] = Field(
        default=None,
        description="Certificate fingerprint (SHA256)"
    )
    not_valid_before: Optional[datetime.datetime] = Field(
        default=None,
        description="Certificate validity start date"
    )
    not_valid_after: Optional[datetime.datetime] = Field(
        default=None,
        description="Certificate validity end date"
    )
    
    @property
    def is_valid_now(self) -> bool:
        """Check if certificate is currently valid."""
        if not self.not_valid_before or not self.not_valid_after:
            return False
        
        now = datetime.datetime.now(datetime.timezone.utc)
        return self.not_valid_before <= now <= self.not_valid_after
    
    @property
    def days_until_expiry(self) -> Optional[int]:
        """Get number of days until certificate expires."""
        if not self.not_valid_after:
            return None
        
        now = datetime.datetime.now(datetime.timezone.utc)
        delta = self.not_valid_after - now
        return delta.days


class CSRResult(BaseModel):
    """Enhanced result of CSR generation with validation."""
    
    model_config = ConfigDict(
        extra='forbid',
        validate_assignment=True
    )
    
    csr_path: Path = Field(description="Path to generated CSR file")
    key_path: Path = Field(description="Path to generated private key file")
    subject: Optional[str] = Field(
        default=None,
        description="CSR subject distinguished name"
    )
    public_key_info: Optional[str] = Field(
        default=None,
        description="Public key information"
    )


class SignOptions(BaseModel):
    """Enhanced options for signing a CSR with validation."""
    
    model_config = ConfigDict(
        extra='forbid',
        validate_assignment=True
    )
    
    csr_path: Path = Field(description="Path to CSR file to sign")
    ca_cert_path: Path = Field(description="Path to CA certificate file")
    ca_key_path: Path = Field(description="Path to CA private key file")
    output_dir: Path = Field(description="Directory for output certificate")
    valid_days: int = Field(
        default=365,
        ge=1,
        le=7300,
        description="Certificate validity period in days"
    )
    hash_algorithm: HashAlgorithm = Field(
        default=HashAlgorithm.SHA256,
        description="Hash algorithm for signing"
    )


class RevokeOptions(BaseModel):
    """Enhanced options for revoking a certificate with validation."""
    
    model_config = ConfigDict(
        extra='forbid',
        validate_assignment=True
    )
    
    cert_to_revoke_path: Path = Field(description="Path to certificate to revoke")
    ca_cert_path: Path = Field(description="Path to CA certificate file")
    ca_key_path: Path = Field(description="Path to CA private key file")
    crl_path: Path = Field(description="Path to CRL file")
    reason: RevocationReason = Field(
        default=RevocationReason.UNSPECIFIED,
        description="Reason for revocation"
    )
    revocation_date: Optional[datetime.datetime] = Field(
        default=None,
        description="Revocation date (defaults to current time)"
    )
    
    @model_validator(mode='after')
    def set_default_revocation_date(self) -> RevokeOptions:
        """Set default revocation date if not provided."""
        if self.revocation_date is None:
            self.revocation_date = datetime.datetime.now(datetime.timezone.utc)
        return self


class RevokedCertInfo(BaseModel):
    """Enhanced information about a revoked certificate for CRL generation."""
    
    model_config = ConfigDict(
        extra='forbid',
        validate_assignment=True
    )
    
    serial_number: SerialNumber = Field(description="Certificate serial number")
    revocation_date: datetime.datetime = Field(description="When certificate was revoked")
    reason: Optional[x509.ReasonFlags] = Field(
        default=None,
        description="Revocation reason"
    )
    invalidity_date: Optional[datetime.datetime] = Field(
        default=None,
        description="Date when certificate became invalid"
    )


class CertificateDetails(BaseModel):
    """Enhanced detailed information about a certificate."""
    
    model_config = ConfigDict(
        extra='forbid',
        validate_assignment=True
    )
    
    subject: str = Field(description="Certificate subject DN")
    issuer: str = Field(description="Certificate issuer DN")
    serial_number: SerialNumber = Field(description="Certificate serial number")
    not_valid_before: datetime.datetime = Field(description="Validity start date")
    not_valid_after: datetime.datetime = Field(description="Validity end date")
    public_key_info: str = Field(description="Public key information")
    signature_algorithm: str = Field(description="Signature algorithm used")
    version: int = Field(description="Certificate version")
    is_ca: bool = Field(description="Whether certificate is a CA")
    fingerprint_sha256: str = Field(description="SHA256 fingerprint")
    fingerprint_sha1: str = Field(description="SHA1 fingerprint")
    key_usage: List[str] = Field(
        default_factory=list,
        description="Key usage extensions"
    )
    extended_key_usage: List[str] = Field(
        default_factory=list,
        description="Extended key usage extensions"
    )
    subject_alt_names: List[str] = Field(
        default_factory=list,
        description="Subject alternative names"
    )
    
    @property
    def is_valid_now(self) -> bool:
        """Check if certificate is currently valid."""
        now = datetime.datetime.now(datetime.timezone.utc)
        return self.not_valid_before <= now <= self.not_valid_after
    
    @property
    def days_until_expiry(self) -> int:
        """Get number of days until certificate expires."""
        now = datetime.datetime.now(datetime.timezone.utc)
        delta = self.not_valid_after - now
        return delta.days
    
    @property
    def is_expired(self) -> bool:
        """Check if certificate has expired."""
        return self.days_until_expiry < 0
    
    @property
    def expires_soon(self, days_threshold: int = 30) -> bool:
        """Check if certificate expires within threshold days."""
        return 0 <= self.days_until_expiry <= days_threshold


# Enhanced custom exceptions with error context
class CertificateException(Exception):
    """Base exception for certificate operations."""
    
    def __init__(self, message: str, *, error_code: Optional[str] = None, **kwargs: Any):
        super().__init__(message)
        self.error_code = error_code
        self.context = kwargs
        
        # Log the exception with context
        logger.error(
            f"CertificateException: {message}",
            extra={
                "error_code": error_code,
                "context": kwargs
            }
        )


class CertificateError(CertificateException):
    """General certificate operation error."""
    pass


class KeyGenerationError(CertificateException):
    """Raised when key generation fails."""
    pass


class CertificateGenerationError(CertificateException):
    """Raised when certificate generation fails."""
    pass


class CertificateNotFoundError(CertificateException, FileNotFoundError):
    """Raised when a certificate file is not found."""
    pass


class CertificateValidationError(CertificateException):
    """Raised when certificate validation fails."""
    pass


class CertificateParsingError(CertificateException):
    """Raised when certificate parsing fails."""
    pass


class CSRGenerationError(CertificateException):
    """Raised when CSR generation fails."""
    pass


class SigningError(CertificateException):
    """Raised when certificate signing fails."""
    pass


class RevocationError(CertificateException):
    """Raised when certificate revocation fails."""
    pass
