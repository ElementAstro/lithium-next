#!/usr/bin/env python3
"""
Utility functions and classes for Nginx Manager.
"""

import os
import platform


class OutputColors:
    """ANSI color codes for terminal output."""
    GREEN = '\033[0;32m'
    RED = '\033[0;31m'
    YELLOW = '\033[0;33m'
    BLUE = '\033[0;34m'
    MAGENTA = '\033[0;35m'
    CYAN = '\033[0;36m'
    RESET = '\033[0m'

    @staticmethod
    def is_color_supported() -> bool:
        """Check if the current terminal supports colors."""
        return platform.system() != "Windows" or "TERM" in os.environ
