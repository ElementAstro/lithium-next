"""
MSVC compiler output parser.

This module provides parsing functionality for Microsoft Visual C++ compiler output.
"""

import re
import logging
from typing import Optional

from ..core.enums import CompilerType, MessageSeverity
from ..core.data_structures import CompilerMessage, CompilerOutput

logger = logging.getLogger(__name__)


class MsvcParser:
    """Parser for Microsoft Visual C++ compiler output."""
    
    def __init__(self):
        """Initialize the MSVC parser."""
        self.compiler_type = CompilerType.MSVC
        self.version_pattern = re.compile(r'Compiler Version (\d+\.\d+\.\d+\.\d+)')
        self.error_pattern = re.compile(
            r'(?P<file>.*)$(?P<line>\d+)$:\s*(?P<type>\w+)\s*(?P<code>\w+\d+):\s*(?P<message>.+)'
        )
        
    def _extract_version(self, output: str) -> str:
        """Extract MSVC compiler version from output string."""
        if version_match := self.version_pattern.search(output):
            return version_match.group()
        return "unknown"
        
    def parse(self, output: str) -> CompilerOutput:
        """Parse MSVC compiler output."""
        version = self._extract_version(output)
        result = CompilerOutput(compiler=self.compiler_type, version=version)
        
        for match in self.error_pattern.finditer(output):
            try:
                severity = MessageSeverity.from_string(match.group('type').lower())
                
                message = CompilerMessage(
                    file=match.group('file'),
                    line=int(match.group('line')),
                    message=match.group('message').strip(),
                    severity=severity,
                    code=match.group('code')
                )
                result.add_message(message)
            except (ValueError, AttributeError) as e:
                logger.warning(f"Skipped invalid message: {e}")
                
        return result
