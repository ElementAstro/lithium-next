#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: converter.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-06-08
Version: 2.0

Description:
------------
Main converter class for the convert_to_header package.
"""

import zlib
import lzma
import bz2
import base64
import hashlib
import re
from datetime import datetime
from pathlib import Path
from typing import Optional, List, Tuple

from loguru import logger
from .options import ConversionOptions
from .utils import HeaderInfo, PathLike, DataFormat, CompressionType
from .exceptions import FileFormatError, CompressionError, ChecksumError


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
                    logger.warning(
                        f"Unknown compression type: {self.options.compression}. Using none."
                    )
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
                    raise CompressionError(
                        f"Unknown compression type: {compression_type}"
                    )
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
                logger.warning(
                    f"Unknown checksum algorithm: {self.options.checksum_algorithm}. Using SHA-256."
                )
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
            chunk = formatted_values[i : i + values_per_line]
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
        guard_name = "".join(c if c.isalnum() else "_" for c in guard_name)

        # Ensure it starts with a letter or underscore (required for macro names)
        if guard_name and not (guard_name[0].isalpha() or guard_name[0] == "_"):
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
            chunks.append(data[i : i + self.options.split_size])

        return chunks

    def _generate_header_file_content(
        self,
        data: bytes,
        part_index: int = 0,
        total_parts: int = 1,
        original_size: int = None,
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
        guard_name = ""

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
        lines.append(
            f"{indent}{opts.const_qualifier} {opts.array_type} {array_name}[] = "
        )

        # Add array initializer
        for i, line in enumerate(array_initializer):
            if i == 0:
                lines.append(f"{indent}{line}")
            else:
                lines.append(f"{indent}{line}")

        # Add size variable
        if opts.include_size_var:
            lines.append("")
            lines.append(
                f"{indent}{opts.const_qualifier} unsigned int {size_name} = sizeof({array_name});"
            )

        # Add class methods if in class
        if in_class:
            lines.append("")
            lines.append(
                f"    const {opts.array_type}* data() const {{ return {array_name}; }}"
            )
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
        array_decl_pattern = r"(\w+)\s+(\w+)\s+(\w+)$$$$"
        if match := re.search(array_decl_pattern, content):
            info["const_qualifier"] = match.group(1)
            info["array_type"] = match.group(2)
            info["array_name"] = match.group(3)

        # Extract compression information
        if "Compression: " in content:
            for line in content.split("\n"):
                if "Compression: " in line:
                    if match := re.search(r"Compression:\s*(\w+)", line):
                        info["compression"] = match.group(1)

        # Extract original size if available
        if "Original size: " in content:
            for line in content.split("\n"):
                if "Original size: " in line:
                    if match := re.search(r"Original size:\s*(\d+)", line):
                        info["original_size"] = int(match.group(1))

        # Extract checksum if available
        if "Checksum: " in content:
            for line in content.split("\n"):
                if "Checksum: " in line:
                    if match := re.search(r"Checksum $(\w+)$:\s*([0-9a-fA-F]+)", line):
                        info["checksum_algorithm"] = match.group(1)
                        info["checksum"] = match.group(2)

        return info

    def _extract_binary_data_from_header(
        self, content: str, header_info: HeaderInfo
    ) -> bytes:
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
        # Get array name from header info or use default
        array_name = header_info.get("array_name", self.options.array_name)

        # Find the array initialization
        # This pattern looks for array initialization between braces
        pattern = rf"{array_name}$$$$\s*=\s*{{([^}}]*)}}"

        if match := re.search(pattern, content, re.DOTALL):
            array_data_str = match.group(1)

            # Remove comments
            array_data_str = re.sub(r"/\*.*?\*/", "", array_data_str, flags=re.DOTALL)
            array_data_str = re.sub(r"//.*?$", "", array_data_str, flags=re.MULTILINE)

            # Split into individual elements and clean up
            elements = [
                elem.strip() for elem in array_data_str.split(",") if elem.strip()
            ]

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
                        elif len(char_content) == 2 and char_content[0] == "\\":
                            # Handle escaped char like '\n', '\t', etc.
                            if char_content[1] in {"n", "t", "r", "0", "\\", "'", '"'}:
                                char = {
                                    "n": "\n",
                                    "t": "\t",
                                    "r": "\r",
                                    "0": "\0",
                                    "\\": "\\",
                                    "'": "'",
                                    '"': '"',
                                }[char_content[1]]
                                binary_data.append(ord(char))
                            else:
                                binary_data.append(ord(char_content[1]))
                    else:  # Decimal or other
                        binary_data.append(int(elem))
                except ValueError as e:
                    raise FileFormatError(f"Failed to parse element '{elem}': {str(e)}")

            return bytes(binary_data)
        else:
            raise FileFormatError(
                f"Could not find array data for '{array_name}' in header file"
            )

    def to_header(
        self,
        input_file: PathLike,
        output_file: Optional[PathLike] = None,
        options: Optional[ConversionOptions] = None,
    ) -> List[Path]:
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
        with open(input_path, "rb") as f:
            data = f.read()

        # Apply start and end offsets
        if opts.start_offset > 0 or opts.end_offset is not None:
            data = data[opts.start_offset : opts.end_offset]

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
                logger.info(
                    f"Compressed size: {len(data)} bytes ({len(data)/original_size:.1%} of original)"
                )
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
                chunk_path = output_path.with_name(
                    f"{output_path.stem}_part_{i}{output_path.suffix}"
                )
            else:
                chunk_path = output_path

            # Generate header content
            logger.info(f"Generating header file {i+1}/{total_chunks}: {chunk_path}")
            content = self._generate_header_file_content(
                chunk, i, total_chunks, original_size
            )

            # Write header file
            try:
                with open(chunk_path, "w", encoding="utf-8") as f:
                    f.write(content)
                output_files.append(chunk_path)
            except IOError as e:
                logger.error(f"Failed to write header file: {str(e)}")
                raise

        logger.info(f"Successfully generated {len(output_files)} header file(s)")
        return output_files

    def to_file(
        self,
        input_header: PathLike,
        output_file: Optional[PathLike] = None,
        options: Optional[ConversionOptions] = None,
    ) -> Path:
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
        with open(input_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Extract header info to detect compression and other settings
        try:
            header_info = self._extract_header_info(content)
        except FileFormatError as e:
            logger.error(f"Failed to parse header file: {str(e)}")
            raise

        # Extract binary data from header
        logger.info("Extracting binary data from header file...")
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
            comp_type = (
                compression_type if compression_type != "none" else opts.compression
            )
            logger.info(f"Decompressing data using {comp_type}...")
            try:
                data = self._decompress_data(data, comp_type)
                logger.info(f"Decompressed size: {len(data)} bytes")
            except CompressionError as e:
                logger.error(f"Decompression failed: {str(e)}")
                raise

        # Verify checksum if requested and available
        if opts.verify_checksum and "checksum" in header_info:
            logger.info(
                f"Verifying {header_info.get('checksum_algorithm', 'checksum')}..."
            )
            if not self._verify_checksum(data, header_info["checksum"]):
                logger.error("Checksum verification failed")
                raise ChecksumError("Checksum verification failed")
            logger.info("Checksum verification successful")

        # Write output file
        logger.info(f"Writing binary file: {output_path}")
        try:
            with open(output_path, "wb") as f:
                f.write(data)
        except IOError as e:
            logger.error(f"Failed to write binary file: {str(e)}")
            raise

        logger.info(f"Successfully generated binary file: {output_path}")
        return output_path

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

        with open(header_path, "r", encoding="utf-8") as f:
            content = f.read()

        return self._extract_header_info(content)


# Convenience functions for direct use
def convert_to_header(
    input_file: PathLike, output_file: Optional[PathLike] = None, **kwargs
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
    input_header: PathLike, output_file: Optional[PathLike] = None, **kwargs
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
