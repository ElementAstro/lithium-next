"""
Advanced Certificate Management Tool.

This package provides comprehensive functionality for creating, managing,
and validating SSL/TLS certificates with support for multiple interfaces.
"""

from .cert_api import CertificateAPI
from .cert_operations import (
    create_self_signed_cert,
    export_to_pkcs12,
    load_ssl_context,
    get_cert_details,
    view_cert_details,
    check_cert_expiry,
)
from .cert_types import (
    CertificateType,
    CertificateOptions,
    CertificateResult,
    CertificateDetails,
    CertificateError,
)
import sys
from loguru import logger

# Configure default logger
logger.configure(
    handlers=[
        {
            "sink": sys.stderr,
            "format": "<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan> - <level>{message}</level>",
            "level": "INFO",
        }
    ]
)

# Import common components for easy access

__all__ = [
    "CertificateType",
    "CertificateOptions",
    "CertificateResult",
    "CertificateDetails",
    "CertificateError",
    "create_self_signed_cert",
    "export_to_pkcs12",
    "load_ssl_context",
    "get_cert_details",
    "view_cert_details",
    "check_cert_expiry",
    "CertificateAPI",
]
