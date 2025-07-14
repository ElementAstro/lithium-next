"""
CSV output writer.

This module provides functionality to write compiler output to CSV format.
"""

import csv
import logging
from pathlib import Path

from ..core.data_structures import CompilerOutput

logger = logging.getLogger(__name__)


class CsvWriter:
    """Writer for CSV output format."""

    def write(self, compiler_output: CompilerOutput, output_path: Path) -> None:
        """Write compiler output to a CSV file."""
        # Prepare flattened data for CSV export
        data = []
        for msg in compiler_output.messages:
            msg_dict = msg.to_dict()
            # Add columns that might not be present in all messages with None values
            msg_dict.setdefault("column", None)
            msg_dict.setdefault("code", None)
            data.append(msg_dict)

        fieldnames = ["file", "line", "column", "severity", "code", "message"]

        with output_path.open("w", newline="", encoding="utf-8") as csvfile:
            writer = csv.DictWriter(
                csvfile, fieldnames=fieldnames, extrasaction="ignore"
            )
            writer.writeheader()
            writer.writerows(data)
        logger.info(f"CSV output written to {output_path}")
