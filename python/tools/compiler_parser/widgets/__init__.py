"""
Widget modules for compiler parser.

This module provides widgets for processing, formatting, and managing compiler output.
"""

from .formatter import ConsoleFormatterWidget
from .processor import CompilerProcessorWidget
from .main_widget import CompilerParserWidget

__all__ = [
    'ConsoleFormatterWidget',
    'CompilerProcessorWidget',
    'CompilerParserWidget'
]
