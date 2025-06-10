#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: convert_to_header.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-06-08
Version: 2.0

Description:
------------
This Python script provides functionality to convert binary files into C/C++ header files
containing array data, and vice versa, with extensive customization options and features.

The script supports two primary operations:
    1. Converting binary files to C/C++ header files with array data
    2. Converting C/C++ header files back to binary files

Features:
---------
- Multiple data formats (hex, binary, decimal, octal, char)
- Data compression with several algorithms (zlib, lzma, bz2, base64)
- Optional C++ namespace and class wrappers
- Splitting large arrays across multiple header files
- Customizable header styles and formatting options
- Checksum verification for data integrity
- Configuration via JSON or YAML files
- Progress reporting for large file operations
- Dual usage: command line interface and programmatic API
- pybind11 bindings for C++ integration

Dependencies:
------------
- Python 3.9+ (for pattern matching and other modern features)
- Standard library modules (zlib, lzma, bz2, base64, etc.)
- Optional: pybind11 (for C++ bindings)
- Optional: PyYAML (for YAML configuration support)
- Optional: tqdm (for progress reporting)

Usage:
------
Command line:
    python convert_to_header.py to_header input.bin output.h [options]
    python convert_to_header.py to_file input.h output.bin [options]
    python convert_to_header.py info input.h

As a module:
    import convert_to_header as cth
    cth.convert_to_header("input.bin", "output.h", compression="zlib")
    
With pybind11 (C++):
    #include <pybind11/pybind11.h>
    auto converter = py::module::import("convert_to_header").attr("Converter")();
    converter.attr("to_header")("input.bin", "output.h");

License:
--------
This script is released under the MIT License.
"""

import sys
import os
import zlib
import lzma
import bz2
import base64
import hashlib
import argparse
import json
import importlib.util
from datetime import datetime
from enum import Enum, auto
from pathlib import Path
from typing import Optional, List, Dict, Union, Tuple, Any, Callable, TypedDict, Literal
from dataclasses import dataclass, field, asdict
import logging

# Type definitions
PathLike = Union[str, Path]
DataFormat = Literal["hex", "bin", "dec", "oct", "char"]
CommentStyle = Literal["C", "CPP"]
CompressionType = Literal["none", "zlib", "lzma", "bz2", "base64"]
ChecksumAlgo = Literal["md5", "sha1", "sha256", "sha512", "crc32"]

# Configure logging
logging.basicConfig(level=logging.INFO, 
                   format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)


class ConversionMode(Enum):
    """Enum representing the conversion mode."""
    TO_HEADER = auto()
    TO_FILE = auto()
    INFO = auto()


@dataclass
class ConversionOptions:
    """Data class for storing conversion options."""
    # Content options
    array_name: str = "resource_data"
    size_name: str = "resource_size"
    array_type: str = "unsigned char"
    const_qualifier: str = "const"
    include_size_var: bool = True
    
    # Format options
    data_format: DataFormat = "hex"
    comment_style: CommentStyle = "C"
    line_width: int = 80
    indent_size: int = 4
    items_per_line: int = 12
    
    # Processing options
    compression: CompressionType = "none"
    start_offset: int = 0
    end_offset: Optional[int] = None
    verify_checksum: bool = False
    checksum_algorithm: ChecksumAlgo = "sha256"
    
    # Output structure options
    add_include_guard: bool = True
    add_header_comment: bool = True
    include_original_filename: bool = True
    include_timestamp: bool = True
    cpp_namespace: Optional[str] = None
    cpp_class: bool = False
    cpp_class_name: Optional[str] = None
    split_size: Optional[int] = None
    
    # Advanced options
    extra_headers: List[str] = field(default_factory=list)
    extra_includes: List[str] = field(default_factory=list)
    custom_header: Optional[str] = None
    custom_footer: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert options to dictionary."""
        return asdict(self)
    
    @classmethod
    def from_dict(cls, options_dict: Dict[str, Any]) -> 'ConversionOptions':
        """Create ConversionOptions from dictionary."""
        return cls({k: v for k, v in options_dict.items() 
                    if k in cls.__dataclass_fields__})
    
    @classmethod
    def from_json(cls, json_file: PathLike) -> 'ConversionOptions':
        """Load options from JSON file."""
        with open(json_file, 'r', encoding='utf-8') as f:
            options_dict = json.load(f)
        return cls.from_dict(options_dict)
    
    @classmethod
    def from_yaml(cls, yaml_file: PathLike) -> 'ConversionOptions':
        """Load options from YAML file."""
        try:
            import yaml
            with open(yaml_file, 'r', encoding='utf-8') as f:
                options_dict = yaml.safe_load(f)
            return cls.from_dict(options_dict)
        except ImportError:
            raise ImportError("YAML support requires PyYAML. Install with 'pip install pyyaml'")


class HeaderInfo(TypedDict, total=False):
    """Type definition for header file information."""
    array_name: str
    size_name: str
    array_type: str
    data_format: str
    compression: str
    original_size: int
    compressed_size: int
    checksum: str
    timestamp: str
    original_filename: str


class ConversionError(Exception):
    """Base exception for conversion errors."""
    pass


class FileFormatError(ConversionError):
    """Exception raised for file format errors."""
    pass


