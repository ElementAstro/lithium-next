"""
Console formatter widget.

This module provides functionality to format compiler output for console display
with colorized output based on message severity.
"""

from termcolor import colored

from ..core.data_structures import CompilerOutput
from ..core.enums import MessageSeverity


class ConsoleFormatterWidget:
    """Widget for formatting compiler output for console display."""

    def __init__(self):
        """Initialize the console formatter widget."""
        self.color_map = {
            MessageSeverity.ERROR: "red",
            MessageSeverity.WARNING: "yellow",
            MessageSeverity.INFO: "blue",
        }

        self.prefix_map = {
            MessageSeverity.ERROR: "ERROR",
            MessageSeverity.WARNING: "WARNING",
            MessageSeverity.INFO: "INFO",
        }

    def format_summary(self, compiler_output: CompilerOutput) -> str:
        """Format a summary of compiler output."""
        lines = [
            "\nCompiler Output Summary:",
            f"Compiler: {compiler_output.compiler.name}",
            f"Version: {compiler_output.version}",
            f"Total Messages: {len(compiler_output.messages)}",
            f"Errors: {len(compiler_output.errors)}",
            f"Warnings: {len(compiler_output.warnings)}",
            f"Info: {len(compiler_output.infos)}",
        ]
        return "\n".join(lines)

    def format_message(self, msg) -> str:
        """Format a single compiler message with color."""
        color = self.color_map.get(msg.severity, "white")
        prefix = self.prefix_map.get(msg.severity, "UNKNOWN")

        location = f"{msg.file}:{msg.line}"
        if msg.column is not None:
            location += f":{msg.column}"

        code_info = f" [{msg.code}]" if msg.code else ""

        message = f"{prefix}: {location}{code_info} - {msg.message}"
        return colored(message, color)

    def colorize_output(self, compiler_output: CompilerOutput) -> None:
        """Print compiler output with colorized formatting based on message severity."""
        print(self.format_summary(compiler_output))
        print("\nMessages:")

        for msg in compiler_output.messages:
            print(self.format_message(msg))

    def get_formatted_output(self, compiler_output: CompilerOutput) -> str:
        """Get formatted output as a string without colors."""
        lines = [self.format_summary(compiler_output), "\nMessages:"]

        for msg in compiler_output.messages:
            prefix = self.prefix_map.get(msg.severity, "UNKNOWN")
            location = f"{msg.file}:{msg.line}"
            if msg.column is not None:
                location += f":{msg.column}"

            code_info = f" [{msg.code}]" if msg.code else ""
            message = f"{prefix}: {location}{code_info} - {msg.message}"
            lines.append(message)

        return "\n".join(lines)
