#!/usr/bin/env python3
"""
Certificate management command-line interface.

This module provides a CLI for creating, managing, and validating SSL/TLS certificates.
"""

import argparse
import sys
from pathlib import Path
from typing import Optional

from loguru import logger

from .cert_types import CertificateOptions, CertificateType
from .cert_operations import (
    create_self_signed_cert,
    view_cert_details,
    check_cert_expiry,
    renew_cert,
    export_to_pkcs12,
)


def setup_logger() -> None:
    """Configure loguru logger."""
    # Remove default handler
    logger.remove()

    # Add stdout handler with formatting
    logger.add(
        sys.stdout,
        format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan> - <level>{message}</level>",
        level="INFO",
    )

    # Add file handler with rotation
    logger.add(
        "certificate_tool.log", rotation="10 MB", retention="1 week", level="DEBUG"
    )


def run_cli() -> int:
    """
    Main CLI entry point.

    Returns:
        Exit code (0 for success, non-zero for errors)
    """
    # Setup logger first thing
    setup_logger()

    parser = argparse.ArgumentParser(description="Advanced Certificate Management Tool")

    parser.add_argument("--hostname", help="The hostname for the certificate")
    parser.add_argument(
        "--cert-dir",
        type=Path,
        default=Path("./certs"),
        help="Directory to save the certificates",
    )
    parser.add_argument(
        "--key-size", type=int, default=2048, help="Size of RSA key in bits"
    )
    parser.add_argument(
        "--valid-days",
        type=int,
        default=365,
        help="Number of days the certificate is valid",
    )
    parser.add_argument(
        "--san", nargs="*", help="List of Subject Alternative Names (SANs)"
    )
    parser.add_argument(
        "--cert-type",
        default="server",
        choices=["server", "client", "ca"],
        help="Type of certificate to create",
    )
    parser.add_argument("--country", help="Country name (C)")
    parser.add_argument("--state", help="State or Province name (ST)")
    parser.add_argument("--organization", help="Organization name (O)")
    parser.add_argument("--organizational-unit", help="Organizational Unit (OU)")
    parser.add_argument("--email", help="Email address")
    parser.add_argument("--cert-file", type=Path, help="Certificate file path")
    parser.add_argument("--key-file", type=Path, help="Private key file path")
    parser.add_argument(
        "--warning-days",
        type=int,
        default=30,
        help="Days before expiry to show warning",
    )
    parser.add_argument("--pfx-password", help="Password for PFX export")
    parser.add_argument("--pfx-output", type=Path, help="Output path for PFX file")
    parser.add_argument("--debug", action="store_true", help="Enable debug logging")

    # Create action group for mutually exclusive operations
    action_group = parser.add_mutually_exclusive_group()
    action_group.add_argument(
        "--create", action="store_true", help="Create a new self-signed certificate"
    )
    action_group.add_argument(
        "--view", action="store_true", help="View certificate details"
    )
    action_group.add_argument(
        "--check-expiry",
        action="store_true",
        help="Check if the certificate is about to expire",
    )
    action_group.add_argument(
        "--renew", action="store_true", help="Renew the certificate"
    )
    action_group.add_argument(
        "--export-pfx", action="store_true", help="Export certificate as PKCS#12"
    )

    # Parse arguments
    args = parser.parse_args()

    # Set debug level if requested
    if args.debug:
        logger.remove()
        logger.add(sys.stdout, level="DEBUG")
        logger.add("certificate_tool.log", level="DEBUG")

    try:
        # Handle operations based on args
        if args.create:
            if not args.hostname:
                logger.error("Hostname is required for certificate creation")
                return 1

            options = CertificateOptions(
                hostname=args.hostname,
                cert_dir=args.cert_dir,
                key_size=args.key_size,
                valid_days=args.valid_days,
                san_list=args.san or [],
                cert_type=CertificateType.from_string(args.cert_type),
                country=args.country,
                state=args.state,
                organization=args.organization,
                organizational_unit=args.organizational_unit,
                email=args.email,
            )

            result = create_self_signed_cert(options)
            print(f"Certificate generated: {result.cert_path}")
            print(f"Private key generated: {result.key_path}")

        elif args.view:
            if not args.cert_file:
                logger.error("Certificate file path is required for viewing")
                return 1
            view_cert_details(args.cert_file)

        elif args.check_expiry:
            if not args.cert_file:
                logger.error("Certificate file path is required for expiry check")
                return 1
            is_expiring, days = check_cert_expiry(args.cert_file, args.warning_days)
            if is_expiring:
                print(f"WARNING: Certificate will expire in {days} days")
            else:
                print(f"Certificate is valid for {days} more days")

        elif args.renew:
            if not args.cert_file or not args.key_file:
                logger.error("Certificate and key file paths are required for renewal")
                return 1
            new_cert_path = renew_cert(args.cert_file, args.key_file, args.valid_days)
            print(f"Certificate renewed: {new_cert_path}")

        elif args.export_pfx:
            if not args.cert_file or not args.key_file or not args.pfx_password:
                logger.error(
                    "Certificate, key, and password are required for PFX export"
                )
                return 1
            pfx_path = export_to_pkcs12(
                args.cert_file, args.key_file, args.pfx_password, args.pfx_output
            )
            print(f"Certificate exported to: {pfx_path}")

        else:
            parser.print_help()
            return 0

        return 0

    except Exception as e:
        logger.exception(f"Error: {str(e)}")
        return 1


if __name__ == "__main__":
    sys.exit(run_cli())