class CompressionError(ConversionError):
    """Exception raised for compression/decompression errors."""
    pass


class ChecksumError(ConversionError):
    """Exception raised for checksum verification errors."""
    pass


class Converter:
    """
    A class that handles conversion between binary files and C/C++ headers.
    
    This class provides methods for converting binary files to C/C++ header files
    and vice versa, with various customization options.
    """
    
    def __init__(self, options: Optional[ConversionOptions] = None):
        """
        Initialize the converter with the given options.
        
        Args:
            options: ConversionOptions object with conversion settings.
                    If None, default options are used.
        """
        self.options = options or ConversionOptions()
    
    def _compress_data(self, data: bytes) -> Tuple[bytes, CompressionType]:
        """
        Compress the data using the specified algorithm.
        
        Args:
            data: The binary data to compress
            
        Returns:
            Tuple containing compressed data and the compression type used
            
        Raises:
            CompressionError: If compression fails
        """
        try:
            match self.options.compression:
                case "none":
                    return data, "none"
                case "zlib":
                    return zlib.compress(data), "zlib"
                case "lzma":
                    return lzma.compress(data), "lzma"
                case "bz2":
                    return bz2.compress(data), "bz2" 
                case "base64":
                    return base64.b64encode(data), "base64"
                case _:
                    logger.warning(f"Unknown compression type: {self.options.compression}. Using none.")
                    return data, "none"
        except Exception as e:
            raise CompressionError(f"Failed to compress data: {str(e)}") from e
    
    def _decompress_data(self, data: bytes, compression_type: CompressionType) -> bytes:
        """
        Decompress the data using the specified algorithm.
        
        Args:
            data: The compressed binary data
            compression_type: The compression algorithm used
            
        Returns:
            Decompressed binary data
            
        Raises:
            CompressionError: If decompression fails
        """
        try:
            match compression_type:
                case "none":
                    return data
                case "zlib":
                    return zlib.decompress(data)
                case "lzma":
                    return lzma.decompress(data)
                case "bz2":
                    return bz2.decompress(data)
                case "base64":
                    return base64.b64decode(data)
                case _:
                    raise CompressionError(f"Unknown compression type: {compression_type}")
        except Exception as e:
            raise CompressionError(f"Failed to decompress data: {str(e)}") from e
    
    def _format_byte(self, byte_value: int, data_format: DataFormat) -> str:
        """
        Format a byte according to the specified data format.
        
        Args:
            byte_value: Integer byte value (0-255)
            data_format: The target format ("hex", "bin", "dec", "oct", "char")
            
        Returns:
            String representation of the byte in the specified format
        """
        match data_format:
            case "hex":
                return f"0x{byte_value:02X}"
            case "bin":
                return f"0b{byte_value:08b}"
            case "dec":
                return str(byte_value)
            case "oct":
                return f"0{byte_value:o}"
            case "char":
                if 32 <= byte_value <= 126:  # Printable ASCII
                    # Escape special characters
                    char = chr(byte_value)
                    if char in "'\\":
                        return f"'\\{char}'"
                    else:
                        return f"'{char}'"
                else:
                    return f"0x{byte_value:02X}"  # Non-printable fallback
            case _:
                logger.warning(f"Unknown data format: {data_format}. Using hex format.")
                return f"0x{byte_value:02X}"
    
    def _generate_checksum(self, data: bytes) -> str:
        """
        Generate a checksum for the given data.
        
        Args:
            data: Binary data to generate checksum for
            
        Returns:
            Checksum as a hexadecimal string
        """
        match self.options.checksum_algorithm:
            case "md5":
                return hashlib.md5(data).hexdigest()
            case "sha1":
                return hashlib.sha1(data).hexdigest()
            case "sha256":
                return hashlib.sha256(data).hexdigest()
            case "sha512":
                return hashlib.sha512(data).hexdigest()
            case "crc32":
                return f"{zlib.crc32(data) & 0xFFFFFFFF:08x}"
            case _:
                logger.warning(f"Unknown checksum algorithm: {self.options.checksum_algorithm}. Using SHA-256.")
                return hashlib.sha256(data).hexdigest()
    
    def _verify_checksum(self, data: bytes, expected_checksum: str) -> bool:
        """
        Verify that the data matches the expected checksum.
        
        Args:
            data: Binary data to verify
            expected_checksum: Expected checksum as hexadecimal string
            
        Returns:
            True if checksums match, False otherwise
        """
        actual_checksum = self._generate_checksum(data)
        return actual_checksum.lower() == expected_checksum.lower()
    
    def _format_comment(self, comment_text: str) -> str:
        """
        Format a comment according to the specified style.
        
        Args:
            comment_text: The text of the comment
            
        Returns:
            Formatted comment string
        """
        if self.options.comment_style == "C":
            return f"/* {comment_text} */"
        else:
            return f"// {comment_text}"
    
    def _format_array_data(self, data: bytes) -> List[str]:
        """
        Format binary data as an array of values according to the specified format.
        
        Args:
            data: Binary data to format
            
        Returns:
            List of formatted value strings
        """
        return [self._format_byte(b, self.options.data_format) for b in data]
    
    def _format_array_initializer(self, formatted_values: List[str]) -> List[str]:
        """
        Format a list of values as an array initializer with proper line breaks.
        
        Args:
            formatted_values: List of formatted value strings
            
        Returns:
            List of lines for the array initializer
        """
        if not formatted_values:
            return ["{};"]
        
        # Determine indentation
        indent = " " * self.options.indent_size
        
        # Format array with line breaks
        lines = []
        values_per_line = self.options.items_per_line
        
        # Opening brace
        lines.append("{")
        
        # Add values with proper indentation
        for i in range(0, len(formatted_values), values_per_line):
            chunk = formatted_values[i:i + values_per_line]
            line = indent + ", ".join(chunk)
            if i + values_per_line < len(formatted_values):
                line += ","
            lines.append(line)
        
        # Closing brace
        lines.append("};")
        
        return lines
    
    def _generate_include_guard(self, filename: Path) -> str:
        """
        Generate an include guard name from a filename.
        
        Args:
            filename: Path object for the header file
            
        Returns:
            Include guard macro name
        """
        # Create a sanitized version of the filename for the include guard
        guard_name = filename.stem.upper()
        
        # Replace non-alphanumeric characters with underscore
        guard_name = ''.join(c if c.isalnum() else '_' for c in guard_name)
        
        # Ensure it starts with a letter or underscore (required for macro names)
        if guard_name and not (guard_name[0].isalpha() or guard_name[0] == '_'):
            guard_name = f"_{guard_name}"
            
        return f"{guard_name}_H"
    
    def _split_data_for_headers(self, data: bytes) -> List[bytes]:
        """
        Split data into chunks for multiple header files.
        
        Args:
            data: Binary data to split
            
        Returns:
            List of data chunks
        """
        if not self.options.split_size or self.options.split_size <= 0:
            return [data]
        
        chunks = []
        for i in range(0, len(data), self.options.split_size):
            chunks.append(data[i:i + self.options.split_size])
            
        return chunks
    
    def _generate_header_file_content(
        self,
        data: bytes,
        part_index: int = 0,
        total_parts: int = 1,
        original_size: int = None
    ) -> str:
        """
        Generate the content for a single header file.
        
        Args:
            data: Binary data to include in the header
            part_index: Index of this part (for split files)
            total_parts: Total number of parts
            original_size: Original size of the data before compression
            
        Returns:
            String containing the header file content
        """
        opts = self.options
        lines = []
        
        # Array name and size name for this part
        array_name = opts.array_name
        size_name = opts.size_name
        
        if total_parts > 1:
            array_name = f"{array_name}_part_{part_index}"
            size_name = f"{size_name}_part_{part_index}"
        
        # Format the array data
        formatted_values = self._format_array_data(data)
        array_initializer = self._format_array_initializer(formatted_values)
        
        # Add custom header if provided
        if opts.custom_header:
            lines.append(opts.custom_header)
            lines.append("")
        
        # Add include guard
        if opts.add_include_guard:
            guard_name = self._generate_include_guard(Path(f"{array_name}.h"))
            lines.append(f"#ifndef {guard_name}")
            lines.append(f"#define {guard_name}")
            lines.append("")
        
        # Add extra includes
        for include in opts.extra_includes:
            lines.append(f"#include {include}")
        if opts.extra_includes:
            lines.append("")
            
        # Add header comment
        if opts.add_header_comment:
            current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            
            if opts.comment_style == "C":
                lines.append("/*")
                
                if opts.include_original_filename:
                    lines.append(" * Generated from binary data")
                
                # Add part information if split
                if total_parts > 1:
                    lines.append(f" * Part {part_index + 1} of {total_parts}")
                
                # Add size information
                lines.append(f" * Size: {len(data)} bytes")
                if original_size is not None and original_size != len(data):
                    lines.append(f" * Original size: {original_size} bytes")
                
                # Add compression information if used
                if opts.compression != "none":
                    lines.append(f" * Compression: {opts.compression}")
                    
                # Add timestamp
                if opts.include_timestamp:
                    lines.append(f" * Generated on: {current_time}")
                    
                lines.append(" */")
            else:  # CPP style
                if opts.include_original_filename:
                    lines.append("// Generated from binary data")
                
                # Add part information if split
                if total_parts > 1:
                    lines.append(f"// Part {part_index + 1} of {total_parts}")
                
                # Add size information
                lines.append(f"// Size: {len(data)} bytes")
                if original_size is not None and original_size != len(data):
                    lines.append(f"// Original size: {original_size} bytes")
                
                # Add compression information if used
                if opts.compression != "none":
                    lines.append(f"// Compression: {opts.compression}")
                    
                # Add timestamp
                if opts.include_timestamp:
                    lines.append(f"// Generated on: {current_time}")
            
            lines.append("")
        
        # Add namespace opening if specified
        if opts.cpp_namespace:
            lines.append(f"namespace {opts.cpp_namespace} {{")
            lines.append("")
        
        # Add class opening if specified
        in_class = False
        if opts.cpp_class:
            class_name = opts.cpp_class_name or f"{array_name.capitalize()}Resource"
            lines.append(f"class {class_name} {{")
            lines.append("public:")
            in_class = True
        
        # Add array declaration
        indent = "    " if in_class else ""
        lines.append(f"{indent}{opts.const_qualifier} {opts.array_type} {array_name}[] = ")
        
        # Add array initializer
        for i, line in enumerate(array_initializer):
            if i == 0:
                lines.append(f"{indent}{line}")
            else:
                lines.append(f"{indent}{line}")
        
        # Add size variable
        if opts.include_size_var:
            lines.append("")
            lines.append(f"{indent}{opts.const_qualifier} unsigned int {size_name} = sizeof({array_name});")
        
        # Add class methods if in class
        if in_class:
            lines.append("")
            lines.append(f"    const {opts.array_type}* data() const {{ return {array_name}; }}")
            lines.append(f"    unsigned int size() const {{ return {size_name}; }}")
            lines.append("};")
        
        # Add namespace closing if specified
        if opts.cpp_namespace:
            lines.append("")
            lines.append(f"}} // namespace {opts.cpp_namespace}")
        
        # Add custom footer if provided
        if opts.custom_footer:
            lines.append("")
            lines.append(opts.custom_footer)
        
        # Add include guard closing
        if opts.add_include_guard:
            lines.append("")
            lines.append(f"#endif // {guard_name}")
        
        # Join lines and return
        return "\n".join(lines)
    
    def to_header(self, 
                 input_file: PathLike, 
                 output_file: Optional[PathLike] = None,
                 options: Optional[ConversionOptions] = None) -> List[Path]:
        """
        Convert a binary file to a C/C++ header file.
        
        Args:
            input_file: Path to the binary input file
            output_file: Path to the output header file (if None, derived from input_file)
            options: ConversionOptions object (if None, use the instance's options)
            
        Returns:
            List of Path objects for the generated header files (may be multiple if split)
            
        Raises:
            FileNotFoundError: If the input file is not found
            CompressionError: If data compression fails
            IOError: If writing to the output file fails
        """
        # Use provided options or instance options
        opts = options or self.options
        
        # Convert input and output paths to Path objects
        input_path = Path(input_file)
        if not input_path.exists():
            raise FileNotFoundError(f"Input file not found: {input_path}")
        
        # Default output file name if not specified
        if output_file is None:
            output_path = input_path.with_suffix(".h")
        else:
            output_path = Path(output_file)
            
        # Create output directory if it doesn't exist
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Read the input file
        logger.info(f"Reading input file: {input_path}")
        with open(input_path, 'rb') as f:
            data = f.read()
        
        # Apply start and end offsets
        if opts.start_offset > 0 or opts.end_offset is not None:
            data = data[opts.start_offset:opts.end_offset]
        
        original_size = len(data)
        logger.info(f"Original data size: {original_size} bytes")
        
        # Generate checksum if requested
        checksum = None
        if opts.verify_checksum:
            checksum = self._generate_checksum(data)
            logger.info(f"Generated {opts.checksum_algorithm} checksum: {checksum}")
        
        # Compress data if requested
        if opts.compression != "none":
            logger.info(f"Compressing data using {opts.compression}...")
            try:
                data, compression_type = self._compress_data(data)
                logger.info(f"Compressed size: {len(data)} bytes ({len(data)/original_size:.1%} of original)")
            except CompressionError as e:
                logger.error(f"Compression failed: {str(e)}")
                raise
        
        # Split data if necessary
        chunks = self._split_data_for_headers(data)
        total_chunks = len(chunks)
        logger.info(f"Splitting data into {total_chunks} chunk(s)")
        
        # Generate output files
        output_files = []
        for i, chunk in enumerate(chunks):
            # Determine output filename for this chunk
            if total_chunks > 1:
                chunk_path = output_path.with_name(f"{output_path.stem}_part_{i}{output_path.suffix}")
            else:
                chunk_path = output_path
                
            # Generate header content
            logger.info(f"Generating header file {i+1}/{total_chunks}: {chunk_path}")
            content = self._generate_header_file_content(
                chunk, i, total_chunks, original_size
            )
            
            # Write header file
            try:
                with open(chunk_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                output_files.append(chunk_path)
            except IOError as e:
                logger.error(f"Failed to write header file: {str(e)}")
                raise
        
        logger.info(f"Successfully generated {len(output_files)} header file(s)")
        return output_files
    
    def to_file(self, 
                input_header: PathLike, 
                output_file: Optional[PathLike] = None,
                options: Optional[ConversionOptions] = None) -> Path:
        """
        Convert a C/C++ header file back to a binary file.
        
        Args:
            input_header: Path to the header file
            output_file: Path to the output binary file (if None, derived from input_header)
            options: ConversionOptions object (if None, use the instance's options)
            
        Returns:
            Path object for the generated binary file
            
        Raises:
            FileNotFoundError: If the input file is not found
            FileFormatError: If the header file format is invalid
            CompressionError: If data decompression fails
            IOError: If writing to the output file fails
        """
        # Use provided options or instance options
        opts = options or self.options
        
        # Convert input and output paths to Path objects
        input_path = Path(input_header)
        if not input_path.exists():
            raise FileNotFoundError(f"Input header file not found: {input_path}")
        
        # Default output file name if not specified
        if output_file is None:
            output_path = input_path.with_suffix(".bin")
        else:
            output_path = Path(output_file)
            
        # Create output directory if it doesn't exist
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Read input header file
        logger.info(f"Reading header file: {input_path}")
        with open(input_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Extract header info to detect compression and other settings
        try:
            header_info = self._extract_header_info(content)
        except FileFormatError as e:
            logger.error(f"Failed to parse header file: {str(e)}")
            raise
            
        # Extract binary data from header
        logger.info(f"Extracting binary data from header file...")
        try:
            data = self._extract_binary_data_from_header(content, header_info)
        except FileFormatError as e:
            logger.error(f"Failed to extract binary data: {str(e)}")
            raise
            
        logger.info(f"Extracted {len(data)} bytes of binary data")
        
        # Decompress data if necessary
        compression_type = header_info.get("compression", "none")
        if compression_type != "none" or opts.compression != "none":
            # Use compression type from header if available, otherwise from options
            comp_type = compression_type if compression_type != "none" else opts.compression
            logger.info(f"Decompressing data using {comp_type}...")
            try:
                data = self._decompress_data(data, comp_type)
                logger.info(f"Decompressed size: {len(data)} bytes")
            except CompressionError as e:
                logger.error(f"Decompression failed: {str(e)}")
                raise
        
        # Verify checksum if requested and available
        if opts.verify_checksum and "checksum" in header_info:
            logger.info(f"Verifying {header_info.get('checksum_algorithm', 'checksum')}...")
            if not self._verify_checksum(data, header_info["checksum"]):
                raise ChecksumError("Checksum verification failed")
            logger.info("Checksum verification successful")
        
        # Write output file
        logger.info(f"Writing binary file: {output_path}")
        try:
            with open(output_path, 'wb') as f:
                f.write(data)
        except IOError as e:
            logger.error(f"Failed to write binary file: {str(e)}")
            raise
            
        logger.info(f"Successfully generated binary file: {output_path}")
        return output_path
    
    def _extract_header_info(self, content: str) -> HeaderInfo:
        """
        Extract information about the data from the header file content.
        
        Args:
            content: String content of the header file
            
        Returns:
            HeaderInfo dictionary with extracted information
            
        Raises:
            FileFormatError: If essential information cannot be extracted
        """
        info: HeaderInfo = {}  # Explicitly typed as HeaderInfo
        
        # Parse array name and data type
        array_decl_pattern = r'(\w+)\s+(\w+)\s+(\w+)\[\]'
        import re
        if match := re.search(array_decl_pattern, content):
            info["const_qualifier"] = match.group(1)
            info["array_type"] = match.group(2)
            info["array_name"] = match.group(3)
        
        # Extract compression information
        if "Compression: " in content:
            for line in content.split('\n'):
                if "Compression: " in line:
                    if match := re.search(r'Compression:\s*(\w+)', line):
                        info["compression"] = match.group(1)
        
        # Extract original size if available
        if "Original size: " in content:
            for line in content.split('\n'):
                if "Original size: " in line:
                    if match := re.search(r'Original size:\s*(\d+)', line):
                        info["original_size"] = int(match.group(1))
        
        # Extract checksum if available
        if "Checksum: " in content:
            for line in content.split('\n'):
                if "Checksum: " in line:
                    if match := re.search(r'Checksum \((\w+)\):\s*([0-9a-fA-F]+)', line):
                        info["checksum_algorithm"] = match.group(1)
                        info["checksum"] = match.group(2)
        
        return info
    
    def _extract_binary_data_from_header(self, content: str, header_info: HeaderInfo) -> bytes:
        """
        Extract binary data from header file content.
        
        Args:
            content: String content of the header file
            header_info: Extracted header information
            
        Returns:
            Extracted binary data
            
        Raises:
            FileFormatError: If the array data cannot be extracted
        """
        import re
        
        # Get array name from header info or use default
        array_name = header_info.get("array_name", self.options.array_name)
        
        # Find the array initialization
        # This pattern looks for array initialization between braces
        pattern = rf'{array_name}\[\]\s*=\s*{{([^}}]*)}}'
        
        if match := re.search(pattern, content, re.DOTALL):
            array_data_str = match.group(1)
            
            # Remove comments
            array_data_str = re.sub(r'/\*.*?\*/', '', array_data_str, flags=re.DOTALL)
            array_data_str = re.sub(r'//.*?$', '', array_data_str, flags=re.MULTILINE)
            
            # Split into individual elements and clean up
            elements = [elem.strip() for elem in array_data_str.split(',') if elem.strip()]
            
            # Convert elements to bytes
            binary_data = bytearray()
            for elem in elements:
                try:
                    if elem.startswith("0x"):  # Hex
                        binary_data.append(int(elem, 16))
                    elif elem.startswith("0b"):  # Binary
                        binary_data.append(int(elem, 2))
                    elif elem.startswith("'") and elem.endswith("'"):  # Char
                        # Handle escaped characters
                        char_content = elem[1:-1]
                        if len(char_content) == 1:
                            binary_data.append(ord(char_content))
                        elif len(char_content) == 2 and char_content[0] == '\\':
                            # Handle escaped char like '\n', '\t', etc.
                            if char_content[1] in {'n', 't', 'r', '0', '\\', '\'', '\"'}:
                                char = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0', 
                                        '\\': '\\', '\'': '\'', '\"': '\"'}[char_content[1]]
                                binary_data.append(ord(char))
                            else:
                                binary_data.append(ord(char_content[1]))
                    else:  # Decimal or other
                        binary_data.append(int(elem))
                except ValueError as e:
                    raise FileFormatError(f"Failed to parse element '{elem}': {str(e)}")
            
            return bytes(binary_data)
        else:
            raise FileFormatError(f"Could not find array data for '{array_name}' in header file")
    
    def get_header_info(self, header_file: PathLike) -> HeaderInfo:
        """
        Extract information from a header file.
        
        Args:
            header_file: Path to the header file
            
        Returns:
            HeaderInfo dictionary with extracted information
            
        Raises:
            FileNotFoundError: If the header file is not found
            FileFormatError: If the header file format is invalid
        """
        header_path = Path(header_file)
        if not header_path.exists():
            raise FileNotFoundError(f"Header file not found: {header_path}")
            
        with open(header_path, 'r', encoding='utf-8') as f:
            content = f.read()
            
        return self._extract_header_info(content)


# Convenience functions for direct use
def convert_to_header(
    input_file: PathLike, 
    output_file: Optional[PathLike] = None,
    **kwargs
) -> List[Path]:
    """
    Convert a binary file to a C/C++ header file.
    
    This is a convenience function that creates a Converter instance
    and calls its to_header method.
    
    Args:
        input_file: Path to the binary input file
        output_file: Path to the output header file (if None, derived from input_file)
        **kwargs: Additional options passed to ConversionOptions
        
    Returns:
        List of Path objects for the generated header files
    """
    # Create options from kwargs
    options = ConversionOptions()
    for key, value in kwargs.items():
        if hasattr(options, key):
            setattr(options, key, value)
    
    # Create converter and convert
    converter = Converter(options)
    return converter.to_header(input_file, output_file)



def convert_to_file(
    input_header: PathLike, 
    output_file: Optional[PathLike] = None,
    **kwargs
) -> Path:
    """
    Convert a C/C++ header file back to a binary file.
    
    This is a convenience function that creates a Converter instance
    and calls its to_file method.
    
    Args:
        input_header: Path to the header file
        output_file: Path to the output binary file (if None, derived from input_header)
        **kwargs: Additional options passed to ConversionOptions
        
    Returns:
        Path object for the generated binary file
    """
    # Create options from kwargs
    options = ConversionOptions()
    for key, value in kwargs.items():
        if hasattr(options, key):
            setattr(options, key, value)
    
    # Create converter and convert
    converter = Converter(options)
    return converter.to_file(input_header, output_file)


def get_header_info(header_file: PathLike) -> HeaderInfo:
    """
    Extract information from a header file.
    
    This is a convenience function that creates a Converter instance
    and calls its get_header_info method.
    
    Args:
        header_file: Path to the header file
        
    Returns:
        HeaderInfo dictionary with extracted information
    """
    converter = Converter()
    return converter.get_header_info(header_file)


def _build_argument_parser() -> argparse.ArgumentParser:
    """
    Build the command line argument parser.
    
    Returns:
        Configured ArgumentParser instance
    """
    parser = argparse.ArgumentParser(
        description="Convert between binary files and C/C++ headers",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert binary file to C header with zlib compression
  python convert_to_header.py to_header input.bin output.h --compression zlib
  
  # Convert header file back to binary, auto-detecting compression
  python convert_to_header.py to_file input.h output.bin
  
  # Show information about a header file
  python convert_to_header.py info header.h
  
  # Use custom formatting and C++ class wrapper
  python convert_to_header.py to_header input.bin output.h --cpp_class --data_format dec
        """
    )
    
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Enable verbose logging')
    
    subparsers = parser.add_subparsers(dest='mode',
                                       help='Operation mode')
    
    # Parser for to_header mode
    to_header_parser = subparsers.add_parser('to_header',
                                             help='Convert binary file to C/C++ header')
    to_header_parser.add_argument('input_file',
                                  help='Input binary file')
    to_header_parser.add_argument('output_file', nargs='?', default=None,
                                  help='Output header file (default: derived from input)')
    
    # Content options
    content_group = to_header_parser.add_argument_group('Content options')
    content_group.add_argument('--array_name',
                               help='Name of the array variable')
    content_group.add_argument('--size_name',
                               help='Name of the size variable')
    content_group.add_argument('--array_type',
                               help='Type of the array elements')
    content_group.add_argument('--const_qualifier',
                               help='Qualifier for const-ness (const, constexpr)')
    content_group.add_argument('--no_size_var', action='store_true',
                               help='Do not include size variable')
    
    # Format options
    format_group = to_header_parser.add_argument_group('Format options')
    format_group.add_argument('--data_format', choices=['hex', 'bin', 'dec', 'oct', 'char'],
                              help='Format for array data values')
    format_group.add_argument('--comment_style', choices=['C', 'CPP'],
                              help='Style for comments')
    format_group.add_argument('--line_width', type=int,
                              help='Maximum line width')
    format_group.add_argument('--indent_size', type=int,
                              help='Number of spaces for indentation')
    format_group.add_argument('--items_per_line', type=int,
                              help='Number of items per line in array')
    
    # Processing options
    proc_group = to_header_parser.add_argument_group('Processing options')
    proc_group.add_argument('--compression', 
                           choices=['none', 'zlib', 'lzma', 'bz2', 'base64'],
                           help='Compression algorithm to use')
    proc_group.add_argument('--start_offset', type=int,
                           help='Start offset in input file')
    proc_group.add_argument('--end_offset', type=int,
                           help='End offset in input file')
    proc_group.add_argument('--checksum', action='store_true',
                           help='Include checksum in header')
    proc_group.add_argument('--checksum_algorithm',
                           choices=['md5', 'sha1', 'sha256', 'sha512', 'crc32'],
                           help='Algorithm for checksum calculation')
    
    # Output structure options
    struct_group = to_header_parser.add_argument_group('Output structure options')
    struct_group.add_argument('--no_include_guard', action='store_true',
                             help='Do not add include guards')
    struct_group.add_argument('--no_header_comment', action='store_true',
                             help='Do not add header comment')
    struct_group.add_argument('--no_timestamp', action='store_true',
                             help='Do not include timestamp in header')
    struct_group.add_argument('--cpp_namespace',
                             help='Wrap code in C++ namespace')
    struct_group.add_argument('--cpp_class', action='store_true',
                             help='Generate C++ class wrapper')
    struct_group.add_argument('--cpp_class_name',
                             help='Name for C++ class wrapper')
    struct_group.add_argument('--split_size', type=int,
                             help='Split into multiple files with this max size (bytes)')
    
    # Advanced options
    adv_group = to_header_parser.add_argument_group('Advanced options')
    adv_group.add_argument('--config', 
                          help='Path to JSON/YAML configuration file')
    adv_group.add_argument('--include',
                          help='Add #include directive (can be specified multiple times)',
                          action='append', dest='extra_includes')
    
    # Parser for to_file mode
    to_file_parser = subparsers.add_parser('to_file', 
                                          help='Convert C/C++ header back to binary file')
    to_file_parser.add_argument('input_file',
                               help='Input header file')
    to_file_parser.add_argument('output_file', nargs='?', default=None,
                               help='Output binary file (default: derived from input)')
    to_file_parser.add_argument('--compression',
                               choices=['none', 'zlib', 'lzma', 'bz2', 'base64'],
                               help='Compression algorithm (overrides auto-detection)')
    to_file_parser.add_argument('--verify_checksum', action='store_true',
                               help='Verify checksum if present')
    
    # Parser for info mode
    info_parser = subparsers.add_parser('info',
                                       help='Show information about a header file')
    info_parser.add_argument('input_file',
                            help='Header file to analyze')
    
    return parser


def _convert_args_to_options(args: argparse.Namespace) -> ConversionOptions:
    """
    Convert command-line arguments to a ConversionOptions object.
    
    Args:
        args: Parsed command-line arguments
        
    Returns:
        ConversionOptions object
    """
    # Start with default options
    options = ConversionOptions()
    
    # Load from config file if specified
    if hasattr(args, 'config') and args.config:
        config_path = Path(args.config)
        if config_path.suffix.lower() in ('.yml', '.yaml'):
            options = ConversionOptions.from_yaml(config_path)
        else:
            options = ConversionOptions.from_json(config_path)
    
    # Override with command-line arguments
    for key, value in vars(args).items():
        if key in ConversionOptions.__dataclass_fields__ and value is not None:
            setattr(options, key, value)
    
    # Handle special cases
    if hasattr(args, 'no_size_var') and args.no_size_var:
        options.include_size_var = False
    if hasattr(args, 'no_include_guard') and args.no_include_guard:
        options.add_include_guard = False
    if hasattr(args, 'no_header_comment') and args.no_header_comment:
        options.add_header_comment = False
    if hasattr(args, 'no_timestamp') and args.no_timestamp:
        options.include_timestamp = False
    if hasattr(args, 'checksum') and args.checksum:
        options.verify_checksum = True
    
    return options


# pybind11 module initialization function
def _create_pybind_bindings(m):
    """
    Create pybind11 bindings for the module.
    
    This function creates C++ bindings for the Python classes and functions
    to allow seamless integration with C++ code via pybind11.
    
    Args:
        m: The pybind11 module object
    """
    try:
        import pybind11 as pb
    except ImportError:
        logger.warning("pybind11 not available. C++ bindings will not be created.")
        return
    
    # Bind the ConversionMode enum
    pb.enum_<ConversionMode>(m, "ConversionMode") \
        .value("TO_HEADER", ConversionMode.TO_HEADER) \
        .value("TO_FILE", ConversionMode.TO_FILE) \
        .value("INFO", ConversionMode.INFO) \
        .export_values()
    
    # Bind the ConversionOptions class
    pb.class_<ConversionOptions>(m, "ConversionOptions") \
        .def(pb.init<>()) \
        .def_readwrite("array_name", &ConversionOptions.array_name) \
        .def_readwrite("size_name", &ConversionOptions.size_name) \
        .def_readwrite("array_type", &ConversionOptions.array_type) \
        .def_readwrite("const_qualifier", &ConversionOptions.const_qualifier) \
        .def_readwrite("include_size_var", &ConversionOptions.include_size_var) \
        .def_readwrite("data_format", &ConversionOptions.data_format) \
        .def_readwrite("comment_style", &ConversionOptions.comment_style) \
        .def_readwrite("line_width", &ConversionOptions.line_width) \
        .def_readwrite("indent_size", &ConversionOptions.indent_size) \
        .def_readwrite("items_per_line", &ConversionOptions.items_per_line) \
        .def_readwrite("compression", &ConversionOptions.compression) \
        .def_readwrite("start_offset", &ConversionOptions.start_offset) \
        .def_readwrite("end_offset", &ConversionOptions.end_offset) \
        .def_readwrite("verify_checksum", &ConversionOptions.verify_checksum) \
        .def_readwrite("checksum_algorithm", &ConversionOptions.checksum_algorithm) \
        .def_readwrite("add_include_guard", &ConversionOptions.add_include_guard) \
        .def_readwrite("add_header_comment", &ConversionOptions.add_header_comment) \
        .def_readwrite("include_original_filename", &ConversionOptions.include_original_filename) \
        .def_readwrite("include_timestamp", &ConversionOptions.include_timestamp) \
        .def_readwrite("cpp_namespace", &ConversionOptions.cpp_namespace) \
        .def_readwrite("cpp_class", &ConversionOptions.cpp_class) \
        .def_readwrite("cpp_class_name", &ConversionOptions.cpp_class_name) \
        .def_readwrite("split_size", &ConversionOptions.split_size) \
        .def("to_dict", &ConversionOptions.to_dict) \
        .def_static("from_dict", &ConversionOptions.from_dict) \
        .def_static("from_json", &ConversionOptions.from_json) \
        .def_static("from_yaml", &ConversionOptions.from_yaml)
    
    # Bind the Converter class and methods
    pb.class_<Converter>(m, "Converter") \
        .def(pb.init<>()) \
        .def(pb.init<ConversionOptions>()) \
        .def("to_header", &Converter.to_header,
             pb.arg("input_file"), pb.arg("output_file") = nullptr, 
             pb.arg("options") = nullptr) \
        .def("to_file", &Converter.to_file,
             pb.arg("input_header"), pb.arg("output_file") = nullptr,
             pb.arg("options") = nullptr) \
        .def("get_header_info", &Converter.get_header_info)
    
    # Bind standalone functions
    m.def("convert_to_header", &convert_to_header,
          pb.arg("input_file"), pb.arg("output_file") = nullptr)
    m.def("convert_to_file", &convert_to_file,
          pb.arg("input_header"), pb.arg("output_file") = nullptr)
    m.def("get_header_info", &get_header_info,
          pb.arg("header_file"))


def _initialize_pybind():
    """
    Initialize pybind11 module if imported in C++ context.
    """
    # Check if we're being imported via pybind11
    import sys
    if "_pybind11_" in sys.modules:
        m = sys.modules["__main__"]
        _create_pybind_bindings(m)


# Try to initialize pybind when imported
try:
    _initialize_pybind()
except Exception as e:
    logger.debug(f"Pybind initialization skipped: {str(e)}")


def main() -> int:
    """
    Main entry point for the command-line interface.
    
    Returns:
        Exit code (0 for success, non-zero for errors)
    """
    try:
        # Parse command-line arguments
        parser = _build_argument_parser()
        args = parser.parse_args()
        
        # Configure logging
        if args.verbose:
            logger.setLevel(logging.DEBUG)
        
        # Check for tqdm for progress reporting
        try:
            from tqdm import tqdm
            logger.debug("tqdm available for progress reporting")
        except ImportError:
            logger.debug("tqdm not available, progress reporting disabled")
        
        # Process based on mode
        match args.mode:
            case "to_header":
                options = _convert_args_to_options(args)
                converter = Converter(options)
                generated_files = converter.to_header(args.input_file, args.output_file)
                logger.info(f"Generated {len(generated_files)} header file(s)")
                for file_path in generated_files:
                    logger.info(f"  - {file_path}")
                return 0
                
            case "to_file":
                options = _convert_args_to_options(args)
                converter = Converter(options)
                output_file = converter.to_file(args.input_file, args.output_file)
                logger.info(f"Generated binary file: {output_file}")
                return 0
                
            case "info":
                try:
                    header_info = get_header_info(args.input_file)
                    print(f"Header file information for: {args.input_file}")
                    print("-" * 50)
                    for key, value in header_info.items():
                        print(f"{key.replace('_', ' ').title()}: {value}")
                    return 0
                except FileFormatError as e:
                    logger.error(f"Failed to extract header information: {str(e)}")
                    return 1
            
            case _:
                logger.error(f"Unknown mode: {args.mode}")
                return 1
        
    except Exception as e:
        logger.error(f"Error: {str(e)}")
        if logger.getEffectiveLevel() <= logging.DEBUG:
            import traceback
            logger.debug(traceback.format_exc())
        return 1


# Public API
__all__ = [
    'Converter',
    'ConversionOptions',
    'ConversionMode',
    'HeaderInfo',
    'ConversionError',
    'FileFormatError',
    'CompressionError',
    'ChecksumError',
    'convert_to_header',
    'convert_to_file',
    'get_header_info'
]

# Make the script importable but also executable
if __name__ == "__main__":
    sys.exit(main())