#!/usr/bin/env python3
"""
Main entry point for running nginx_manager as a Python module.
"""

import sys
from loguru import logger

from .cli import NginxManagerCLI


def main() -> int:
    """
    Main entry point for the command-line application.

    Returns:
        Exit code (0 for success, non-zero for failure)
    """
    cli = NginxManagerCLI()
    return cli.run()


if __name__ == "__main__":
    sys.exit(main())
