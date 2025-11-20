#!/usr/bin/env python3
"""
Certificate operations.

This module provides core functionality for creating, managing, and
validating SSL/TLS certificates.
"""

import datetime
import ssl
from pathlib import Path
from typing import List, Optional, Tuple

from cryptography import x509
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID, ExtensionOID
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.serialization import pkcs12
from loguru import logger

from .cert_types import (
    CertificateOptions, CertificateResult, RevokedCertInfo,
    CertificateDetails, KeyGenerationError, CertificateGenerationError,
    CertificateType
)
from .cert_utils import ensure_directory_exists, log_operation


@log_operation
def create_key(key_size: int = 2048) -> rsa.RSAPrivateKey:
    """
    Generates an RSA private key with the specified key size.

    Args:
        key_size: RSA key size in bits (default: 2048)

    Returns:
        An RSA private key object

    Raises:
        KeyGenerationError: If key generation fails
    """
    try:
        return rsa.generate_private_key(
            public_exponent=65537,  # Standard value for e
            key_size=key_size,
        )
    except Exception as e:
        raise KeyGenerationError(
            f"Failed to generate RSA key: {str(e)}") from e


@log_operation
def create_self_signed_cert(
    options: CertificateOptions
) -> CertificateResult:
    """
    Creates a self-signed SSL certificate based on the provided options.

    This function generates a new key pair and a self-signed certificate
    with the specified parameters. The certificate and key are saved to
    the specified directory.

    Args:
        options: Configuration options for certificate generation

    Returns:
        CertificateResult containing paths to the generated files

    Raises:
        CertificateGenerationError: If certificate generation fails
        OSError: If file operations fail
    """
    try:
        # Ensure the certificate directory exists
        ensure_directory_exists(options.cert_dir)

        # Generate private key
        key = create_key(options.key_size)

        # Prepare subject attributes
        name_attributes = [x509.NameAttribute(
            NameOID.COMMON_NAME, options.hostname)]

        # Add optional attributes if provided
        if options.country:
            name_attributes.append(x509.NameAttribute(
                NameOID.COUNTRY_NAME, options.country))
        if options.state:
            name_attributes.append(x509.NameAttribute(
                NameOID.STATE_OR_PROVINCE_NAME, options.state))
        if options.organization:
            name_attributes.append(x509.NameAttribute(
                NameOID.ORGANIZATION_NAME, options.organization))
        if options.organizational_unit:
            name_attributes.append(x509.NameAttribute(
                NameOID.ORGANIZATIONAL_UNIT_NAME, options.organizational_unit))
        if options.email:
            name_attributes.append(x509.NameAttribute(
                NameOID.EMAIL_ADDRESS, options.email))

        # Create subject
        subject = x509.Name(name_attributes)

        # Prepare subject alternative names
        alt_names = [x509.DNSName(options.hostname)]
        if options.san_list:
            alt_names.extend([x509.DNSName(name) for name in options.san_list])

        # Certificate validity period
        not_valid_before = datetime.datetime.utcnow()
        not_valid_after = not_valid_before + \
            datetime.timedelta(days=options.valid_days)

        # Start building the certificate
        cert_builder = (
            x509.CertificateBuilder()
            .subject_name(subject)
            .issuer_name(subject)  # Self-signed, so issuer = subject
            .public_key(key.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(not_valid_before)
            .not_valid_after(not_valid_after)
            .add_extension(
                x509.SubjectAlternativeName(alt_names),
                critical=False,
            )
        )

        # Add extensions based on certificate type
        match options.cert_type:
            case CertificateType.CA:
                # CA certificate needs special extensions
                cert_builder = cert_builder.add_extension(
                    x509.BasicConstraints(ca=True, path_length=None),
                    critical=True,
                )
                # Add key usage for CA
                cert_builder = cert_builder.add_extension(
                    x509.KeyUsage(
                        digital_signature=True,
                        content_commitment=False,
                        key_encipherment=False,
                        data_encipherment=False,
                        key_agreement=False,
                        key_cert_sign=True,
                        crl_sign=True,
                        encipher_only=False,
                        decipher_only=False
                    ),
                    critical=True
                )

            case CertificateType.CLIENT:
                # Client certificate
                cert_builder = cert_builder.add_extension(
                    x509.BasicConstraints(ca=False, path_length=None),
                    critical=True,
                )
                cert_builder = cert_builder.add_extension(
                    x509.KeyUsage(
                        digital_signature=True,
                        content_commitment=False,
                        key_encipherment=True,
                        data_encipherment=False,
                        key_agreement=False,
                        key_cert_sign=False,
                        crl_sign=False,
                        encipher_only=False,
                        decipher_only=False
                    ),
                    critical=True
                )
                cert_builder = cert_builder.add_extension(
                    x509.ExtendedKeyUsage([ExtendedKeyUsageOID.CLIENT_AUTH]),
                    critical=False,
                )

            case CertificateType.SERVER:
                # Server certificate
                cert_builder = cert_builder.add_extension(
                    x509.BasicConstraints(ca=False, path_length=None),
                    critical=True,
                )
                cert_builder = cert_builder.add_extension(
                    x509.KeyUsage(
                        digital_signature=True,
                        content_commitment=False,
                        key_encipherment=True,
                        data_encipherment=False,
                        key_agreement=False,
                        key_cert_sign=False,
                        crl_sign=False,
                        encipher_only=False,
                        decipher_only=False
                    ),
                    critical=True
                )
                cert_builder = cert_builder.add_extension(
                    x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]),
                    critical=False,
                )

        # Add Subject Key Identifier extension
        subject_key_identifier = x509.SubjectKeyIdentifier.from_public_key(
            key.public_key())
        cert_builder = cert_builder.add_extension(
            subject_key_identifier,
            critical=False
        )

        # Sign the certificate with the private key
        cert = cert_builder.sign(key, hashes.SHA256())

        # Define output paths
        cert_path = options.cert_dir / f"{options.hostname}.crt"
        key_path = options.cert_dir / f"{options.hostname}.key"

        # Write certificate to file
        with cert_path.open("wb") as cert_file:
            cert_file.write(cert.public_bytes(serialization.Encoding.PEM))

        # Write private key to file
        with key_path.open("wb") as key_file:
            key_file.write(
                key.private_bytes(
                    encoding=serialization.Encoding.PEM,
                    format=serialization.PrivateFormat.TraditionalOpenSSL,
                    encryption_algorithm=serialization.NoEncryption(),
                )
            )

        logger.info(f"Certificate created successfully: {cert_path}")
        return CertificateResult(cert_path=cert_path, key_path=key_path)

    except Exception as e:
        error_message = f"Failed to create certificate: {str(e)}"
        logger.error(error_message)
        raise CertificateGenerationError(error_message) from e


