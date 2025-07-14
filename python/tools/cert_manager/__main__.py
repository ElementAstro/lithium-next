"""
Allows the package to be run as a script.

Example:
    python -m cert_manager create --hostname my.server.com
"""

from .cert_cli import app

if __name__ == "__main__":
    app()
