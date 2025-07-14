#!/usr/bin/env python3
"""
Core certificate operations.
"""

import datetime
import ssl
from pathlib import Path
from typing import List, Optional, Tuple

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import ExtensionOID  # Added import
from loguru import logger

from .cert_builder import CertificateBuilder
from .cert_io import (
    create_certificate_chain_file,
    export_to_pkcs12_file as io_export_to_pkcs12,
    load_certificate,
    load_csr,
    load_private_key,
    save_certificate,
    save_crl,
    save_csr,
    save_key,
)
from .cert_types import (
    CertificateDetails,
    CertificateGenerationError,
    CertificateOptions,
    CertificateResult,
    CSRResult,
    KeyGenerationError,
    RevokedCertInfo,
    RevokeOptions,
    SignOptions,
)
from .cert_utils import log_operation


@log_operation
def create_key(key_size: int = 2048) -> rsa.RSAPrivateKey:
    """Generates an RSA private key."""
    try:
        return rsa.generate_private_key(public_exponent=65537, key_size=key_size)
    except Exception as e:
        raise KeyGenerationError(f"Failed to generate RSA key: {e}") from e


@log_operation
def create_self_signed_cert(options: CertificateOptions) -> CertificateResult:
    """Creates a self-signed SSL certificate."""
    try:
        key = create_key(options.key_size)
        builder = CertificateBuilder(options, key)
        cert = builder.build()

        cert_path = options.cert_dir / f"{options.hostname}.crt"
        key_path = options.cert_dir / f"{options.hostname}.key"

        save_certificate(cert, cert_path)
        save_key(key, key_path)

        return CertificateResult(cert_path=cert_path, key_path=key_path)
    except Exception as e:
        raise CertificateGenerationError(f"Failed to create certificate: {e}") from e


@log_operation
def create_csr(options: CertificateOptions) -> CSRResult:
    """Creates a Certificate Signing Request (CSR)."""
    key = create_key(options.key_size)
    
    # Simplified builder logic for CSR
    name_attributes = [x509.NameAttribute(x509.NameOID.COMMON_NAME, options.hostname)]
    # Add other attributes from options...
    subject = x509.Name(name_attributes)
    
    csr_builder = x509.CertificateSigningRequestBuilder().subject_name(subject)
    csr = csr_builder.sign(key, hashes.SHA256())

    csr_path = options.cert_dir / f"{options.hostname}.csr"
    key_path = options.cert_dir / f"{options.hostname}.key"

    save_csr(csr, csr_path)
    save_key(key, key_path)

    return CSRResult(csr_path=csr_path, key_path=key_path)


