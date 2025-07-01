#!/usr/bin/env python3
"""
Main entry point for running the async nginx_manager as a Python module.
"""

import sys
from .cli import main

if __name__ == "__main__":
    sys.exit(main())
