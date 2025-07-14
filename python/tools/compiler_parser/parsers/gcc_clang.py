"""
GCC and Clang compiler output parser.

This module provides parsing functionality for GCC and Clang compiler outputs.
"""

import re
import logging
from typing import Optional

from ..core.enums import CompilerType, MessageSeverity
from ..core.data_structures import CompilerMessage, CompilerOutput

logger = logging.getLogger(__name__)


class GccClangParser:
    """Parser for GCC and Clang compiler output."""
    
    def __init__(self, compiler_type: CompilerType):
        """Initialize the GCC/Clang parser."""
        self.compiler_type = compiler_type
        self.version_pattern = re.compile(r'(gcc|clang) version (\d+\.\d+\.\d+)')
        self.error_pattern = re.compile(
            r'(?P<file>.*):(?P<line>\d+):(?P<column>\d+):\s*(?P<type>\w+):\s*(?P<message>.+)'
        )
        
    def _extract_version(self, output: str) -> str:
        """Extract GCC/Clang compiler version from output string."""
        if version_match := self.version_pattern.search(output):
            return version_match.group()
        return "unknown"
        
    def parse(self, output: str) -> CompilerOutput:
        """Parse GCC/Clang compiler output."""
        version = self._extract_version(output)
        result = CompilerOutput(compiler=self.compiler_type, version=version)
        
        for match in self.error_pattern.finditer(output):
            try:
                severity = MessageSeverity.from_string(match.group('type').lower())
                
                message = CompilerMessage(
                    file=match.group('file'),
                    line=int(match.group('line')),
                    column=int(match.group('column')),
                    message=match.group('message').strip(),
                    severity=severity
                )
                result.add_message(message)
            except (ValueError, AttributeError) as e:
                logger.warning(f"Skipped invalid message: {e}")
                
        return result
