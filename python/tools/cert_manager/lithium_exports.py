"""
Lithium export declarations for cert_manager module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from lithium_bridge import expose_command, expose_controller

from .cert_api import CertificateAPI


@expose_controller(
    endpoint="/api/cert/create",
    method="POST",
    description="Create a self-signed certificate",
    tags=["certificate", "security"],
    version="1.0.0",
)
def create_certificate(
    hostname: str,
    cert_dir: str,
    key_size: int = 2048,
    valid_days: int = 365,
    san_list: list = None,
    cert_type: str = "server",
    country: str = None,
    state: str = None,
    organization: str = None,
    organizational_unit: str = None,
    email: str = None,
) -> dict:
    """
    Create a self-signed certificate.

    Args:
        hostname: The hostname for the certificate
        cert_dir: Directory to store the certificate
        key_size: RSA key size (default: 2048)
        valid_days: Certificate validity in days (default: 365)
        san_list: Subject Alternative Names list
        cert_type: Certificate type (server/client/ca)
        country: Country code (e.g., "US")
        state: State or province
        organization: Organization name
        organizational_unit: Organizational unit
        email: Email address

    Returns:
        Dictionary with cert_path, key_path, and success status
    """
    return CertificateAPI.create_certificate(
        hostname=hostname,
        cert_dir=cert_dir,
        key_size=key_size,
        valid_days=valid_days,
        san_list=san_list,
        cert_type=cert_type,
        country=country,
        state=state,
        organization=organization,
        organizational_unit=organizational_unit,
        email=email,
    )


@expose_controller(
    endpoint="/api/cert/export-pkcs12",
    method="POST",
    description="Export certificate to PKCS#12 format",
    tags=["certificate", "security"],
    version="1.0.0",
)
def export_to_pkcs12(
    cert_path: str,
    key_path: str,
    password: str,
    export_path: str = None,
) -> dict:
    """
    Export certificate to PKCS#12 format.

    Args:
        cert_path: Path to the certificate file
        key_path: Path to the private key file
        password: Password for the PKCS#12 file
        export_path: Optional output path for the PKCS#12 file

    Returns:
        Dictionary with export_path and success status
    """
    return CertificateAPI.export_to_pkcs12(
        cert_path=cert_path,
        key_path=key_path,
        password=password,
        export_path=export_path,
    )


@expose_command(
    command_id="cert.create",
    description="Create a self-signed certificate (command)",
    priority=5,
    timeout_ms=30000,
    tags=["certificate"],
)
def cmd_create_certificate(
    hostname: str,
    cert_dir: str,
    key_size: int = 2048,
    valid_days: int = 365,
) -> dict:
    """Create certificate via command dispatcher."""
    return CertificateAPI.create_certificate(
        hostname=hostname,
        cert_dir=cert_dir,
        key_size=key_size,
        valid_days=valid_days,
    )
