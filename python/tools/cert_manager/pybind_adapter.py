#!/usr/bin/env python3
"""
PyBind11 adapter for Certificate Manager.

This module provides simplified interfaces for C++ bindings via pybind11.
All methods return dict/list types for optimal pybind11 compatibility.
"""

import asyncio
from pathlib import Path
from typing import Dict, List, Optional
from loguru import logger

from .cert_api import CertificateAPI
from .cert_operations import (
    get_cert_details, check_cert_expiry, renew_cert, export_to_pkcs12
)
from .cert_types import CertificateError


class CertManagerPyBindAdapter:
    """
    Adapter class to expose CertificateManager functionality to C++ via pybind11.

    This class simplifies the interface and handles conversions between Python and C++.
    All return types are dict or list for pybind11 compatibility.
    """

    # Synchronous operations

    @staticmethod
    def create_cert(
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
    ) -> Dict:
        """
        Create a self-signed certificate (synchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), cert_path (str), key_path (str), error (str)
        """
        logger.info(
            f"C++ binding: Creating certificate for {hostname} in {cert_dir}")
        try:
            result = CertificateAPI.create_certificate(
                hostname=hostname,
                cert_dir=cert_dir,
                key_size=key_size,
                valid_days=valid_days,
                san_list=san_list or [],
                cert_type=cert_type,
                country=country,
                state=state,
                organization=organization,
                organizational_unit=organizational_unit,
                email=email
            )
            success = result.get("success") == "true"
            return {
                "success": success,
                "cert_path": result.get("cert_path", ""),
                "key_path": result.get("key_path", ""),
                "error": result.get("error", "") if not success else ""
            }
        except Exception as e:
            logger.exception(f"Error in create_cert: {e}")
            return {
                "success": False,
                "cert_path": "",
                "key_path": "",
                "error": str(e)
            }

    @staticmethod
    def renew_cert(
        cert_path: str,
        key_path: str,
        valid_days: int = 365,
        new_cert_dir: Optional[str] = None,
        new_suffix: str = "_renewed"
    ) -> Dict:
        """
        Renew an existing certificate (synchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), new_cert_path (str), error (str)
        """
        logger.info(f"C++ binding: Renewing certificate {cert_path}")
        try:
            new_path = renew_cert(
                cert_path=Path(cert_path),
                key_path=Path(key_path),
                valid_days=valid_days,
                new_cert_dir=Path(new_cert_dir) if new_cert_dir else None,
                new_suffix=new_suffix
            )
            return {
                "success": True,
                "new_cert_path": str(new_path),
                "error": ""
            }
        except Exception as e:
            logger.exception(f"Error in renew_cert: {e}")
            return {
                "success": False,
                "new_cert_path": "",
                "error": str(e)
            }

    @staticmethod
    def export_cert(
        cert_path: str,
        key_path: str,
        password: str,
        export_path: Optional[str] = None
    ) -> Dict:
        """
        Export certificate to PKCS#12 format (synchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), pfx_path (str), error (str)
        """
        logger.info(f"C++ binding: Exporting certificate {cert_path} to PKCS#12")
        try:
            result = CertificateAPI.export_to_pkcs12(
                cert_path=cert_path,
                key_path=key_path,
                password=password,
                export_path=export_path
            )
            success = result.get("success") == "true"
            return {
                "success": success,
                "pfx_path": result.get("pfx_path", ""),
                "error": result.get("error", "") if not success else ""
            }
        except Exception as e:
            logger.exception(f"Error in export_cert: {e}")
            return {
                "success": False,
                "pfx_path": "",
                "error": str(e)
            }

    @staticmethod
    def get_cert_info(cert_path: str) -> Dict:
        """
        Get detailed information about a certificate (synchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), details (dict), error (str)
        """
        logger.info(f"C++ binding: Getting certificate info for {cert_path}")
        try:
            details = get_cert_details(Path(cert_path))
            return {
                "success": True,
                "details": {
                    "subject": details.subject,
                    "issuer": details.issuer,
                    "serial_number": str(details.serial_number),
                    "not_valid_before": details.not_valid_before.isoformat(),
                    "not_valid_after": details.not_valid_after.isoformat(),
                    "is_ca": details.is_ca,
                    "fingerprint": details.fingerprint,
                    "public_key": details.public_key
                },
                "error": ""
            }
        except Exception as e:
            logger.exception(f"Error in get_cert_info: {e}")
            return {
                "success": False,
                "details": {},
                "error": str(e)
            }

    @staticmethod
    def check_expiry(cert_path: str, warning_days: int = 30) -> Dict:
        """
        Check if a certificate is about to expire (synchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), is_expiring (bool), days_remaining (int), error (str)
        """
        logger.info(f"C++ binding: Checking expiry for {cert_path}")
        try:
            is_expiring, days_remaining = check_cert_expiry(
                Path(cert_path), warning_days)
            return {
                "success": True,
                "is_expiring": is_expiring,
                "days_remaining": days_remaining,
                "error": ""
            }
        except Exception as e:
            logger.exception(f"Error in check_expiry: {e}")
            return {
                "success": False,
                "is_expiring": False,
                "days_remaining": 0,
                "error": str(e)
            }

    # Asynchronous operations

    @staticmethod
    async def create_cert_async(
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
    ) -> Dict:
        """
        Create a self-signed certificate (asynchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), cert_path (str), key_path (str), error (str)
        """
        logger.info(
            f"C++ binding (async): Creating certificate for {hostname} in {cert_dir}")
        try:
            loop = asyncio.get_event_loop()
            result = await loop.run_in_executor(
                None,
                lambda: CertificateAPI.create_certificate(
                    hostname=hostname,
                    cert_dir=cert_dir,
                    key_size=key_size,
                    valid_days=valid_days,
                    san_list=san_list or [],
                    cert_type=cert_type,
                    country=country,
                    state=state,
                    organization=organization,
                    organizational_unit=organizational_unit,
                    email=email
                )
            )
            success = result.get("success") == "true"
            return {
                "success": success,
                "cert_path": result.get("cert_path", ""),
                "key_path": result.get("key_path", ""),
                "error": result.get("error", "") if not success else ""
            }
        except Exception as e:
            logger.exception(f"Error in create_cert_async: {e}")
            return {
                "success": False,
                "cert_path": "",
                "key_path": "",
                "error": str(e)
            }

    @staticmethod
    async def renew_cert_async(
        cert_path: str,
        key_path: str,
        valid_days: int = 365,
        new_cert_dir: Optional[str] = None,
        new_suffix: str = "_renewed"
    ) -> Dict:
        """
        Renew an existing certificate (asynchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), new_cert_path (str), error (str)
        """
        logger.info(f"C++ binding (async): Renewing certificate {cert_path}")
        try:
            loop = asyncio.get_event_loop()
            new_path = await loop.run_in_executor(
                None,
                lambda: renew_cert(
                    cert_path=Path(cert_path),
                    key_path=Path(key_path),
                    valid_days=valid_days,
                    new_cert_dir=Path(new_cert_dir) if new_cert_dir else None,
                    new_suffix=new_suffix
                )
            )
            return {
                "success": True,
                "new_cert_path": str(new_path),
                "error": ""
            }
        except Exception as e:
            logger.exception(f"Error in renew_cert_async: {e}")
            return {
                "success": False,
                "new_cert_path": "",
                "error": str(e)
            }

    @staticmethod
    async def export_cert_async(
        cert_path: str,
        key_path: str,
        password: str,
        export_path: Optional[str] = None
    ) -> Dict:
        """
        Export certificate to PKCS#12 format (asynchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), pfx_path (str), error (str)
        """
        logger.info(
            f"C++ binding (async): Exporting certificate {cert_path} to PKCS#12")
        try:
            loop = asyncio.get_event_loop()
            result = await loop.run_in_executor(
                None,
                lambda: CertificateAPI.export_to_pkcs12(
                    cert_path=cert_path,
                    key_path=key_path,
                    password=password,
                    export_path=export_path
                )
            )
            success = result.get("success") == "true"
            return {
                "success": success,
                "pfx_path": result.get("pfx_path", ""),
                "error": result.get("error", "") if not success else ""
            }
        except Exception as e:
            logger.exception(f"Error in export_cert_async: {e}")
            return {
                "success": False,
                "pfx_path": "",
                "error": str(e)
            }

    @staticmethod
    async def get_cert_info_async(cert_path: str) -> Dict:
        """
        Get detailed information about a certificate (asynchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), details (dict), error (str)
        """
        logger.info(f"C++ binding (async): Getting certificate info for {cert_path}")
        try:
            loop = asyncio.get_event_loop()
            details = await loop.run_in_executor(
                None,
                lambda: get_cert_details(Path(cert_path))
            )
            return {
                "success": True,
                "details": {
                    "subject": details.subject,
                    "issuer": details.issuer,
                    "serial_number": str(details.serial_number),
                    "not_valid_before": details.not_valid_before.isoformat(),
                    "not_valid_after": details.not_valid_after.isoformat(),
                    "is_ca": details.is_ca,
                    "fingerprint": details.fingerprint,
                    "public_key": details.public_key
                },
                "error": ""
            }
        except Exception as e:
            logger.exception(f"Error in get_cert_info_async: {e}")
            return {
                "success": False,
                "details": {},
                "error": str(e)
            }

    @staticmethod
    async def check_expiry_async(cert_path: str, warning_days: int = 30) -> Dict:
        """
        Check if a certificate is about to expire (asynchronous wrapper for C++ binding).

        Returns:
            Dictionary with keys: success (bool), is_expiring (bool), days_remaining (int), error (str)
        """
        logger.info(f"C++ binding (async): Checking expiry for {cert_path}")
        try:
            loop = asyncio.get_event_loop()
            is_expiring, days_remaining = await loop.run_in_executor(
                None,
                lambda: check_cert_expiry(Path(cert_path), warning_days)
            )
            return {
                "success": True,
                "is_expiring": is_expiring,
                "days_remaining": days_remaining,
                "error": ""
            }
        except Exception as e:
            logger.exception(f"Error in check_expiry_async: {e}")
            return {
                "success": False,
                "is_expiring": False,
                "days_remaining": 0,
                "error": str(e)
            }
