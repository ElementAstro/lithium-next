"""
Writer factory for creating appropriate writer instances.

This module provides a factory for creating output writers based on the format type.
"""

from typing import Union

from ..core.enums import OutputFormat
from .base import OutputWriter
from .json_writer import JsonWriter
from .csv_writer import CsvWriter
from .xml_writer import XmlWriter


class WriterFactory:
    """Factory for creating appropriate output writer instances."""

    @staticmethod
    def create_writer(format_type: Union[OutputFormat, str]) -> OutputWriter:
        """Create and return the appropriate writer for the given output format."""
        if isinstance(format_type, str):
            format_type = OutputFormat.from_string(format_type)

        match format_type:
            case OutputFormat.JSON:
                return JsonWriter()
            case OutputFormat.CSV:
                return CsvWriter()
            case OutputFormat.XML:
                return XmlWriter()
            case _:
                raise ValueError(f"Unsupported output format: {format_type}")
