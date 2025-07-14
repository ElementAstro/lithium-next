"""
JSON output writer.

This module provides functionality to write compiler output to JSON format.
"""

import json
import logging
from pathlib import Path

from ..core.data_structures import CompilerOutput

logger = logging.getLogger(__name__)


class JsonWriter:
    """Writer for JSON output format."""

    def write(self, compiler_output: CompilerOutput, output_path: Path) -> None:
        """Write compiler output to a JSON file."""
        data = compiler_output.to_dict()
        with output_path.open("w", encoding="utf-8") as json_file:
            json.dump(data, json_file, indent=2)
        logger.info(f"JSON output written to {output_path}")
