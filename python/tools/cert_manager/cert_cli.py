#!/usr/bin/env python3
"""
Certificate management command-line interface using Typer.
"""

import sys
from pathlib import Path
from typing import List, Optional, Any

import typer
from loguru import logger
from rich.console import Console

from .cert_config import ConfigManager
from .cert_operations import (
    check_cert_expiry,
    create_csr,
    create_self_signed_cert,
    export_to_pkcs12_file,
    generate_crl,
    renew_cert,
    revoke_certificate,
    sign_certificate,
    view_cert_details,
)
from .cert_types import (
    CertificateType,
    RevocationReason,
    RevokeOptions,
    SignOptions,
    CertificateOptions,  # Added missing import
)

app = typer.Typer(
    name="certmanager",
    help="Advanced Certificate Management Tool",
    add_completion=False,
)
console = Console()


def setup_logger(debug: bool):
    """Configures the logger based on debug flag."""
    level = "DEBUG" if debug else "INFO"
    logger.remove()
    logger.add(
        sys.stderr,
        level=level,
        format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan> - <level>{message}</level>",
    )


def get_options(ctx: typer.Context, **kwargs) -> CertificateOptions:  # Added return type
    """Helper to merge config file settings with CLI arguments."""
    config_path = ctx.meta.get("config_path")
    if not config_path:
        raise typer.BadParameter("Config path is required")
    profile = ctx.meta.get("profile")
    manager = ConfigManager(config_path=Path(config_path), profile_name=profile)  # Ensure Path conversion
    return manager.get_options(kwargs)


@app.callback()
def main(
    ctx: typer.Context,
    debug: bool = typer.Option(False, "--debug", help="Enable debug logging."),
    config: Path = typer.Option(Path("config.toml"), "--config", help="Path to config file."),
    profile: Optional[str] = typer.Option(None, "--profile", "-p", help="Config profile to use."),
):
    """Manage SSL/TLS certificates."""
    setup_logger(debug)
    ctx.meta["config_path"] = config
    ctx.meta["profile"] = profile


@app.command()
def create(
    ctx: typer.Context,
    hostname: str = typer.Option(..., "--hostname", help="The hostname for the certificate (CN)."),
    cert_dir: Optional[Path] = typer.Option(None, "--cert-dir", help="Directory to save files."),
    key_size: Optional[int] = typer.Option(None, "--key-size", help="Size of RSA key in bits."),
    valid_days: Optional[int] = typer.Option(None, "--valid-days", help="Certificate validity period."),
    san: Optional[List[str]] = typer.Option(None, "--san", help="Subject Alternative Names."),
    cert_type: Optional[CertificateType] = typer.Option(None, "--cert-type", help="Type of certificate."),
    country: Optional[str] = typer.Option(None, "--country", help="Country name (C)."),
    state: Optional[str] = typer.Option(None, "--state", help="State or Province name (ST)."),
    organization: Optional[str] = typer.Option(None, "--org", help="Organization name (O)."),
    org_unit: Optional[str] = typer.Option(None, "--org-unit", help="Organizational Unit (OU)."),
    email: Optional[str] = typer.Option(None, "--email", help="Email address."),
    auto_confirm: bool = typer.Option(False, "--auto-confirm", help="Skip confirmation prompts."),
):
    """Create a new self-signed certificate."""
    options = get_options(ctx, **{
        k: v for k, v in locals().items() 
        if k not in ['ctx', 'auto_confirm'] and v is not None
    })
    console.print(f"Creating certificate for [bold cyan]{options.hostname}[/bold cyan]...")
    if not auto_confirm and not typer.confirm("Proceed with certificate creation?"):
        raise typer.Abort()
    result = create_self_signed_cert(options)
    if result and hasattr(result, 'cert_path') and hasattr(result, 'key_path'):
        console.print(f"[green]✔[/green] Certificate created: {result.cert_path}")
        console.print(f"[green]✔[/green] Private key created: {result.key_path}")


@app.command("csr")
def create_csr_command(
    ctx: typer.Context,
    hostname: str = typer.Option(..., "--hostname", help="The hostname for the CSR (CN)."),
    cert_dir: Optional[Path] = typer.Option(None, "--cert-dir", help="Directory to save files."),
    # ... other options similar to create ...
):
    """Create a Certificate Signing Request (CSR)."""
    options = get_options(ctx, **{
        k: v for k, v in locals().items() 
        if k != 'ctx' and v is not None
    })
    console.print(f"Creating CSR for [bold cyan]{options.hostname}[/bold cyan]...")
    result = create_csr(options)
    if result and hasattr(result, 'csr_path') and hasattr(result, 'key_path'):
        console.print(f"[green]✔[/green] CSR created: {result.csr_path}")
        console.print(f"[green]✔[/green] Private key created: {result.key_path}")


