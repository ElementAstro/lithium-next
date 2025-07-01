# Advanced Certificate Management Tool

This tool provides comprehensive functionality for creating, managing, and validating SSL/TLS certificates. It supports a full certificate lifecycle, from key generation and CSRs to signing, revocation, and automated renewal.

## Features

- **Certificate Creation**: Generate self-signed server, client, or CA certificates.
- **CSR Management**: Create and sign Certificate Signing Requests (CSRs).
- **PKI Management**: Use a CA certificate to sign other certificates.
- **Revocation**: Revoke certificates and generate Certificate Revocation Lists (CRLs).
- **Multiple Export Formats**: Export certificates to PEM and PKCS#12 (.pfx).
- **Configuration Profiles**: Define certificate profiles in a `config.toml` for easy and repeatable generation.
- **Modern CLI**: A powerful and easy-to-use command-line interface.
- **Programmatic API**: A stable API for integration with other tools and C++ bindings.

## Installation

Install the tool and its dependencies using `pip`:

```bash
pip install .
```

For development, install with the `dev` extras:

```bash
pip install -e ".[dev]"
```

## Usage

The tool is available via the `certmanager` command.

### Quick Start: Create a Self-Signed Certificate

```bash
certmanager create --hostname my.server.com --cert-type server --auto-confirm
```

This will create `my.server.com.crt` and `my.server.com.key` in the `./certs` directory.

### Using Configuration Profiles

You can define certificate settings in a `config.toml` file.

**Example `config.toml`:**

```toml
[default]
cert_dir = "./certs"
key_size = 2048
valid_days = 365
country = "US"
state = "California"
organization = "My Org"

[profiles.server]
cert_type = "server"
san_list = ["server1.my.org", "server2.my.org"]

[profiles.client]
cert_type = "client"
```

**Create a certificate using a profile:**

```bash
certmanager create --hostname my.client.com --profile client --auto-confirm
```

### Commands

- `certmanager create`: Create a new self-signed certificate.
- `certmanager create-csr`: Create a Certificate Signing Request (CSR).
- `certmanager sign`: Sign a CSR with a CA.
- `certmanager view`: View details of a certificate.
- `certmanager check-expiry`: Check if a certificate is expiring soon.
- `certmanager renew`: Renew an existing certificate.
- `certmanager export-pfx`: Export a certificate and key to PKCS#12 format.
- `certmanager revoke`: Revoke a certificate.
- `certmanager generate-crl`: Generate a Certificate Revocation List (CRL).

For detailed help on any command, use `--help`, e.g., `certmanager create --help`.
