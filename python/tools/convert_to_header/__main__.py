#!/usr/bin/env python3
"""
Allows the package to be run as a script.
Example: python -m convert_to_header to-header input.bin
"""
from .cli import main

if __name__ == "__main__":
    main()
