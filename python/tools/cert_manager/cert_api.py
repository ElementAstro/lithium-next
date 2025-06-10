#!/usr/bin/env python3
"""
Certificate management API.

This module provides programmatic APIs for certificate management,
including a stable interface for pybind11 C++ bindings.
"""

from pathlib import Path
from typing import Dict, List, Optional

from loguru import logger

from .cert_types import CertificateOptions, CertificateType
from .cert_operations import create_self_signed_cert, export_to_pkcs12


class CertificateAPI:
    """
    API class for integrating with C++ via pybind11.

    This class provides a stable interface that can be used with pybind11
    to create C++ bindings for the certificate functionality.
    """

    @staticmethod
    def create_certificate(
        hostname: str,
        cert_dir: str,
        key_size: int = 2048,
        valid_days: int = 365,
        san_list: Optional[List[str]] = None,
        cert_type: str = "server",
        country: Optional[str] = None,
        state: Optional[str] = None,
        organization: Optional[str] = None,
        organizational_unit: Optional[str] = None,
        email: Optional[str] = None
    ) -> Dict[str, str]:
        """Create a self-signed certificate and return paths."""
        options = CertificateOptions(
            hostname=hostname,
            cert_dir=Path(cert_dir),
            key_size=key_size,
            valid_days=valid_days,
            san_list=san_list or [],
            cert_type=CertificateType.from_string(cert_type),
            country=country,
            state=state,
            organization=organization,
            organizational_unit=organizational_unit,
            email=email
        )

        try:
            result = create_self_signed_cert(options)
            return {
                "cert_path": str(result.cert_path),
                "key_path": str(result.key_path),
                "success": "true"  # Fixed: use string "true"
            }
        except Exception as e:
            logger.exception(f"Error creating certificate: {str(e)}")
            return {
                "cert_path": "",
                "key_path": "",
                "success": "false",  # Fixed: use string "false"
                "error": str(e)
            }

    @staticmethod
    def export_to_pkcs12(
        cert_path: str,
        key_path: str,
        password: str,
        export_path: Optional[str] = None
    ) -> Dict[str, str]:
        """Export certificate to PKCS#12 format."""
        try:
            result = export_to_pkcs12(
                Path(cert_path),
                Path(key_path),
                password,
                Path(export_path) if export_path else None
            )
            return {
                "pfx_path": str(result),
                "success": "true"  # Fixed: use string "true"
            }
        except Exception as e:
            logger.exception(f"Error exporting certificate: {str(e)}")
            return {
                "pfx_path": "",
                "success": "false",  # Fixed: use string "false"
                "error": str(e)
            }
