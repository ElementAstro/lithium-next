"""
Base writer interface.

This module defines the protocol that all output writers must implement.
"""

from typing import Protocol
from pathlib import Path
from ..core.data_structures import CompilerOutput


class OutputWriter(Protocol):
    """Protocol defining interface for output writers."""
    
    def write(self, compiler_output: CompilerOutput, output_path: Path) -> None:
        """Write the compiler output to the specified path."""
        ...
