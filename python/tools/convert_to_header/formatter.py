#!/usr/bin/env python3
"""Generates the content for C/C++ header files."""
import textwrap
from datetime import datetime
from pathlib import Path
from typing import List
from .options import ConversionOptions
from .utils import DataFormat


class HeaderFormatter:
    """A class to format binary data into a C/C++ header file."""

    def __init__(self, options: ConversionOptions):
        self.options = options

    def _format_byte(self, byte_value: int) -> str:
        """Format a byte according to the specified data format."""
        data_format = self.options.data_format
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
                if 32 <= byte_value <= 126 and chr(byte_value) not in "'\\":
                    return f"'{chr(byte_value)}'"
                if byte_value == ord("'"):
                    return "'''"
                if byte_value == ord("\\"):
                    return "'\\\\'"
                return f"0x{byte_value:02X}"
            case _:
                raise ValueError(f"Unknown data format: {data_format}")

    def _format_array_initializer(self, data: bytes) -> str:
        """Format binary data as a C-style array initializer."""
        if not data:
            return "{};"

        formatted_values = [self._format_byte(b) for b in data]

        lines = []
        line = "  "
        for i, value in enumerate(formatted_values):
            new_line = line + value + ","
            if len(new_line) > self.options.line_width:
                lines.append(line.rstrip())
                line = "  " + value + ","
            else:
                line = new_line

            if (i + 1) % self.options.items_per_line == 0:
                lines.append(line.rstrip())
                line = "  "

        if line.strip():
            lines.append(line.rstrip().rstrip(","))

        for i in range(len(lines) - 1, -1, -1):
            if lines[i].strip():
                lines[i] = lines[i].rstrip(",")
                break

        return "{\n" + "\n".join(lines) + "\n};"

    def _generate_include_guard(self, filename: Path) -> str:
        """Generate an include guard name from a filename."""
        guard_name = f"{filename.stem.upper()}_{filename.suffix[1:].upper()}_H"
        return "".join(c if c.isalnum() else "_" for c in guard_name)

    def generate_header_content(
        self,
        data: bytes,
        output_path: Path,
        original_filename: str,
        original_size: int,
        checksum: str | None,
    ) -> str:
        """Generate the complete content for a single header file."""
        opts = self.options
        lines = []

        if opts.add_header_comment:
            lines.append(f"// Generated from {original_filename}")
            if opts.include_timestamp:
                lines.append(
                    f"// Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
                )
            if opts.compression != "none":
                lines.append(f"// Compression: {opts.compression}")
                lines.append(f"// Original size: {original_size} bytes")
            if checksum:
                lines.append(f"// Checksum ({opts.checksum_algorithm}): {checksum}")
            lines.append(f"// Compressed size: {len(data)} bytes")
            lines.append("")

        if opts.add_include_guard:
            guard_name = self._generate_include_guard(output_path)
            lines.append(f"#ifndef {guard_name}")
            lines.append(f"#define {guard_name}")
            lines.append("")

        if opts.extra_includes:
            for include in opts.extra_includes:
                lines.append(f"#include {include}")
            lines.append("")

        if opts.cpp_namespace:
            lines.append(f"namespace {opts.cpp_namespace} {{")
            lines.append("")

        class_indent = "  " if opts.cpp_class else ""
        if opts.cpp_class:
            class_name = (
                opts.cpp_class_name or f"{opts.array_name.capitalize()}Resource"
            )
            lines.append(f"class {class_name} {{")
            lines.append("public:")

        array_decl = f"{opts.const_qualifier} {opts.array_type} {opts.array_name}[]"
        lines.append(f"{class_indent}{array_decl} =")
        lines.append(
            textwrap.indent(self._format_array_initializer(data), class_indent)
        )
        lines.append("")

        if opts.include_size_var:
            size_decl = f"{opts.const_qualifier} unsigned int {opts.size_name} = sizeof({opts.array_name});"
            lines.append(f"{class_indent}{size_decl}")
            lines.append("")

        if opts.cpp_class:
            lines.append(
                f"  const {opts.array_type}* data() const {{ return {opts.array_name}; }}"
            )
            if opts.include_size_var:
                lines.append(
                    f"  unsigned int size() const {{ return {opts.size_name}; }}"
                )
            lines.append("};")
            lines.append("")

        if opts.cpp_namespace:
            lines.append(f"}} // namespace {opts.cpp_namespace}")
            lines.append("")

        if opts.add_include_guard:
            lines.append(f"#endif // {guard_name}")

        return "\n".join(lines)
