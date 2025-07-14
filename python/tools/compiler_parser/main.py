"""
Main entry point for the compiler parser.

This module provides the main function for command-line usage and can be run as a script.
"""

import sys
import logging
from .utils.cli import main_cli

# Configure logging
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
)


def main():
    """Main entry point for the compiler parser."""
    return main_cli()


if __name__ == "__main__":
    sys.exit(main())