@log_operation
def export_to_pkcs12(
    cert_path: Path,
    key_path: Path,
    password: str,
    export_path: Optional[Path] = None
) -> Path:
    """
    Export the certificate and private key to a PKCS#12 (PFX) file.

    The PKCS#12 format is commonly used to import/export certificates and
    private keys in Windows and macOS systems.

    Args:
        cert_path: Path to the certificate file
        key_path: Path to the private key file
        password: Password to protect the PFX file
        export_path: Path to save the PFX file, defaults to same directory as certificate

    Returns:
        Path to the created PFX file

    Raises:
        FileNotFoundError: If certificate or key file doesn't exist
        ValueError: If password is empty or invalid
    """
    # Input validation
    if not cert_path.exists():
        raise FileNotFoundError(f"Certificate file not found: {cert_path}")
    if not key_path.exists():
        raise FileNotFoundError(f"Private key file not found: {key_path}")
    if not password:
        raise ValueError("Password is required for PKCS#12 export")

    # Set default export path if not provided
    if export_path is None:
        export_path = cert_path.with_suffix(".pfx")

    try:
        # Load certificate
        with cert_path.open("rb") as cert_file:
            cert = x509.load_pem_x509_certificate(cert_file.read())

        # Load private key
        with key_path.open("rb") as key_file:
            key = serialization.load_pem_private_key(
                key_file.read(), password=None)

        # Ensure the private key is of a supported type for PKCS#12
        from cryptography.hazmat.primitives.asymmetric import rsa, dsa, ec, ed25519, ed448
        if not isinstance(key, (rsa.RSAPrivateKey, dsa.DSAPrivateKey, ec.EllipticCurvePrivateKey, ed25519.Ed25519PrivateKey, ed448.Ed448PrivateKey)):
            raise TypeError(
                "Unsupported private key type for PKCS#12 export. Must be RSA, DSA, EC, Ed25519, or Ed448 private key.")

        # Create PKCS#12 file
        pfx = pkcs12.serialize_key_and_certificates(
            name=cert.subject.rfc4514_string().encode(),
            key=key,
            cert=cert,
            cas=None,
            encryption_algorithm=serialization.BestAvailableEncryption(
                password.encode())
        )

        # Write to file
        with export_path.open("wb") as pfx_file:
            pfx_file.write(pfx)

        logger.info(f"Certificate exported to PKCS#12 format: {export_path}")
        return export_path

    except Exception as e:
        error_message = f"Failed to export to PKCS#12: {str(e)}"
        logger.error(error_message)
        raise ValueError(error_message) from e