@app.command()
def sign(
    csr_path: Path = typer.Option(..., "--csr", help="Path to the CSR file to sign."),
    ca_cert_path: Path = typer.Option(..., "--ca-cert", help="Path to the CA certificate."),
    ca_key_path: Path = typer.Option(..., "--ca-key", help="Path to the CA private key."),
    output_dir: Path = typer.Option(Path("./certs"), "--out", help="Directory to save the signed certificate."),
    valid_days: int = typer.Option(365, "--valid-days", help="Validity period for the new certificate."),
):
    """Sign a CSR with a CA."""
    options = SignOptions(
        csr_path=csr_path,
        ca_cert_path=ca_cert_path,
        ca_key_path=ca_key_path,
        output_dir=output_dir,
        valid_days=valid_days,
    )
    console.print(f"Signing CSR [bold cyan]{csr_path.name}[/bold cyan]...")
    cert_path = sign_certificate(options)
    console.print(f"[green]✔[/green] Certificate signed successfully: {cert_path}")


@app.command()
def view(cert_path: Path = typer.Argument(..., help="Path to the certificate file.")):
    """View details of a certificate."""
    view_cert_details(cert_path)


@app.command("check-expiry")
def check_expiry_command(
    cert_path: Path = typer.Argument(..., help="Path to the certificate file."),
    warning_days: int = typer.Option(30, "--warn", help="Days before expiry to warn."),
):
    """Check if a certificate is about to expire."""
    is_expiring, days_left = check_cert_expiry(cert_path, warning_days)
    if is_expiring:
        console.print(f"[yellow]WARNING[/yellow]: Certificate will expire in {days_left} days.")
    else:
        console.print(f"[green]OK[/green]: Certificate is valid for {days_left} more days.")


@app.command()
def renew(
    cert_path: Path = typer.Option(..., "--cert", help="Path to the certificate to renew."),
    key_path: Path = typer.Option(..., "--key", help="Path to the private key."),
    valid_days: int = typer.Option(365, "--valid-days", help="New validity period."),
):
    """Renew an existing certificate."""
    console.print(f"Renewing certificate [bold cyan]{cert_path.name}[/bold cyan]...")
    new_cert_path = renew_cert(cert_path, key_path, valid_days)
    console.print(f"[green]✔[/green] Certificate renewed successfully: {new_cert_path}")


@app.command("export-pfx")
def export_pfx_command(
    cert_path: Path = typer.Option(..., "--cert", help="Path to the certificate."),
    key_path: Path = typer.Option(..., "--key", help="Path to the private key."),
    password: str = typer.Option(..., "--password", help="Password for the PFX file.", prompt=True, hide_input=True),
    output_path: Optional[Path] = typer.Option(None, "--out", help="Output path for the PFX file."),
):
    """Export a certificate and key to a PKCS#12 (.pfx) file."""
    if not output_path:
        output_path = cert_path.with_suffix(".pfx")
    console.print(f"Exporting certificate to [bold cyan]{output_path}[/bold cyan]...")
    export_to_pkcs12_file(cert_path, key_path, password, output_path)
    console.print(f"[green]✔[/green] Export successful.")


@app.command()
def revoke(
    cert_to_revoke_path: Path = typer.Option(..., "--cert", help="Path to the certificate to revoke."),
    ca_cert_path: Path = typer.Option(..., "--ca-cert", help="Path to the CA certificate."),
    ca_key_path: Path = typer.Option(..., "--ca-key", help="Path to the CA private key."),
    crl_path: Path = typer.Option(..., "--crl", help="Path to the existing CRL file."),
    reason: RevocationReason = typer.Option(RevocationReason.UNSPECIFIED, "--reason", help="Reason for revocation."),
):
    """Revoke a certificate and update the CRL."""
    options = RevokeOptions(
        cert_to_revoke_path=cert_to_revoke_path,
        ca_cert_path=ca_cert_path,
        ca_key_path=ca_key_path,
        crl_path=crl_path,
        reason=reason,
    )
    console.print(f"Revoking certificate [bold cyan]{cert_to_revoke_path.name}[/bold cyan]...")
    new_crl_path = revoke_certificate(options)
    console.print(f"[green]✔[/green] Certificate revoked. CRL updated at: {new_crl_path}")


@app.command("generate-crl")
def generate_crl_command(
    ca_cert_path: Path = typer.Option(..., "--ca-cert", help="Path to the CA certificate."),
    ca_key_path: Path = typer.Option(..., "--ca-key", help="Path to the CA private key."),
    output_dir: Path = typer.Option(Path("./crl"), "--out", help="Directory to save the CRL file."),
):
    """Generate a new (empty) Certificate Revocation List (CRL)."""
    console.print("Generating new CRL...")
    crl_path = generate_crl(ca_cert_path, ca_key_path, [], output_dir)
    console.print(f"[green]✔[/green] Empty CRL generated at: {crl_path}")


if __name__ == "__main__":
    app()