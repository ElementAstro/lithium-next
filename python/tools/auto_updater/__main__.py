# __main__.py
"""
Command line entry point for Auto Updater.
This allows running the module as: python -m auto_updater
"""

from .cli import main
import sys

if __name__ == "__main__":
    sys.exit(main())
