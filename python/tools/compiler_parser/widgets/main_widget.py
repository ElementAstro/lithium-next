"""
Main compiler parser widget.

This module provides the main widget that orchestrates the entire compiler parsing process,
integrating all the sub-widgets for a complete solution.
"""

import json
import logging
from pathlib import Path
from typing import Dict, List, Optional, Union, Any

from ..core.enums import CompilerType, OutputFormat, MessageSeverity
from ..core.data_structures import CompilerOutput
from ..writers.factory import WriterFactory
from .processor import CompilerProcessorWidget
from .formatter import ConsoleFormatterWidget

logger = logging.getLogger(__name__)


class CompilerParserWidget:
    """Main widget for orchestrating compiler output parsing and processing."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """Initialize the main compiler parser widget."""
        self.config = config or {}
        self.processor = CompilerProcessorWidget(config)
        self.formatter = ConsoleFormatterWidget()

    def parse_from_string(
        self,
        compiler_type: Union[CompilerType, str],
        output: str,
        filter_severities: Optional[List[Union[MessageSeverity, str]]] = None,
        file_pattern: Optional[str] = None,
    ) -> CompilerOutput:
        """Parse compiler output from a string."""
        # Process the string
        compiler_output = self.processor.process_string(compiler_type, output)

        # Apply filters if specified
        if filter_severities or file_pattern:
            # Convert string severities to enum values
            severities = None
            if filter_severities:
                severities = []
                for sev in filter_severities:
                    if isinstance(sev, str):
                        severities.append(MessageSeverity.from_string(sev))
                    else:
                        severities.append(sev)

            compiler_output = self.processor.filter_messages(
                compiler_output, severities=severities, file_pattern=file_pattern
            )

        return compiler_output

    def parse_from_file(
        self,
        compiler_type: Union[CompilerType, str],
        file_path: Union[str, Path],
        filter_severities: Optional[List[Union[MessageSeverity, str]]] = None,
        file_pattern: Optional[str] = None,
    ) -> CompilerOutput:
        """Parse compiler output from a file."""
        # Process the file
        compiler_output = self.processor.process_file(compiler_type, file_path)

        # Apply filters if specified
        if filter_severities or file_pattern:
            # Convert string severities to enum values
            severities = None
            if filter_severities:
                severities = []
                for sev in filter_severities:
                    if isinstance(sev, str):
                        severities.append(MessageSeverity.from_string(sev))
                    else:
                        severities.append(sev)

            compiler_output = self.processor.filter_messages(
                compiler_output, severities=severities, file_pattern=file_pattern
            )

        return compiler_output

    def parse_from_files(
        self,
        compiler_type: Union[CompilerType, str],
        file_paths: List[Union[str, Path]],
        filter_severities: Optional[List[Union[MessageSeverity, str]]] = None,
        file_pattern: Optional[str] = None,
        concurrency: int = 4,
        combine_outputs: bool = True,
    ) -> Union[CompilerOutput, List[CompilerOutput]]:
        """Parse compiler output from multiple files."""
        # Process all files
        compiler_outputs = self.processor.process_files(
            compiler_type, file_paths, concurrency
        )

        # Apply filters if specified
        if filter_severities or file_pattern:
            # Convert string severities to enum values
            severities = None
            if filter_severities:
                severities = []
                for sev in filter_severities:
                    if isinstance(sev, str):
                        severities.append(MessageSeverity.from_string(sev))
                    else:
                        severities.append(sev)

            filtered_outputs = []
            for output in compiler_outputs:
                filtered = self.processor.filter_messages(
                    output, severities=severities, file_pattern=file_pattern
                )
                filtered_outputs.append(filtered)

            compiler_outputs = filtered_outputs

        # Combine outputs if requested
        if combine_outputs:
            combined = self.processor.combine_outputs(compiler_outputs)
            return (
                combined
                if combined
                else CompilerOutput(
                    compiler=(
                        CompilerType.from_string(compiler_type)
                        if isinstance(compiler_type, str)
                        else compiler_type
                    ),
                    version="unknown",
                )
            )

        return compiler_outputs

    def write_output(
        self,
        compiler_output: CompilerOutput,
        output_format: Union[OutputFormat, str],
        output_path: Union[str, Path],
    ) -> None:
        """Write compiler output to a file in the specified format."""
        # Ensure output_path is a Path object
        output_path = Path(output_path)

        # Create writer and write output
        writer = WriterFactory.create_writer(output_format)
        writer.write(compiler_output, output_path)

    def display_output(
        self, compiler_output: CompilerOutput, colorize: bool = True
    ) -> None:
        """Display compiler output to console."""
        if colorize:
            self.formatter.colorize_output(compiler_output)
        else:
            print(self.formatter.get_formatted_output(compiler_output))

    def generate_statistics(
        self, compiler_outputs: List[CompilerOutput]
    ) -> Dict[str, Any]:
        """Generate statistics from compiler outputs."""
        return self.processor.generate_statistics(compiler_outputs)

    def process_and_export(
        self,
        compiler_type: Union[CompilerType, str],
        input_files: List[Union[str, Path]],
        output_format: Union[OutputFormat, str],
        output_path: Union[str, Path],
        filter_severities: Optional[List[Union[MessageSeverity, str]]] = None,
        file_pattern: Optional[str] = None,
        concurrency: int = 4,
        display_stats: bool = False,
        display_output: bool = False,
    ) -> CompilerOutput:
        """Complete processing pipeline: parse, filter, combine, and export."""
        # Parse from files
        combined_output = self.parse_from_files(
            compiler_type,
            input_files,
            filter_severities,
            file_pattern,
            concurrency,
            combine_outputs=True,
        )

        # Ensure we have a valid output
        if not isinstance(combined_output, CompilerOutput):
            raise ValueError("Failed to process input files")

        # Write output
        self.write_output(combined_output, output_format, output_path)

        # Display statistics if requested
        if display_stats:
            # For stats, we need the individual outputs
            individual_outputs = self.parse_from_files(
                compiler_type,
                input_files,
                filter_severities,
                file_pattern,
                concurrency,
                combine_outputs=False,
            )

            if isinstance(individual_outputs, list):
                stats = self.generate_statistics(individual_outputs)
                print("\nStatistics:")
                print(json.dumps(stats, indent=4))

        # Display output if requested
        if display_output:
            self.display_output(combined_output)

        return combined_output
