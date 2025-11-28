"""
Compiler Output Parser

This module provides functionality to parse, analyze, and convert compiler outputs from
various compilers (GCC, Clang, MSVC, CMake) into structured formats like JSON, CSV, or XML.
It supports both command-line usage and programmatic integration through pybind11.

Features:
- Multi-compiler support (GCC, Clang, MSVC, CMake)
- Multiple output formats (JSON, CSV, XML)
- Concurrent file processing
- Detailed statistics and filtering capabilities
"""

from __future__ import annotations

import re
import json
import csv
import argparse
import logging
from enum import Enum, auto
from dataclasses import dataclass, field, asdict
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Dict, List, Optional, Union, Any, Literal, TypeVar, Protocol
import xml.etree.ElementTree as ET
from termcolor import colored
import sys
from functools import partial


# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger(__name__)


class CompilerType(Enum):
    """Enumeration of supported compiler types."""
    GCC = auto()
    CLANG = auto()
    MSVC = auto()
    CMAKE = auto()
    
    @classmethod
    def from_string(cls, compiler_name: str) -> CompilerType:
        """Convert string compiler name to enum value."""
        name = compiler_name.upper()
        if name in cls.__members__:
            return cls[name]
        raise ValueError(f"Unsupported compiler: {compiler_name}")


class OutputFormat(Enum):
    """Enumeration of supported output formats."""
    JSON = auto()
    CSV = auto()
    XML = auto()
    
    @classmethod
    def from_string(cls, format_name: str) -> OutputFormat:
        """Convert string format name to enum value."""
        name = format_name.upper()
        if name in cls.__members__:
            return cls[name]
        raise ValueError(f"Unsupported output format: {format_name}")


class MessageSeverity(Enum):
    """Enumeration of message severity levels."""
    ERROR = "error"
    WARNING = "warning"
    INFO = "info"
    
    @classmethod
    def from_string(cls, severity: str) -> MessageSeverity:
        """Convert string severity to enum value."""
        mapping = {
            "error": cls.ERROR,
            "warning": cls.WARNING,
            "info": cls.INFO
        }
        normalized = severity.lower()
        if normalized in mapping:
            return mapping[normalized]
        # Default to INFO if unknown
        return cls.INFO


@dataclass
class CompilerMessage:
    """Data class representing a compiler message (error, warning, or info)."""
    file: str
    line: int
    message: str
    severity: MessageSeverity
    column: Optional[int] = None
    code: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert the CompilerMessage to a dictionary."""
        result = {
            "file": self.file,
            "line": self.line,
            "message": self.message,
            "severity": self.severity.value,
        }
        if self.column is not None:
            result["column"] = self.column
        if self.code is not None:
            result["code"] = self.code
        return result


@dataclass
class CompilerOutput:
    """Data class representing the structured output from a compiler."""
    compiler: CompilerType
    version: str
    messages: List[CompilerMessage] = field(default_factory=list)
    
    def add_message(self, message: CompilerMessage) -> None:
        """Add a message to the compiler output."""
        self.messages.append(message)
        
    def get_messages_by_severity(self, severity: MessageSeverity) -> List[CompilerMessage]:
        """Get all messages with the specified severity."""
        return [msg for msg in self.messages if msg.severity == severity]
    
    @property
    def errors(self) -> List[CompilerMessage]:
        """Get all error messages."""
        return self.get_messages_by_severity(MessageSeverity.ERROR)
    
    @property
    def warnings(self) -> List[CompilerMessage]:
        """Get all warning messages."""
        return self.get_messages_by_severity(MessageSeverity.WARNING)
    
    @property
    def infos(self) -> List[CompilerMessage]:
        """Get all info messages."""
        return self.get_messages_by_severity(MessageSeverity.INFO)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert the CompilerOutput to a dictionary."""
        return {
            "compiler": self.compiler.name,
            "version": self.version,
            "messages": [msg.to_dict() for msg in self.messages]
        }


