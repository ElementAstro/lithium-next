"""
Advanced Certificate Management Tool.

This package provides comprehensive functionality for creating, managing,
and validating SSL/TLS certificates with support for multiple interfaces.
"""

from .cert_api import CertificateAPI
from .cert_operations import (
    create_self_signed_cert, export_to_pkcs12, load_ssl_context,
    get_cert_details, view_cert_details, check_cert_expiry, renew_cert,
    generate_crl, create_certificate_chain
)
from .cert_types import (
    CertificateType, CertificateOptions, CertificateResult,
    CertificateDetails, CertificateError
)
import sys
from loguru import logger

# Module metadata
__version__ = "1.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"

# Configure default logger
logger.configure(handlers=[
    {
        "sink": sys.stderr,
        "format": "<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan> - <level>{message}</level>",
        "level": "INFO"
    }
])


def get_tool_info() -> dict:
    """
    Get metadata and information about the cert_manager module.

    Returns:
        Dictionary containing module metadata, capabilities, and available functions.
    """
    return {
        "name": "cert_manager",
        "version": __version__,
        "description": "Advanced Certificate Management Tool for creating, managing, and validating SSL/TLS certificates",
        "author": __author__,
        "license": __license__,
        "supported": True,
        "platform": ["windows", "linux", "macos"],
        "functions": [
            "create_self_signed_cert",
            "export_to_pkcs12",
            "load_ssl_context",
            "get_cert_details",
            "view_cert_details",
            "check_cert_expiry",
            "renew_cert",
            "generate_crl",
            "create_certificate_chain"
        ],
        "requirements": [
            "cryptography",
            "loguru"
        ],
        "capabilities": [
            "Create self-signed SSL/TLS certificates (Server, Client, CA)",
            "Export certificates to PKCS#12 (PFX) format",
            "Load and configure SSL contexts",
            "Extract and validate certificate details",
            "Check certificate expiry status",
            "Renew existing certificates",
            "Generate Certificate Revocation Lists (CRL)",
            "Create certificate chains"
        ],
        "classes": {
            "CertificateAPI": "Stable API interface for C++ pybind11 bindings",
            "CertificateType": "Enum for certificate types (SERVER, CLIENT, CA)",
            "CertificateOptions": "Data class for certificate generation options",
            "CertificateResult": "Data class for certificate generation results",
            "CertificateDetails": "Data class for detailed certificate information"
        }
    }


# Import common components for easy access

__all__ = [
    'CertificateType', 'CertificateOptions', 'CertificateResult',
    'CertificateDetails', 'CertificateError',
    'create_self_signed_cert', 'export_to_pkcs12', 'load_ssl_context',
    'get_cert_details', 'view_cert_details', 'check_cert_expiry',
    'renew_cert', 'generate_crl', 'create_certificate_chain',
    'CertificateAPI', 'get_tool_info'
]
