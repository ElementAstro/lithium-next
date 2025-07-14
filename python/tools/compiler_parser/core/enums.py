"""
Enums for compiler parser.

This module contains all the enumeration types used throughout the compiler parser system.
"""

from enum import Enum, auto


class CompilerType(Enum):
    """Enumeration of supported compiler types."""

    GCC = auto()
    CLANG = auto()
    MSVC = auto()
    CMAKE = auto()

    @classmethod
    def from_string(cls, compiler_name: str) -> "CompilerType":
        """Convert string compiler name to enum value."""
        name = compiler_name.upper()
        if name in cls.__members__:
            return cls[name]
        raise ValueError(f"Unsupported compiler: {compiler_name}")


class OutputFormat(Enum):
    """Enumeration of supported output formats."""

    JSON = auto()
    CSV = auto()
    XML = auto()

    @classmethod
    def from_string(cls, format_name: str) -> "OutputFormat":
        """Convert string format name to enum value."""
        name = format_name.upper()
        if name in cls.__members__:
            return cls[name]
        raise ValueError(f"Unsupported output format: {format_name}")


class MessageSeverity(Enum):
    """Enumeration of message severity levels."""

    ERROR = "error"
    WARNING = "warning"
    INFO = "info"

    @classmethod
    def from_string(cls, severity: str) -> "MessageSeverity":
        """Convert string severity to enum value."""
        mapping = {"error": cls.ERROR, "warning": cls.WARNING, "info": cls.INFO}
        normalized = severity.lower()
        if normalized in mapping:
            return mapping[normalized]
        # Default to INFO if unknown
        return cls.INFO
