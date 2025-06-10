#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: cli.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-06-08
Version: 2.0

Description:
------------
Command-line interface for the convert_to_header package.
"""

import sys
import argparse
from pathlib import Path

from loguru import logger
from .options import ConversionOptions
from .converter import Converter, convert_to_header, convert_to_file, get_header_info
from .exceptions import ConversionError, FileFormatError, ChecksumError


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
                            choices=['md5', 'sha1',
                                     'sha256', 'sha512', 'crc32'],
                            help='Algorithm for checksum calculation')

    # Output structure options
    struct_group = to_header_parser.add_argument_group(
        'Output structure options')
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
                                choices=['none', 'zlib',
                                         'lzma', 'bz2', 'base64'],
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


def main() -> int:
    """
    Main entry point for the command-line interface.

    Returns:
        Exit code (0 for success, non-zero for errors)
    """
    args = None
    try:
        # Parse command-line arguments
        parser = _build_argument_parser()
        args = parser.parse_args()

        # Configure logging based on verbosity
        if args.verbose:
            logger.remove()
            logger.add(sys.stderr, level="DEBUG")
            logger.debug("Verbose logging enabled")

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
                generated_files = converter.to_header(
                    args.input_file, args.output_file)
                logger.success(
                    f"Generated {len(generated_files)} header file(s)")
                for file_path in generated_files:
                    logger.info(f"  - {file_path}")
                return 0

            case "to_file":
                options = _convert_args_to_options(args)
                converter = Converter(options)
                output_file = converter.to_file(
                    args.input_file, args.output_file)
                logger.success(f"Generated binary file: {output_file}")
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
                    logger.error(
                        f"Failed to extract header information: {str(e)}")
                    return 1

            case _:
                logger.error(f"Unknown mode: {args.mode}")
                parser.print_help()
                return 1
    except Exception as e:
        logger.error(f"Error: {str(e)}")
        if args is not None and hasattr(args, "verbose") and args.verbose:
            import traceback
            logger.debug(traceback.format_exc())
        return 1
        return 1


if __name__ == "__main__":
    sys.exit(main())