class CompilerOutputParser(Protocol):
    """Protocol defining interface for compiler output parsers."""
    
    def parse(self, output: str) -> CompilerOutput:
        """Parse the compiler output string into a structured CompilerOutput object."""
        ...


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


class CMakeParser:
    """Parser for CMake build system output."""
    
    def __init__(self):
        """Initialize the CMake parser."""
        self.compiler_type = CompilerType.CMAKE
        self.version_pattern = re.compile(r'cmake version (\d+\.\d+\.\d+)')
        self.error_pattern = re.compile(
            r'(?P<file>.*):(?P<line>\d+):(?P<type>\w+):\s*(?P<message>.+)'
        )
        
    def _extract_version(self, output: str) -> str:
        """Extract CMake version from output string."""
        if version_match := self.version_pattern.search(output):
            return version_match.group()
        return "unknown"
        
    def parse(self, output: str) -> CompilerOutput:
        """Parse CMake build system output."""
        version = self._extract_version(output)
        result = CompilerOutput(compiler=self.compiler_type, version=version)
        
        for match in self.error_pattern.finditer(output):
            try:
                severity = MessageSeverity.from_string(match.group('type').lower())
                
                message = CompilerMessage(
                    file=match.group('file'),
                    line=int(match.group('line')),
                    message=match.group('message').strip(),
                    severity=severity
                )
                result.add_message(message)
            except (ValueError, AttributeError) as e:
                logger.warning(f"Skipped invalid message: {e}")
                
        return result


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


class OutputWriter(Protocol):
    """Protocol defining interface for output writers."""
    
    def write(self, compiler_output: CompilerOutput, output_path: Path) -> None:
        """Write the compiler output to the specified path."""
        ...


class JsonWriter:
    """Writer for JSON output format."""
    
    def write(self, compiler_output: CompilerOutput, output_path: Path) -> None:
        """Write compiler output to a JSON file."""
        data = compiler_output.to_dict()
        with output_path.open('w', encoding="utf-8") as json_file:
            json.dump(data, json_file, indent=2)
        logger.info(f"JSON output written to {output_path}")


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
            
        fieldnames = ['file', 'line', 'column', 'severity', 'code', 'message']
        
        with output_path.open('w', newline='', encoding="utf-8") as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames, extrasaction='ignore')
            writer.writeheader()
            writer.writerows(data)
        logger.info(f"CSV output written to {output_path}")


class XmlWriter:
    """Writer for XML output format."""
    
    def write(self, compiler_output: CompilerOutput, output_path: Path) -> None:
        """Write compiler output to an XML file."""
        root = ET.Element("CompilerOutput")
        # Add metadata
        metadata = ET.SubElement(root, "Metadata")
        ET.SubElement(metadata, "Compiler").text = compiler_output.compiler.name
        ET.SubElement(metadata, "Version").text = compiler_output.version
        ET.SubElement(metadata, "MessageCount").text = str(len(compiler_output.messages))
        
        # Add messages
        messages_elem = ET.SubElement(root, "Messages")
        for msg in compiler_output.messages:
            msg_elem = ET.SubElement(messages_elem, "Message")
            for key, value in msg.to_dict().items():
                if value is not None:  # Skip None values
                    ET.SubElement(msg_elem, key).text = str(value)
                    
        # Write XML to file
        tree = ET.ElementTree(root)
        tree.write(output_path, encoding="utf-8", xml_declaration=True)
        logger.info(f"XML output written to {output_path}")


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


