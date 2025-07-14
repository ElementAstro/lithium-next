"""
Parser factory for creating appropriate parser instances.

This module provides a factory for creating compiler output parsers based on
the compiler type.
"""

from typing import Union

from ..core.enums import CompilerType
from .base import CompilerOutputParser
from .gcc_clang import GccClangParser
from .msvc import MsvcParser
from .cmake import CMakeParser


class ParserFactory:
    """Factory for creating appropriate compiler output parser instances."""
    
    @staticmethod
    def create_parser(compiler_type: Union[CompilerType, str]) -> CompilerOutputParser:
        """Create and return the appropriate parser for the given compiler type."""
        if isinstance(compiler_type, str):
            compiler_type = CompilerType.from_string(compiler_type)
            
        match compiler_type:
            case CompilerType.GCC:
                return GccClangParser(CompilerType.GCC)
            case CompilerType.CLANG:
                return GccClangParser(CompilerType.CLANG)
            case CompilerType.MSVC:
                return MsvcParser()
            case CompilerType.CMAKE:
                return CMakeParser()
            case _:
                raise ValueError(f"Unsupported compiler type: {compiler_type}")
