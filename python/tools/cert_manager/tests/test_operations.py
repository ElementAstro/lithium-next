#!/usr/bin/env python3
"""
Tests for certificate operations.
"""

import datetime
from pathlib import Path
from unittest.mock import patch

import pytest
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa

from cert_manager.cert_operations import (
    create_self_signed_cert,
    create_csr,
    sign_certificate,
    renew_cert,
    create_key,
)
from cert_manager.cert_types import (
    CertificateOptions,
    CertificateType,
    SignOptions,
)


@pytest.fixture
def temp_cert_dir(tmp_path: Path) -> Path:
    """Create a temporary directory for certificates."""
    return tmp_path / "certs"


@pytest.fixture
def basic_options(temp_cert_dir: Path) -> CertificateOptions:
    """Fixture for basic certificate options."""
    return CertificateOptions(
        hostname="test.local",
        cert_dir=temp_cert_dir,
        key_size=512,  # Use smaller key size for faster tests
    )


def test_create_key():
    """Test RSA key generation."""
    key = create_key(key_size=512)
    assert isinstance(key, rsa.RSAPrivateKey)
    assert key.key_size == 512


def test_create_self_signed_cert(basic_options: CertificateOptions):
    """Test creation of a self-signed certificate."""
    result = create_self_signed_cert(basic_options)

    assert result.cert_path.exists()
    assert result.key_path.exists()

    # Verify certificate content
    with result.cert_path.open("rb") as f:
        cert = x509.load_pem_x509_certificate(f.read())
    assert (
        cert.subject.get_attributes_for_oid(x509.NameOID.COMMON_NAME)[0].value
        == "test.local"
    )
    assert cert.issuer == cert.subject  # Self-signed


def test_create_csr(basic_options: CertificateOptions):
    """Test creation of a Certificate Signing Request."""
    result = create_csr(basic_options)

    assert result.csr_path.exists()
    assert result.key_path.exists()

    # Verify CSR content
    with result.csr_path.open("rb") as f:
        csr = x509.load_pem_x509_csr(f.read())
    assert (
        csr.subject.get_attributes_for_oid(x509.NameOID.COMMON_NAME)[0].value
        == "test.local"
    )
    assert csr.is_signature_valid


def test_sign_certificate(basic_options: CertificateOptions, temp_cert_dir: Path):
    """Test signing a CSR with a CA."""
    # 1. Create a CA
    ca_options = CertificateOptions(
        hostname="ca.local",
        cert_dir=temp_cert_dir,
        cert_type=CertificateType.CA,
        key_size=512,
    )
    ca_result = create_self_signed_cert(ca_options)

    # 2. Create a CSR
    csr_result = create_csr(basic_options)

    # 3. Sign the CSR with the CA
    sign_options = SignOptions(
        csr_path=csr_result.csr_path,
        ca_cert_path=ca_result.cert_path,
        ca_key_path=ca_result.key_path,
        output_dir=temp_cert_dir,
        valid_days=90,
    )
    signed_cert_path = sign_certificate(sign_options)

    assert signed_cert_path.exists()

    # Verify the signed certificate
    with ca_result.cert_path.open("rb") as f:
        ca_cert = x509.load_pem_x509_certificate(f.read())
    with signed_cert_path.open("rb") as f:
        signed_cert = x509.load_pem_x509_certificate(f.read())

    assert (
        signed_cert.subject.get_attributes_for_oid(x509.NameOID.COMMON_NAME)[0].value
        == "test.local"
    )
    assert signed_cert.issuer == ca_cert.subject


def test_renew_cert(basic_options: CertificateOptions):
    """Test certificate renewal."""
    # 1. Create an original certificate
    original_result = create_self_signed_cert(basic_options)
    with original_result.cert_path.open("rb") as f:
        original_cert = x509.load_pem_x509_certificate(f.read())

    # 2. Renew it
    with patch("cert_manager.cert_operations.datetime") as mock_datetime:
        # Mock time to be in the future to see a change in validity
        future_time = datetime.datetime.utcnow() + datetime.timedelta(days=100)
        mock_datetime.utcnow.return_value = future_time

        renewed_cert_path = renew_cert(
            cert_path=original_result.cert_path,
            key_path=original_result.key_path,
            valid_days=180,
        )

    assert renewed_cert_path.exists()
    with renewed_cert_path.open("rb") as f:
        renewed_cert = x509.load_pem_x509_certificate(f.read())

    # Verify that the new certificate has an updated validity period
    assert renewed_cert.not_valid_before == future_time
    assert renewed_cert.not_valid_after > original_cert.not_valid_after
    assert renewed_cert.subject == original_cert.subject