@log_operation
def generate_crl(
    cert_path: Path,
    key_path: Path,
    revoked_certs: List[RevokedCertInfo],
    crl_dir: Path,
    crl_filename: str = "revoked.crl",
    valid_days: int = 30
) -> Path:
    """
    Generate a Certificate Revocation List (CRL) for the given CA certificate.

    Args:
        cert_path: Path to the issuer certificate file
        key_path: Path to the issuer's private key
        revoked_certs: List of certificates to revoke
        crl_dir: Directory to save the CRL file
        crl_filename: Name of the CRL file to create
        valid_days: Number of days the CRL will be valid

    Returns:
        Path to the generated CRL file

    Raises:
        FileNotFoundError: If certificate or key file doesn't exist
        ValueError: If the certificate is not a CA certificate
    """
    # Ensure directories exist
    ensure_directory_exists(crl_dir)

    try:
        # Load the CA certificate
        with cert_path.open("rb") as cert_file:
            cert = x509.load_pem_x509_certificate(cert_file.read())

        # Check if this is a CA certificate
        is_ca = False
        for ext in cert.extensions:
            if ext.oid == ExtensionOID.BASIC_CONSTRAINTS:
                is_ca = ext.value.ca
                break

        if not is_ca:
            raise ValueError(
                f"Certificate {cert_path} is not a CA certificate")

        # Load the private key
        with key_path.open("rb") as key_file:
            private_key = serialization.load_pem_private_key(
                key_file.read(), password=None)

        # Ensure the private key is of a supported type
        from cryptography.hazmat.primitives.asymmetric import rsa, dsa, ec
        if not isinstance(private_key, (rsa.RSAPrivateKey, dsa.DSAPrivateKey, ec.EllipticCurvePrivateKey)):
            raise TypeError(
                "Unsupported private key type for CRL signing. Must be RSA, DSA, or EC private key.")

        # Build the CRL
        builder = x509.CertificateRevocationListBuilder().issuer_name(cert.subject)

        # Add revoked certificates
        for revoked in revoked_certs:
            revoked_cert_builder = x509.RevokedCertificateBuilder().serial_number(
                revoked.serial_number
            ).revocation_date(
                revoked.revocation_date
            )

            if revoked.reason:
                revoked_cert_builder = revoked_cert_builder.add_extension(
                    x509.CRLReason(revoked.reason),
                    critical=False
                )

            builder = builder.add_revoked_certificate(
                revoked_cert_builder.build())

        # Set validity period
        now = datetime.datetime.utcnow()
        builder = builder.last_update(now).next_update(
            now + datetime.timedelta(days=valid_days))

        # Sign the CRL
        crl = builder.sign(private_key, hashes.SHA256())

        # Write to file
        crl_path = crl_dir / crl_filename
        with crl_path.open("wb") as crl_file:
            crl_file.write(crl.public_bytes(serialization.Encoding.PEM))

        logger.info(f"CRL generated: {crl_path}")
        return crl_path

    except Exception as e:
        error_message = f"Failed to generate CRL: {str(e)}"
        logger.error(error_message)
        raise ValueError(error_message) from e


