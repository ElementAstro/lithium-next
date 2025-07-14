"""
Compiler Output Parser (Compatibility Layer)

This module provides compatibility with the original interface while using the new
widget-based architecture. It re-exports the main functionality from the modular
compiler_parser package.

For new code, prefer importing directly from the compiler_parser package.
"""

from __future__ import annotations

import sys
import logging
from typing import Dict, List, Optional, Any

# Import the new widget-based architecture
from .compiler_parser import (
    CompilerType,
    OutputFormat,
    MessageSeverity,
    CompilerMessage,
    CompilerOutput,
    CompilerParserWidget,
    parse_compiler_output,
    parse_compiler_file,
    main_cli
)

# Configure logging
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger(__name__)

# Re-export the classes and functions to maintain backward compatibility
__all__ = [
    'CompilerType',
    'OutputFormat',
    'MessageSeverity',
    'CompilerMessage',
    'CompilerOutput',
    'parse_compiler_output',
    'parse_compiler_file',
    'main'
]

# For backward compatibility, create aliases for the original class names
CompilerOutputParser = CompilerParserWidget
ParserFactory = CompilerParserWidget
WriterFactory = CompilerParserWidget
CompilerOutputProcessor = CompilerParserWidget
ConsoleFormatter = CompilerParserWidget

# Main function for backward compatibility
def main():
    """Main function for command-line operation (backward compatibility)."""
    return main_cli()


if __name__ == "__main__":
    sys.exit(main())
