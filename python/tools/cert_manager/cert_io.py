#!/usr/bin/env python3
"""
Certificate I/O Module.

This module provides functions for reading and writing certificate-related
files, such as keys, certificates, CSRs, and CRLs.
"""

from pathlib import Path
from typing import List, Tuple

from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.serialization import pkcs12
from loguru import logger

from .cert_utils import ensure_directory_exists


def save_key(key: rsa.RSAPrivateKey, path: Path) -> None:
    """Saves a private key to a file in PEM format."""
    ensure_directory_exists(path.parent)
    with path.open("wb") as key_file:
        key_file.write(
            key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.TraditionalOpenSSL,
                encryption_algorithm=serialization.NoEncryption(),
            )
        )
    logger.info(f"Private key saved to: {path}")


def save_certificate(cert: x509.Certificate, path: Path) -> None:
    """Saves a certificate to a file in PEM format."""
    ensure_directory_exists(path.parent)
    with path.open("wb") as cert_file:
        cert_file.write(cert.public_bytes(serialization.Encoding.PEM))
    logger.info(f"Certificate saved to: {path}")


def save_csr(csr: x509.CertificateSigningRequest, path: Path) -> None:
    """Saves a CSR to a file in PEM format."""
    ensure_directory_exists(path.parent)
    with path.open("wb") as csr_file:
        csr_file.write(csr.public_bytes(serialization.Encoding.PEM))
    logger.info(f"CSR saved to: {path}")


def save_crl(crl: x509.CertificateRevocationList, path: Path) -> None:
    """Saves a CRL to a file in PEM format."""
    ensure_directory_exists(path.parent)
    with path.open("wb") as crl_file:
        crl_file.write(crl.public_bytes(serialization.Encoding.PEM))
    logger.info(f"CRL saved to: {path}")


def load_certificate(path: Path) -> x509.Certificate:
    """Loads a certificate from a PEM file."""
    if not path.exists():
        raise FileNotFoundError(f"Certificate file not found: {path}")
    with path.open("rb") as f:
        return x509.load_pem_x509_certificate(f.read())


def load_private_key(path: Path) -> rsa.RSAPrivateKey:
    """Loads a private key from a PEM file."""
    if not path.exists():
        raise FileNotFoundError(f"Private key file not found: {path}")
    with path.open("rb") as f:
        key = serialization.load_pem_private_key(f.read(), password=None)
        if not isinstance(key, rsa.RSAPrivateKey):
            raise TypeError("Only RSA keys are supported.")
        return key


def load_csr(path: Path) -> x509.CertificateSigningRequest:
    """Loads a CSR from a PEM file."""
    if not path.exists():
        raise FileNotFoundError(f"CSR file not found: {path}")
    with path.open("rb") as f:
        return x509.load_pem_x509_csr(f.read())


def export_to_pkcs12_file(
    cert: x509.Certificate,
    key: rsa.RSAPrivateKey,
    password: str,
    path: Path,
    friendly_name: bytes,
) -> None:
    """Exports a certificate and key to a PKCS#12 file."""
    ensure_directory_exists(path.parent)
    pfx = pkcs12.serialize_key_and_certificates(
        name=friendly_name,
        key=key,
        cert=cert,
        cas=None,
        encryption_algorithm=serialization.BestAvailableEncryption(password.encode()),
    )
    with path.open("wb") as pfx_file:
        pfx_file.write(pfx)
    logger.info(f"Certificate exported to PKCS#12 format: {path}")


def create_certificate_chain_file(cert_paths: List[Path], output_path: Path) -> None:
    """Creates a certificate chain file from multiple certificates."""
    ensure_directory_exists(output_path.parent)
    with output_path.open("wb") as chain_file:
        for cert_path in cert_paths:
            if not cert_path.exists():
                raise FileNotFoundError(f"Certificate file not found: {cert_path}")
            with cert_path.open("rb") as cert_file:
                chain_file.write(cert_file.read())
                chain_file.write(b"\n")
    logger.info(f"Certificate chain created: {output_path}")
