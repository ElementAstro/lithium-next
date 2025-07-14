"""
Base parser interface.

This module defines the protocol that all compiler output parsers must implement.
"""

from typing import Protocol
from ..core.data_structures import CompilerOutput


class CompilerOutputParser(Protocol):
    """Protocol defining interface for compiler output parsers."""

    def parse(self, output: str) -> CompilerOutput:
        """Parse the compiler output string into a structured CompilerOutput object."""
        ...
