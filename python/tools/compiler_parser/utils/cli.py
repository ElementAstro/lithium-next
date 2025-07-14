"""
Command-line interface utilities.

This module provides CLI argument parsing and main function for command-line operation.
"""

import argparse
import logging
import sys
from pathlib import Path
from typing import Optional

from ..core.enums import MessageSeverity
from ..widgets.main_widget import CompilerParserWidget

logger = logging.getLogger(__name__)


def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Parse compiler output and convert to various formats."
    )

    parser.add_argument(
        "compiler",
        choices=["gcc", "clang", "msvc", "cmake"],
        help="The compiler used for the output.",
    )

    parser.add_argument(
        "file_paths", nargs="+", help="Paths to the compiler output files."
    )

    parser.add_argument(
        "--output-format",
        choices=["json", "csv", "xml"],
        default="json",
        help="Output format (default: json).",
    )

    parser.add_argument(
        "--output-file",
        default="compiler_output",
        help="Base name for the output file without extension (default: compiler_output).",
    )

    parser.add_argument(
        "--output-dir",
        default=".",
        help="Directory for output files (default: current directory).",
    )

    parser.add_argument(
        "--filter",
        nargs="*",
        choices=["error", "warning", "info"],
        help="Filter by message severity types.",
    )

    parser.add_argument(
        "--file-pattern", help="Regular expression to filter files by name."
    )

    parser.add_argument(
        "--stats", action="store_true", help="Include statistics in the output."
    )

    parser.add_argument(
        "--verbose", action="store_true", help="Enable verbose logging output."
    )

    parser.add_argument(
        "--concurrency",
        type=int,
        default=4,
        help="Number of concurrent threads for processing files (default: 4).",
    )

    parser.add_argument(
        "--no-color", action="store_true", help="Disable colorized output."
    )

    return parser.parse_args()


def main_cli():
    """Main function for command-line operation."""
    args = parse_args()

    # Configure logging based on verbosity
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    else:
        logging.getLogger().setLevel(logging.INFO)

    logger.info(f"Starting compiler output processing with {args.compiler}")

    # Create output directory if it doesn't exist
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    # Determine file extension based on output format
    extension = args.output_format.lower()
    output_path = output_dir / f"{args.output_file}.{extension}"

    # Create main widget
    widget = CompilerParserWidget()

    try:
        # Process and export
        result = widget.process_and_export(
            compiler_type=args.compiler,
            input_files=args.file_paths,
            output_format=args.output_format,
            output_path=output_path,
            filter_severities=args.filter,
            file_pattern=args.file_pattern,
            concurrency=args.concurrency,
            display_stats=args.stats,
            display_output=not args.no_color,
        )

        print(f"\nOutput saved to: {output_path}")

        if result.messages:
            print(f"Processed {len(result.messages)} messages successfully.")
        else:
            print("No compiler messages found or all messages were filtered out.")

    except Exception as e:
        logger.error(f"Error processing compiler output: {e}")
        return 1

    return 0
