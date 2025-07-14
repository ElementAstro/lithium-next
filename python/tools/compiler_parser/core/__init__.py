"""
Core module for compiler parser.

This module contains the fundamental data structures and enums used throughout
the compiler parser system.
"""

from .enums import CompilerType, OutputFormat, MessageSeverity
from .data_structures import CompilerMessage, CompilerOutput

__all__ = [
    "CompilerType",
    "OutputFormat",
    "MessageSeverity",
    "CompilerMessage",
    "CompilerOutput",
]