@log_operation
def load_ssl_context(
    cert_path: Path,
    key_path: Path,
    ca_path: Optional[Path] = None
) -> ssl.SSLContext:
    """
    Load an SSL context from certificate and key files.

    Creates a security-hardened SSL context suitable for servers or clients.

    Args:
        cert_path: Path to the certificate file
        key_path: Path to the private key file
        ca_path: Optional path to CA certificate for verification

    Returns:
        An SSLContext object configured with the certificate and key

    Raises:
        FileNotFoundError: If certificate or key file doesn't exist
        ssl.SSLError: If loading the certificate or key fails
    """
    # Verify files exist
    if not cert_path.exists():
        raise FileNotFoundError(f"Certificate file not found: {cert_path}")
    if not key_path.exists():
        raise FileNotFoundError(f"Key file not found: {key_path}")

    # Create SSL context
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)

    # Set security options
    context.options |= ssl.OP_NO_TLSv1 | ssl.OP_NO_TLSv1_1  # Disable TLS 1.0 and 1.1
    context.set_ciphers('ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:DHE+CHACHA20')
    context.load_cert_chain(certfile=str(cert_path), keyfile=str(key_path))

    # Load CA certificate if provided
    if ca_path and ca_path.exists():
        context.load_verify_locations(cafile=str(ca_path))
        context.verify_mode = ssl.CERT_REQUIRED

    return context


@log_operation
def get_cert_details(cert_path: Path) -> CertificateDetails:
    """
    Extract detailed information from a certificate file.

    Args:
        cert_path: Path to the certificate file

    Returns:
        CertificateDetails object containing certificate information

    Raises:
        FileNotFoundError: If certificate file doesn't exist
        ValueError: If the certificate format is invalid
    """
    if not cert_path.exists():
        raise FileNotFoundError(f"Certificate file not found: {cert_path}")

    with cert_path.open("rb") as cert_file:
        cert = x509.load_pem_x509_certificate(cert_file.read())

    # Check if this is a CA certificate
    is_ca = False
    for ext in cert.extensions:
        if ext.oid == ExtensionOID.BASIC_CONSTRAINTS:
            is_ca = ext.value.ca
            break

    # Get public key in PEM format
    public_key = cert.public_key().public_bytes(
        serialization.Encoding.PEM,
        serialization.PublicFormat.SubjectPublicKeyInfo
    ).decode('utf-8')

    # Calculate fingerprint
    fingerprint = cert.fingerprint(hashes.SHA256()).hex()

    return CertificateDetails(
        subject=cert.subject.rfc4514_string(),
        issuer=cert.issuer.rfc4514_string(),
        serial_number=cert.serial_number,
        not_valid_before=cert.not_valid_before,
        not_valid_after=cert.not_valid_after,
        public_key=public_key,
        extensions=list(cert.extensions),
        is_ca=is_ca,
        fingerprint=fingerprint
    )


@log_operation
def view_cert_details(cert_path: Path) -> None:
    """
    Display the details of a certificate to the console.

    Args:
        cert_path: Path to the certificate file

    Raises:
        FileNotFoundError: If certificate file doesn't exist
    """
    details = get_cert_details(cert_path)

    print(f"\nCertificate Details for: {cert_path}")
    print("=" * 60)
    print(f"Subject:        {details.subject}")
    print(f"Issuer:         {details.issuer}")
    print(f"Serial Number:  {details.serial_number}")
    print(f"Valid From:     {details.not_valid_before}")
    print(f"Valid Until:    {details.not_valid_after}")
    print(f"Is CA:          {details.is_ca}")
    print(f"Fingerprint:    {details.fingerprint}")
    print("\nPublic Key:")
    print("-" * 60)
    print(details.public_key)
    print("-" * 60)
    print("\nExtensions:")
    for ext in details.extensions:
        print(f" - {ext.oid._name}: {ext.critical}")


@log_operation
def check_cert_expiry(cert_path: Path, warning_days: int = 30) -> Tuple[bool, int]:
    """
    Check if a certificate is about to expire.

    Args:
        cert_path: Path to the certificate file
        warning_days: Number of days before expiry to trigger a warning

    Returns:
        Tuple of (is_expiring, days_remaining)

    Raises:
        FileNotFoundError: If certificate file doesn't exist
    """
    details = get_cert_details(cert_path)
    remaining_days = (details.not_valid_after -
                      datetime.datetime.utcnow()).days

    is_expiring = remaining_days <= warning_days

    if is_expiring:
        logger.warning(
            f"Certificate {cert_path} is expiring in {remaining_days} days")
    else:
        logger.info(
            f"Certificate {cert_path} is valid for {remaining_days} more days")

    return is_expiring, remaining_days


