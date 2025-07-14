#!/usr/bin/env python3
"""
Utility functions and classes for Nginx Manager.
"""

from __future__ import annotations

import os
import platform
from enum import Enum
from typing import ClassVar


class OutputColors(Enum):
    """ANSI color codes for terminal output with enhanced features."""

    GREEN = "\033[0;32m"
    RED = "\033[0;31m"
    YELLOW = "\033[0;33m"
    BLUE = "\033[0;34m"
    MAGENTA = "\033[0;35m"
    CYAN = "\033[0;36m"
    WHITE = "\033[0;37m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"
    RESET = "\033[0m"

    # Light variants
    LIGHT_GREEN = "\033[0;92m"
    LIGHT_RED = "\033[0;91m"
    LIGHT_YELLOW = "\033[0;93m"
    LIGHT_BLUE = "\033[0;94m"
    LIGHT_MAGENTA = "\033[0;95m"
    LIGHT_CYAN = "\033[0;96m"

    @classmethod
    def is_color_supported(cls) -> bool:
        global _color_support_cache
        """Check if the current terminal supports colors with caching."""
        if _color_support_cache is None:
            _color_support_cache = cls._check_color_support()
        return _color_support_cache

    @staticmethod
    def _check_color_support() -> bool:
        """Internal method to check color support."""
        # Check for common environment variables
        if any(env_var in os.environ for env_var in ("COLORTERM", "FORCE_COLOR")):
            return True

        # Check TERM environment variable
        term = os.environ.get("TERM", "").lower()
        if any(term_type in term for term_type in ("color", "ansi", "xterm", "screen")):
            return True

        # Windows-specific checks
        if platform.system() == "Windows":
            # Check for Windows 10 version 1607+ (build 14393+) which supports ANSI
            try:
                import sys

                if sys.version_info >= (3, 6):
                    # Modern Windows with ANSI support
                    return True
            except ImportError:
                pass
            return "TERM" in os.environ

        # Unix-like systems
        return os.isatty(1)  # Check if stdout is a TTY

    def format_text(self, text: str, reset: bool = True) -> str:
        """Format text with this color."""
        if not self.is_color_supported():
            return text
        return f"{self.value}{text}{self.RESET.value if reset else ''}"


_color_support_cache: bool | None = None


class OperatingSystem(Enum):
    """Operating system types."""

    LINUX = "Linux"
    WINDOWS = "Windows"
    MACOS = "Darwin"
    OTHER = "Other"

    @classmethod
    def get_current(cls) -> "OperatingSystem":
        os_name = platform.system()
        for os_enum in cls:
            if os_enum.value == os_name:
                return os_enum
        return cls.OTHER

    def is_linux(self) -> bool:
        return self == OperatingSystem.LINUX

    def is_windows(self) -> bool:
        return self == OperatingSystem.WINDOWS

    def is_macos(self) -> bool:
        return self == OperatingSystem.MACOS