class ConsoleFormatter:
    """Class for formatting compiler output for console display."""
    
    @staticmethod
    def colorize_output(compiler_output: CompilerOutput) -> None:
        """Print compiler output with colorized formatting based on message severity."""
        print("\nCompiler Output Summary:")
        print(f"Compiler: {compiler_output.compiler.name}")
        print(f"Version: {compiler_output.version}")
        print(f"Total Messages: {len(compiler_output.messages)}")
        print(f"Errors: {len(compiler_output.errors)}")
        print(f"Warnings: {len(compiler_output.warnings)}")
        print(f"Info: {len(compiler_output.infos)}")
        print("\nMessages:")
        
        for msg in compiler_output.messages:
            match msg.severity:
                case MessageSeverity.ERROR:
                    color = 'red'
                    prefix = "ERROR"
                case MessageSeverity.WARNING:
                    color = 'yellow'
                    prefix = "WARNING"
                case MessageSeverity.INFO:
                    color = 'blue'
                    prefix = "INFO"
                case _:
                    color = 'white'
                    prefix = "UNKNOWN"
                    
            location = f"{msg.file}:{msg.line}"
            if msg.column is not None:
                location += f":{msg.column}"
                
            code_info = f" [{msg.code}]" if msg.code else ""
            
            message = f"{prefix}: {location}{code_info} - {msg.message}"
            print(colored(message, color))


class CompilerOutputProcessor:
    """Main class for processing compiler output files."""
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """Initialize the processor with optional configuration."""
        self.config = config or {}
        
    def process_file(self, compiler_type: Union[CompilerType, str], file_path: Path) -> CompilerOutput:
        """Process a single file containing compiler output."""
        # Ensure file_path is a Path object
        file_path = Path(file_path)
        
        logger.info(f"Processing file: {file_path}")
        
        # Create parser based on compiler type
        parser = ParserFactory.create_parser(compiler_type)
        
        # Read and parse the file
        try:
            with file_path.open('r', encoding="utf-8") as file:
                output = file.read()
            return parser.parse(output)
        except FileNotFoundError:
            logger.error(f"File not found: {file_path}")
            raise
        except Exception as e:
            logger.error(f"Error processing file {file_path}: {e}")
            raise
    
    def process_files(
        self, 
        compiler_type: Union[CompilerType, str],
        file_paths: List[Union[str, Path]],
        concurrency: int = 4
    ) -> List[CompilerOutput]:
        """Process multiple files concurrently and return all compiler outputs."""
        results = []
        
        # Convert strings to Path objects
        file_paths = [Path(p) for p in file_paths]
        
        # Use ThreadPoolExecutor for concurrent processing
        with ThreadPoolExecutor(max_workers=concurrency) as executor:
            # Create a partial function with the compiler type
            process_func = partial(self.process_file, compiler_type)
            
            # Submit all file processing tasks
            futures = {executor.submit(process_func, file_path): file_path 
                      for file_path in file_paths}
            
            # Collect results as they complete
            for future in as_completed(futures):
                file_path = futures[future]
                try:
                    result = future.result()
                    results.append(result)
                    logger.info(f"Successfully processed {file_path}")
                except Exception as e:
                    logger.error(f"Failed to process {file_path}: {e}")
                    
        return results
    
    def filter_messages(
        self,
        compiler_output: CompilerOutput,
        severities: Optional[List[MessageSeverity]] = None,
        file_pattern: Optional[str] = None
    ) -> CompilerOutput:
        """Filter messages by severity and/or file pattern."""
        if not severities and not file_pattern:
            return compiler_output
            
        # Create a new output with the same metadata
        filtered = CompilerOutput(
            compiler=compiler_output.compiler,
            version=compiler_output.version
        )
        
        # Filter messages based on criteria
        for msg in compiler_output.messages:
            # Check severity filter
            severity_match = not severities or msg.severity in severities
            
            # Check file pattern filter
            file_match = not file_pattern or re.search(file_pattern, msg.file)
            
            # Add message if it matches all filters
            if severity_match and file_match:
                filtered.add_message(msg)
                
        return filtered
    
    def generate_statistics(self, compiler_outputs: List[CompilerOutput]) -> Dict[str, Any]:
        """Generate statistics from a list of compiler outputs."""
        stats = {
            "total_files": len(compiler_outputs),
            "total_messages": 0,
            "by_severity": {
                "error": 0,
                "warning": 0,
                "info": 0
            },
            "by_compiler": {},
            "files_with_errors": 0
        }
        
        for output in compiler_outputs:
            # Count messages by severity
            errors = len(output.errors)
            warnings = len(output.warnings)
            infos = len(output.infos)
            
            # Update counts
            stats["total_messages"] += errors + warnings + infos
            stats["by_severity"]["error"] += errors
            stats["by_severity"]["warning"] += warnings
            stats["by_severity"]["info"] += infos
            
            # Count files with errors
            if errors > 0:
                stats["files_with_errors"] += 1
                
            # Count by compiler
            compiler_name = output.compiler.name
            if compiler_name not in stats["by_compiler"]:
                stats["by_compiler"][compiler_name] = 0
            stats["by_compiler"][compiler_name] += 1
            
        return stats


