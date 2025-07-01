"""
Advanced Certificate Management Tool.

This package provides comprehensive functionality for creating, managing,
and validating SSL/TLS certificates with support for multiple interfaces.
"""

import sys
from loguru import logger

# Configure default logger for library use
logger.configure(handlers=[{
    "sink": sys.stderr,
    "format": "<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan> - <level>{message}</level>",
    "level": "INFO",
}])

# Core Functionality
from .cert_operations import (
    create_self_signed_cert,
    create_csr,
    sign_certificate,
    renew_cert,
    export_to_pkcs12_file as export_to_pkcs12, # Alias for backward compatibility
    generate_crl,
    revoke_certificate,
    get_cert_details,
    check_cert_expiry,
    load_ssl_context,
    create_certificate_chain,
)

# API and Types
from .cert_api import CertificateAPI
from .cert_types import (
    CertificateType,
    CertificateOptions,
    CertificateResult,
    CSRResult,
    SignOptions,
    RevokeOptions,
    CertificateDetails,
    RevokedCertInfo,
    CertificateError,
    KeyGenerationError,
    CertificateGenerationError,
    CertificateNotFoundError,
)

__all__ = [
    # Enums & Dataclasses
    'CertificateType',
    'CertificateOptions',
    'CertificateResult',
    'CSRResult',
    'SignOptions',
    'RevokeOptions',
    'CertificateDetails',
    'RevokedCertInfo',
    # Exceptions
    'CertificateError',
    'KeyGenerationError',
    'CertificateGenerationError',
    'CertificateNotFoundError',
    # Core Functions
    'create_self_signed_cert',
    'create_csr',
    'sign_certificate',
    'renew_cert',
    'export_to_pkcs12',
    'generate_crl',
    'revoke_certificate',
    'get_cert_details',
    'check_cert_expiry',
    'load_ssl_context',
    'create_certificate_chain',
    # API Class
    'CertificateAPI',
]