@log_operation
def sign_certificate(options: SignOptions) -> Path:
    """Signs a CSR with a CA certificate."""
    csr = load_csr(options.csr_path)
    ca_cert = load_certificate(options.ca_cert_path)
    ca_key = load_private_key(options.ca_key_path)

    builder = (
        x509.CertificateBuilder()
        .subject_name(csr.subject)
        .issuer_name(ca_cert.subject)
        .public_key(csr.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(datetime.datetime.now(datetime.timezone.utc))  # Fixed
        .not_valid_after(
            datetime.datetime.now(datetime.timezone.utc) + 
            datetime.timedelta(days=options.valid_days)  # Fixed
        )
    )
    # Copy extensions from CSR
    for extension in csr.extensions:
        builder = builder.add_extension(extension.value, extension.critical)

    # Add authority key identifier
    builder = builder.add_extension(
        x509.AuthorityKeyIdentifier.from_issuer_public_key(ca_key.public_key()),
        critical=False,
    )

    cert = builder.sign(ca_key, hashes.SHA256())
    
    common_name = csr.subject.get_attributes_for_oid(x509.NameOID.COMMON_NAME)[0].value
    cert_path = options.output_dir / f"{common_name}.crt"
    save_certificate(cert, cert_path)
    return cert_path


@log_operation
def export_to_pkcs12_file(
    cert_path: Path, key_path: Path, password: str, export_path: Path
) -> None:
    """Exports a certificate and key to a PKCS#12 file."""
    cert = load_certificate(cert_path)
    key = load_private_key(key_path)
    friendly_name = cert.subject.rfc4514_string().encode()
    io_export_to_pkcs12(cert, key, password, export_path, friendly_name)


@log_operation
def generate_crl(
    ca_cert_path: Path,
    ca_key_path: Path,
    revoked_certs: List[RevokedCertInfo],
    crl_dir: Path,
    valid_days: int = 30,
) -> Path:
    """Generates a Certificate Revocation List (CRL)."""
    ca_cert = load_certificate(ca_cert_path)
    ca_key = load_private_key(ca_key_path)

    builder = x509.CertificateRevocationListBuilder().issuer_name(ca_cert.subject)
    now = datetime.datetime.now(datetime.timezone.utc)  # Fixed
    builder = builder.last_update(now).next_update(now + datetime.timedelta(days=valid_days))

    for revoked in revoked_certs:
        revoked_builder = (
            x509.RevokedCertificateBuilder()
            .serial_number(revoked.serial_number)
            .revocation_date(revoked.revocation_date)
        )
        if revoked.reason:
            revoked_builder = revoked_builder.add_extension(
                x509.CRLReason(revoked.reason), critical=False
            )
        builder = builder.add_revoked_certificate(revoked_builder.build())

    crl = builder.sign(ca_key, hashes.SHA256())
    crl_path = crl_dir / "revoked.crl"
    save_crl(crl, crl_path)
    return crl_path


@log_operation
def revoke_certificate(options: RevokeOptions) -> Path:
    """Revokes a certificate and updates the CRL."""
    cert_to_revoke = load_certificate(options.cert_to_revoke_path)
    revoked_info = RevokedCertInfo(
        serial_number=cert_to_revoke.serial_number,
        revocation_date=datetime.datetime.now(datetime.timezone.utc),  # Fixed
        reason=options.reason.to_crypto_reason(),
    )
    return generate_crl(
        options.ca_cert_path, options.ca_key_path, [revoked_info], options.crl_path.parent
    )


@log_operation
def load_ssl_context(
    cert_path: Path, key_path: Path, ca_path: Optional[Path] = None
) -> ssl.SSLContext:
    """Loads a security-hardened SSL context."""
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.options |= ssl.OP_NO_TLSv1 | ssl.OP_NO_TLSv1_1
    context.set_ciphers("ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:DHE+CHACHA20")
    context.load_cert_chain(certfile=str(cert_path), keyfile=str(key_path))
    if ca_path and ca_path.exists():
        context.load_verify_locations(cafile=str(ca_path))
        context.verify_mode = ssl.CERT_REQUIRED
    return context


@log_operation
def get_cert_details(cert_path: Path) -> CertificateDetails:
    """Extracts detailed information from a certificate."""
    cert = load_certificate(cert_path)
    is_ca = False
    try:
        basic_constraints = cert.extensions.get_extension_for_oid(ExtensionOID.BASIC_CONSTRAINTS)
        # Defensive: ensure .value is BasicConstraints and has .ca
        try:
            is_ca = bool(getattr(basic_constraints.value, 'ca', False))
        except Exception:
            is_ca = False
    except x509.ExtensionNotFound:
        pass

    return CertificateDetails(
        subject=cert.subject.rfc4514_string(),
        issuer=cert.issuer.rfc4514_string(),
        serial_number=cert.serial_number,
        not_valid_before=cert.not_valid_before,
        not_valid_after=cert.not_valid_after,
        public_key_info=cert.public_key().public_bytes(
            serialization.Encoding.PEM,
            serialization.PublicFormat.SubjectPublicKeyInfo,
        ).decode(),
        signature_algorithm=cert.signature_hash_algorithm.name if cert.signature_hash_algorithm else "unknown",
        # Handle cryptography.x509.Version enum safely
        version=(cert.version.value if hasattr(cert.version, 'value') and isinstance(cert.version.value, int) else 3),
        is_ca=is_ca,
        fingerprint_sha256=cert.fingerprint(hashes.SHA256()).hex(),
        fingerprint_sha1=cert.fingerprint(hashes.SHA1()).hex(),
        key_usage=[str(ku) for ku in getattr(cert, 'key_usage', [])] if hasattr(cert, 'key_usage') else [],
        extended_key_usage=[str(eku) for eku in getattr(cert, 'extended_key_usage', [])] if hasattr(cert, 'extended_key_usage') else [],
        subject_alt_names=[str(san) for san in getattr(cert, 'subject_alt_name', [])] if hasattr(cert, 'subject_alt_name') else [],
    )


@log_operation
def view_cert_details(cert_path: Path) -> None:
    """Displays certificate details to the console."""
    details = get_cert_details(cert_path)
    # Rich printing would be nice here
    print(f"\nCertificate Details for: {cert_path}")
    print("=" * 60)
    print(f"Subject:        {details.subject}")
    print(f"Issuer:         {details.issuer}")
    print(f"Serial Number:  {details.serial_number}")
    print(f"Valid From:     {details.not_valid_before}")
    print(f"Valid Until:    {details.not_valid_after}")
    print(f"Is CA:          {details.is_ca}")
    print(f"Fingerprint:    {details.fingerprint}")
    print("\nExtensions:")
    for ext in details.extensions:
        print(f" - {ext.oid._name}: Critical={ext.critical}")


@log_operation
def check_cert_expiry(cert_path: Path, warning_days: int = 30) -> Tuple[bool, int]:
    """Checks if a certificate is about to expire."""
    details = get_cert_details(cert_path)
    remaining = details.not_valid_after - datetime.datetime.now(datetime.timezone.utc)  # Fixed
    is_expiring = remaining.days <= warning_days
    if is_expiring:
        logger.warning(f"Certificate {cert_path} is expiring in {remaining.days} days")
    else:
        logger.info(f"Certificate {cert_path} is valid for {remaining.days} more days")
    return is_expiring, remaining.days


@log_operation
def renew_cert(cert_path: Path, key_path: Path, valid_days: int = 365) -> Path:
    """Renews an existing certificate."""
    cert = load_certificate(cert_path)
    key = load_private_key(key_path)

    new_builder = (
        x509.CertificateBuilder()
        .subject_name(cert.subject)
        .issuer_name(cert.issuer)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(datetime.datetime.now(datetime.timezone.utc))  # Fixed
        .not_valid_after(
            datetime.datetime.now(datetime.timezone.utc) + 
            datetime.timedelta(days=valid_days)  # Fixed
        )
    )
    for extension in cert.extensions:
        new_builder = new_builder.add_extension(extension.value, extension.critical)

    new_cert = new_builder.sign(key, hashes.SHA256())
    
    common_name = cert.subject.get_attributes_for_oid(x509.NameOID.COMMON_NAME)[0].value
    new_cert_path = cert_path.parent / f"{common_name}_renewed.crt"
    save_certificate(new_cert, new_cert_path)
    return new_cert_path


@log_operation
def create_certificate_chain(cert_paths: List[Path], output_path: Path) -> Path:
    """Creates a certificate chain file."""
    create_certificate_chain_file(cert_paths, output_path)
    return output_path