# pybind11 exports - These functions can be called from C++
def parse_compiler_output(
    compiler_type: str,
    output: str,
    filter_severities: Optional[List[str]] = None
) -> Dict[str, Any]:
    """
    Parse compiler output and return structured data.
    
    This function is designed to be exported through pybind11 for use in C++ applications.
    
    Args:
        compiler_type: String identifier for the compiler (gcc, clang, msvc, cmake)
        output: The raw compiler output string to parse
        filter_severities: Optional list of severities to include (error, warning, info)
        
    Returns:
        Dictionary with parsed compiler output
    """
    parser = ParserFactory.create_parser(compiler_type)
    compiler_output = parser.parse(output)
    
    # Apply filters if specified
    if filter_severities:
        severities = [MessageSeverity.from_string(sev) for sev in filter_severities]
        processor = CompilerOutputProcessor()
        compiler_output = processor.filter_messages(compiler_output, severities=severities)
    
    return compiler_output.to_dict()


def parse_compiler_file(
    compiler_type: str,
    file_path: str,
    filter_severities: Optional[List[str]] = None
) -> Dict[str, Any]:
    """
    Parse compiler output from a file and return structured data.
    
    This function is designed to be exported through pybind11 for use in C++ applications.
    
    Args:
        compiler_type: String identifier for the compiler (gcc, clang, msvc, cmake)
        file_path: Path to the file containing compiler output
        filter_severities: Optional list of severities to include (error, warning, info)
        
    Returns:
        Dictionary with parsed compiler output
    """
    processor = CompilerOutputProcessor()
    compiler_output = processor.process_file(compiler_type, Path(file_path))
    
    # Apply filters if specified
    if filter_severities:
        severities = [MessageSeverity.from_string(sev) for sev in filter_severities]
        compiler_output = processor.filter_messages(compiler_output, severities=severities)
    
    return compiler_output.to_dict()


# CLI functions
def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Parse compiler output and convert to various formats."
    )
    
    parser.add_argument(
        'compiler',
        choices=['gcc', 'clang', 'msvc', 'cmake'],
        help="The compiler used for the output."
    )
    
    parser.add_argument(
        'file_paths',
        nargs='+',
        help="Paths to the compiler output files."
    )
    
    parser.add_argument(
        '--output-format',
        choices=['json', 'csv', 'xml'],
        default='json',
        help="Output format (default: json)."
    )
    
    parser.add_argument(
        '--output-file',
        default='compiler_output',
        help="Base name for the output file without extension (default: compiler_output)."
    )
    
    parser.add_argument(
        '--output-dir',
        default='.',
        help="Directory for output files (default: current directory)."
    )
    
    parser.add_argument(
        '--filter',
        nargs='*',
        choices=['error', 'warning', 'info'],
        help="Filter by message severity types."
    )
    
    parser.add_argument(
        '--file-pattern',
        help="Regular expression to filter files by name."
    )
    
    parser.add_argument(
        '--stats',
        action='store_true',
        help="Include statistics in the output."
    )
    
    parser.add_argument(
        '--verbose',
        action='store_true',
        help="Enable verbose logging output."
    )
    
    parser.add_argument(
        '--concurrency',
        type=int,
        default=4,
        help="Number of concurrent threads for processing files (default: 4)."
    )
    
    return parser.parse_args()


