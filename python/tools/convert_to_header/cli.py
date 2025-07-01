#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: cli.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-07-01
Version: 2.1

Description:
------------
Command-line interface for the convert_to_header package, powered by Typer.
"""

import sys
from pathlib import Path
from typing import Optional, List

import typer
from rich.console import Console
from loguru import logger

from .options import ConversionOptions
from .converter import Converter, get_header_info
from .exceptions import ConversionError
from .utils import DataFormat, CommentStyle, CompressionType, ChecksumAlgo

app = typer.Typer(
    name="convert-to-header",
    help="A powerful tool to convert binary files to C/C++ headers and back.",
    add_completion=False,
    context_settings={"help_option_names": ["-h", "--help"]},
)
console = Console()


def version_callback(value: bool):
    if value:
        console.print("convert-to-header version: 2.1.0")
        raise typer.Exit()


@app.callback()
def main_options(
    verbose: bool = typer.Option(
        False, "--verbose", "-v", help="Enable verbose logging."),
    version: Optional[bool] = typer.Option(
        None, "--version", callback=version_callback, is_eager=True, help="Show version and exit."
    ),
):
    """Manage global options."""
    logger.remove()
    level = "DEBUG" if verbose else "INFO"
    logger.add(sys.stderr, level=level,
               format="{time:YYYY-MM-DD HH:mm:ss} | {level: <8} | {message}")


def _get_options_from_args(config_file: Optional[Path], **kwargs) -> ConversionOptions:
    """Create ConversionOptions from config file and CLI arguments."""
    options = ConversionOptions()
    if config_file and config_file.exists():
        logger.info(f"Loading options from config file: {config_file}")
        if config_file.suffix.lower() in ('.yml', '.yaml'):
            options = ConversionOptions.from_yaml(config_file)
        else:
            options = ConversionOptions.from_json(config_file)

    for key, value in kwargs.items():
        if value is not None and hasattr(options, key):
            setattr(options, key, value)
    return options


@app.command("to-header")
def to_header_command(
    input_file: Path = typer.Argument(..., help="Input binary file.",
                                      exists=True, readable=True),
    output_file: Optional[Path] = typer.Argument(
        None, help="Output header file. [default: <input>.h]"),
    config: Optional[Path] = typer.Option(
        None, help="Path to JSON/YAML configuration file.", exists=True),
    # ... other options ...
):
    """Convert a binary file to a C/C++ header."""
    try:
        cli_args = {k: v for k, v in locals().items() if k not in [
            'input_file', 'output_file', 'config']}
        options = _get_options_from_args(config, **cli_args)
        converter = Converter(options)
        generated_files = converter.to_header(input_file, output_file)
        console.print(
            f"[green]✔[/green] Generated {len(generated_files)} header file(s):")
        for file_path in generated_files:
            console.print(f"  - {file_path}")
    except ConversionError as e:
        console.print(f"[red]Error:[/red] {e}")
        raise typer.Exit(code=1)


@app.command("to-file")
def to_file_command(
    input_file: Path = typer.Argument(..., help="Input header file.",
                                      exists=True, readable=True),
    output_file: Optional[Path] = typer.Argument(
        None, help="Output binary file. [default: <input>.bin]"),
    compression: Optional[CompressionType] = typer.Option(
        None, help="Override compression auto-detection."),
    verify_checksum: bool = typer.Option(
        False, "--verify-checksum", help="Verify checksum if present in header."),
):
    """Convert a C/C++ header back to a binary file."""
    try:
        options = ConversionOptions(
            compression=compression, verify_checksum=verify_checksum)
        converter = Converter(options)
        generated_file = converter.to_file(input_file, output_file)
        console.print(
            f"[green]✔[/green] Generated binary file: {generated_file}")
    except ConversionError as e:
        console.print(f"[red]Error:[/red] {e}")
        raise typer.Exit(code=1)


@app.command("info")
def info_command(
    input_file: Path = typer.Argument(
        ..., help="Header file to analyze.", exists=True, readable=True)
):
    """Show information about a generated header file."""
    try:
        info = get_header_info(input_file)
        console.print(
            f"Header Information for: [bold cyan]{input_file}[/bold cyan]")
        for key, value in info.items():
            console.print(
                f"  [bold]{key.replace('_', ' ').title()}:[/bold] {value}")
    except ConversionError as e:
        console.print(f"[red]Error:[/red] {e}")
        raise typer.Exit(code=1)


def main():
    app()


if __name__ == "__main__":
    main()
