#!/usr/bin/env python3
"""
Certificate Builder Module.

This module provides a fluent builder for creating x509 certificates,
abstracting the complexities of the `cryptography` library.
"""

import datetime
from typing import List

from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID

from .cert_types import CertificateType, CertificateOptions


class CertificateBuilder:
    """A builder for creating x509 certificates."""

    def __init__(self, options: CertificateOptions, key: rsa.RSAPrivateKey):
        self._options = options
        self._key = key
        self._builder = x509.CertificateBuilder()

    def build(self) -> x509.Certificate:
        """Builds and signs the certificate."""
        self._prepare_subject_and_issuer()
        self._set_validity_period()
        self._add_basic_constraints()
        self._add_key_usage()
        self._add_extended_key_usage()
        self._add_subject_alternative_name()
        self._add_subject_key_identifier()

        return self._builder.sign(self._key, hashes.SHA256())

    def _prepare_subject_and_issuer(self) -> None:
        """Sets the subject and issuer names."""
        name_attributes = self._get_name_attributes()
        subject = x509.Name(name_attributes)
        issuer = subject  # Self-signed

        self._builder = self._builder.subject_name(subject)
        self._builder = self._builder.issuer_name(issuer)
        self._builder = self._builder.public_key(self._key.public_key())
        self._builder = self._builder.serial_number(x509.random_serial_number())

    def _get_name_attributes(self) -> List[x509.NameAttribute]:
        """Constructs the list of X.509 name attributes."""
        attrs = [x509.NameAttribute(NameOID.COMMON_NAME, self._options.hostname)]
        if self._options.country:
            attrs.append(
                x509.NameAttribute(NameOID.COUNTRY_NAME, self._options.country)
            )
        if self._options.state:
            attrs.append(
                x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, self._options.state)
            )
        if self._options.organization:
            attrs.append(
                x509.NameAttribute(
                    NameOID.ORGANIZATION_NAME, self._options.organization
                )
            )
        if self._options.organizational_unit:
            attrs.append(
                x509.NameAttribute(
                    NameOID.ORGANIZATIONAL_UNIT_NAME, self._options.organizational_unit
                )
            )
        if self._options.email:
            attrs.append(x509.NameAttribute(NameOID.EMAIL_ADDRESS, self._options.email))
        return attrs

    def _set_validity_period(self) -> None:
        """Sets the Not Before and Not After dates."""
        not_valid_before = datetime.datetime.utcnow()
        not_valid_after = not_valid_before + datetime.timedelta(
            days=self._options.valid_days
        )
        self._builder = self._builder.not_valid_before(not_valid_before)
        self._builder = self._builder.not_valid_after(not_valid_after)

    def _add_basic_constraints(self) -> None:
        """Adds the Basic Constraints extension."""
        is_ca = self._options.cert_type == CertificateType.CA
        self._builder = self._builder.add_extension(
            x509.BasicConstraints(ca=is_ca, path_length=None),
            critical=True,
        )

    def _add_key_usage(self) -> None:
        """Adds the Key Usage extension based on certificate type."""
        usage = None
        if self._options.cert_type == CertificateType.CA:
            usage = x509.KeyUsage(
                digital_signature=True,
                key_cert_sign=True,
                crl_sign=True,
                content_commitment=False,
                key_encipherment=False,
                data_encipherment=False,
                key_agreement=False,
                encipher_only=False,
                decipher_only=False,
            )
        elif self._options.cert_type in (
            CertificateType.SERVER,
            CertificateType.CLIENT,
        ):
            usage = x509.KeyUsage(
                digital_signature=True,
                key_encipherment=True,
                content_commitment=False,
                data_encipherment=False,
                key_agreement=False,
                key_cert_sign=False,
                crl_sign=False,
                encipher_only=False,
                decipher_only=False,
            )
        if usage:
            self._builder = self._builder.add_extension(usage, critical=True)

    def _add_extended_key_usage(self) -> None:
        """Adds the Extended Key Usage extension."""
        ext_usage = []
        if self._options.cert_type == CertificateType.SERVER:
            ext_usage.append(ExtendedKeyUsageOID.SERVER_AUTH)
        elif self._options.cert_type == CertificateType.CLIENT:
            ext_usage.append(ExtendedKeyUsageOID.CLIENT_AUTH)

        if ext_usage:
            self._builder = self._builder.add_extension(
                x509.ExtendedKeyUsage(ext_usage),
                critical=False,
            )

    def _add_subject_alternative_name(self) -> None:
        """Adds the Subject Alternative Name (SAN) extension."""
        alt_names = [x509.DNSName(self._options.hostname)]
        if self._options.san_list:
            alt_names.extend(x509.DNSName(name) for name in self._options.san_list)
        self._builder = self._builder.add_extension(
            x509.SubjectAlternativeName(alt_names),
            critical=False,
        )

    def _add_subject_key_identifier(self) -> None:
        """Adds the Subject Key Identifier extension."""
        self._builder = self._builder.add_extension(
            x509.SubjectKeyIdentifier.from_public_key(self._key.public_key()),
            critical=False,
        )