def main():
    """Main function for command-line operation."""
    args = parse_args()
    
    # Configure logging based on verbosity
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    logger.info(f"Starting compiler output processing with {args.compiler}")
    
    # Create output directory if it doesn't exist
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Process files
    processor = CompilerOutputProcessor()
    compiler_outputs = processor.process_files(
        args.compiler,
        args.file_paths,
        args.concurrency
    )
    
    # Apply filters if specified
    if args.filter or args.file_pattern:
        filtered_outputs = []
        severities = [MessageSeverity.from_string(sev) for sev in (args.filter or [])]
        
        for output in compiler_outputs:
            filtered = processor.filter_messages(
                output,
                severities=severities,
                file_pattern=args.file_pattern
            )
            filtered_outputs.append(filtered)
        
        compiler_outputs = filtered_outputs
        
    # Prepare combined output
    combined_output = None
    if compiler_outputs:
        # Use the first compiler type and version for the combined output
        combined_output = CompilerOutput(
            compiler=compiler_outputs[0].compiler,
            version=compiler_outputs[0].version
        )
        
        # Add all messages from all outputs
        for output in compiler_outputs:
            for msg in output.messages:
                combined_output.add_message(msg)
    
    # Generate and display statistics if requested
    if args.stats and compiler_outputs:
        stats = processor.generate_statistics(compiler_outputs)
        print("\nStatistics:")
        print(json.dumps(stats, indent=4))
    
    # Write output to specified format if we have results
    if combined_output:
        # Determine file extension based on output format
        extension = args.output_format.lower()
        output_path = output_dir / f"{args.output_file}.{extension}"
        
        # Create writer and write output
        writer = WriterFactory.create_writer(args.output_format)
        writer.write(combined_output, output_path)
        
        print(f"\nOutput saved to: {output_path}")
        
        # Display colorized console output
        ConsoleFormatter.colorize_output(combined_output)
    else:
        print("\nNo compiler messages found or all messages were filtered out.")

    return 0


# Module metadata
__version__ = "1.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"


def get_tool_info() -> Dict[str, Any]:
    """
    Return metadata about this tool for discovery by PythonWrapper.

    This function follows the Lithium-Next Python tool discovery convention,
    allowing the C++ PythonWrapper to introspect and catalog this module's
    capabilities.

    Returns:
        Dict containing tool metadata including name, version, description,
        available functions, requirements, and platform compatibility.
    """
    return {
        "name": "compiler_parser",
        "version": __version__,
        "description": "Parse and analyze compiler output from GCC, Clang, MSVC, CMake",
        "author": __author__,
        "license": __license__,
        "supported": True,
        "platform": ["windows", "linux", "macos"],
        "functions": [
            "parse_output",
            "filter_messages",
            "get_statistics",
            "write_json",
            "write_csv",
            "write_xml",
            "process_file",
            "process_files",
        ],
        "requirements": ["termcolor"],
        "capabilities": [
            "gcc_parsing",
            "clang_parsing",
            "msvc_parsing",
            "cmake_parsing",
            "json_output",
            "csv_output",
            "xml_output",
            "concurrent_processing",
            "message_filtering",
            "statistics_generation",
        ],
        "classes": {
            "CompilerType": "Enum of supported compiler types",
            "OutputFormat": "Enum of supported output formats",
            "MessageSeverity": "Enum of message severity levels",
            "CompilerMessage": "Individual compiler message data",
            "CompilerOutput": "Collection of compiler messages",
            "CompilerOutputProcessor": "Main processor class",
        }
    }


if __name__ == "__main__":
    sys.exit(main())