@log_operation
def renew_cert(
    cert_path: Path,
    key_path: Path,
    valid_days: int = 365,
    new_cert_dir: Optional[Path] = None,
    new_suffix: str = "_renewed"
) -> Path:
    """
    Renew an existing certificate by creating a new one with extended validity.

    Args:
        cert_path: Path to the existing certificate file
        key_path: Path to the existing key file
        valid_days: Number of days the new certificate is valid
        new_cert_dir: Directory to save the new certificate, defaults to the original location
        new_suffix: Suffix to append to the renewed certificate filename

    Returns:
        Path to the new certificate

    Raises:
        FileNotFoundError: If certificate or key file doesn't exist
        ValueError: If the certificate or key format is invalid
    """
    # Set default save directory if not specified
    if new_cert_dir is None:
        new_cert_dir = cert_path.parent
    else:
        ensure_directory_exists(new_cert_dir)

    # Load the existing certificate
    with cert_path.open("rb") as cert_file:
        cert = x509.load_pem_x509_certificate(cert_file.read())

    # Extract subject and issuer
    subject = cert.subject
    issuer = cert.issuer

    # Load the private key
    with key_path.open("rb") as key_file:
        key = serialization.load_pem_private_key(
            key_file.read(), password=None)

    # Ensure the private key is of a supported type
    from cryptography.hazmat.primitives.asymmetric import rsa, dsa, ec, ed25519, ed448
    if not isinstance(key, (rsa.RSAPrivateKey, dsa.DSAPrivateKey, ec.EllipticCurvePrivateKey, ed25519.Ed25519PrivateKey, ed448.Ed448PrivateKey)):
        raise TypeError(
            "Unsupported private key type for certificate renewal. Must be RSA, DSA, EC, Ed25519, or Ed448 private key.")

    # Try to extract the common name for filename
    common_name = None
    for attr in subject.get_attributes_for_oid(NameOID.COMMON_NAME):
        common_name = attr.value
        break

    if not common_name:
        common_name = "certificate"

    # Create new validity period
    now = datetime.datetime.utcnow()

    # Copy extensions from old certificate but update validity
    new_cert_builder = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(issuer)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now)
        .not_valid_after(now + datetime.timedelta(days=valid_days))
    )

    # Copy all extensions from the original certificate
    for extension in cert.extensions:
        new_cert_builder = new_cert_builder.add_extension(
            extension.value,
            extension.critical
        )

    # Sign the new certificate
    new_cert = new_cert_builder.sign(key, hashes.SHA256())

    # Create the new certificate filename
    new_cert_path = new_cert_dir / f"{common_name}{new_suffix}.crt"

    # Write the new certificate
    with new_cert_path.open("wb") as new_cert_file:
        new_cert_file.write(new_cert.public_bytes(serialization.Encoding.PEM))

    logger.info(f"Certificate renewed: {new_cert_path}")
    return new_cert_path


@log_operation
def create_certificate_chain(
    cert_paths: List[Path],
    output_path: Optional[Path] = None
) -> Path:
    """
    Create a certificate chain file from multiple certificates.

    Args:
        cert_paths: List of certificate paths, in order (leaf to root)
        output_path: Output path for the chain file

    Returns:
        Path to the certificate chain file

    Raises:
        FileNotFoundError: If any certificate file doesn't exist
    """
    # Verify all certificate files exist
    for cert_path in cert_paths:
        if not cert_path.exists():
            raise FileNotFoundError(f"Certificate file not found: {cert_path}")

    # Default output path if not specified
    if output_path is None:
        output_path = cert_paths[0].parent / "certificate_chain.pem"

    # Concatenate all certificates
    with output_path.open("wb") as chain_file:
        for cert_path in cert_paths:
            with cert_path.open("rb") as cert_file:
                chain_file.write(cert_file.read())
                chain_file.write(b"\n")  # Add newline between certificates

    logger.info(f"Certificate chain created: {output_path}")
    return output_path