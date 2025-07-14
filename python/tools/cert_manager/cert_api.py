#!/usr/bin/env python3
"""
Certificate management API.

This module provides programmatic APIs for certificate management,
including a stable interface for pybind11 C++ bindings.
"""

from pathlib import Path
from typing import Any, Dict, List, Optional

from loguru import logger

from .cert_operations import (
    create_self_signed_cert,
    create_csr,
    sign_certificate,
    export_to_pkcs12_file,
)
from .cert_types import CertificateOptions, CertificateType, SignOptions


class CertificateAPI:
    """
    API class for integrating with C++ via pybind11.

    This class provides a stable interface that can be used with pybind11
    to create C++ bindings for the certificate functionality.
    """

    @staticmethod
    def _build_options(params: Dict[str, Any]) -> CertificateOptions:
        """Helper to build CertificateOptions from a dictionary."""
        params["cert_dir"] = Path(params["cert_dir"])
        if "cert_type" in params and isinstance(params["cert_type"], str):
            params["cert_type"] = CertificateType.from_string(params["cert_type"])
        return CertificateOptions(**params)

    @staticmethod
    def _handle_exception(e: Exception, operation: str) -> Dict[str, Any]:
        """Centralized exception handling for API methods."""
        logger.exception(f"Error during {operation}: {e}")
        return {"success": False, "error": str(e)}

    def create_certificate(self, **kwargs: Any) -> Dict[str, Any]:
        """Create a self-signed certificate and return paths."""
        try:
            options = self._build_options(kwargs)
            result = create_self_signed_cert(options)
            return {
                "cert_path": str(result.cert_path),
                "key_path": str(result.key_path),
                "success": True,
            }
        except Exception as e:
            return self._handle_exception(e, "certificate creation")

    def create_csr(self, **kwargs: Any) -> Dict[str, Any]:
        """Create a Certificate Signing Request."""
        try:
            options = self._build_options(kwargs)
            result = create_csr(options)
            return {
                "csr_path": str(result.csr_path),
                "key_path": str(result.key_path),
                "success": True,
            }
        except Exception as e:
            return self._handle_exception(e, "CSR creation")

    def sign_certificate(
        self,
        csr_path: str,
        ca_cert_path: str,
        ca_key_path: str,
        output_dir: str,
        valid_days: int = 365,
    ) -> Dict[str, Any]:
        """Sign a CSR with a given CA."""
        try:
            options = SignOptions(
                csr_path=Path(csr_path),
                ca_cert_path=Path(ca_cert_path),
                ca_key_path=Path(ca_key_path),
                output_dir=Path(output_dir),
                valid_days=valid_days,
            )
            result_path = sign_certificate(options)
            return {"cert_path": str(result_path), "success": True}
        except Exception as e:
            return self._handle_exception(e, "certificate signing")

    def export_to_pkcs12(
        self,
        cert_path: str,
        key_path: str,
        password: str,
        export_path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Export certificate to PKCS#12 format."""
        try:
            p_cert_path = Path(cert_path)
            p_key_path = Path(key_path)
            p_export_path = (
                Path(export_path) if export_path else p_cert_path.with_suffix(".pfx")
            )

            export_to_pkcs12_file(p_cert_path, p_key_path, password, p_export_path)
            return {"pfx_path": str(p_export_path), "success": True}
        except Exception as e:
            return self._handle_exception(e, "PKCS#12 export")
