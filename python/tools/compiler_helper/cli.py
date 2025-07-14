#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Command-line interface for the compiler helper module.
"""
import sys
import argparse
from pathlib import Path

from loguru import logger

from .core_types import CompileOptions, LinkOptions
from .compiler_manager import CompilerManager
from .build_manager import BuildManager
from .utils import load_json


def main():
    """
    Main function for command-line usage.
    """
    parser = argparse.ArgumentParser(
        description="Enhanced Compiler Helper Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Compile a single file
  python compiler_helper.py source.cpp -o output.o --cpp-version c++20

  # Compile and link multiple files
  python compiler_helper.py source1.cpp source2.cpp -o myprogram --link --compiler GCC

  # Build with specific options
  python compiler_helper.py source.cpp -o output.o --include-path ./include --define DEBUG=1

  # Use incremental builds
  python compiler_helper.py *.cpp -o myprogram --build-dir ./build --incremental
""",
    )

    # Basic arguments
    parser.add_argument(
        "source_files", nargs="+", type=Path, help="Source files to compile"
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        required=True,
        help="Output file (object or executable)",
    )
    parser.add_argument(
        "--compiler", type=str, help="Compiler to use (GCC, Clang, MSVC)"
    )
    parser.add_argument(
        "--cpp-version",
        type=str,
        default="c++17",
        help="C++ standard version (e.g., c++17, c++20)",
    )
    parser.add_argument(
        "--link", action="store_true", help="Link the object files into an executable"
    )

    # Build options
    build_group = parser.add_argument_group("Build options")
    build_group.add_argument(
        "--build-dir", type=Path, help="Directory for build artifacts"
    )
    build_group.add_argument(
        "--incremental", action="store_true", help="Use incremental builds"
    )
    build_group.add_argument(
        "--force-rebuild", action="store_true", help="Force rebuilding all files"
    )
    build_group.add_argument(
        "--parallel", action="store_true", default=True, help="Use parallel compilation"
    )
    build_group.add_argument(
        "--jobs", type=int, help="Number of parallel compilation jobs"
    )

    # Compilation options
    compile_group = parser.add_argument_group("Compilation options")
    compile_group.add_argument(
        "--include-path",
        "-I",
        action="append",
        dest="include_paths",
        help="Add include directory",
    )
    compile_group.add_argument(
        "--define",
        "-D",
        action="append",
        dest="defines",
        help="Add preprocessor definition",
    )
    compile_group.add_argument(
        "--warnings", "-W", action="append", help="Add warning flags"
    )
    compile_group.add_argument("--optimization", "-O", help="Set optimization level")
    compile_group.add_argument(
        "--debug", "-g", action="store_true", help="Include debug information"
    )
    compile_group.add_argument(
        "--pic", action="store_true", help="Generate position-independent code"
    )
    compile_group.add_argument("--stdlib", help="Specify standard library to use")
    compile_group.add_argument(
        "--sanitize", action="append", dest="sanitizers", help="Enable sanitizer"
    )

    # Linking options
    link_group = parser.add_argument_group("Linking options")
    link_group.add_argument(
        "--library-path",
        "-L",
        action="append",
        dest="library_paths",
        help="Add library directory",
    )
    link_group.add_argument(
        "--library",
        "-l",
        action="append",
        dest="libraries",
        help="Add library to link against",
    )
    link_group.add_argument(
        "--shared", action="store_true", help="Create a shared library"
    )
    link_group.add_argument(
        "--static", action="store_true", help="Prefer static linking"
    )
    link_group.add_argument("--strip", action="store_true", help="Strip debug symbols")
    link_group.add_argument("--map-file", help="Generate map file")

    # Additional flags
    parser.add_argument(
        "--compile-flags", nargs="*", help="Additional compilation flags"
    )
    parser.add_argument("--link-flags", nargs="*", help="Additional linking flags")
    parser.add_argument(
        "--flags", nargs="*", help="Additional flags for both compilation and linking"
    )
    parser.add_argument(
        "--config", type=Path, help="Load options from configuration file (JSON)"
    )

    # Output control
    parser.add_argument(
        "--verbose", "-v", action="count", default=0, help="Increase verbosity"
    )
    parser.add_argument(
        "--quiet", "-q", action="store_true", help="Suppress non-error output"
    )
    parser.add_argument(
        "--list-compilers",
        action="store_true",
        help="List available compilers and exit",
    )

    args = parser.parse_args()

    # Configure logging based on verbosity
    logger.remove()  # Remove default handler
    if args.quiet:
        logger.add(sys.stderr, level="WARNING")
    elif args.verbose == 1:
        logger.add(sys.stderr, level="INFO")
    elif args.verbose >= 2:
        logger.add(sys.stderr, level="DEBUG")
    else:
        # Default level
        logger.add(sys.stderr, level="INFO")

    # Create compiler manager
    compiler_manager = CompilerManager()

    # Handle list-compilers flag
    if args.list_compilers:
        compilers = compiler_manager.detect_compilers()
        if compilers:
            print("Available compilers:")
            for name, compiler in compilers.items():
                print(
                    f"  {name}: {compiler.config.command} (version: {compiler.config.version})"
                )
            print(f"Default compiler: {compiler_manager.default_compiler}")
        else:
            print("No supported compilers found.")
        return 0

    # Parse C++ version
    from .core_types import CppVersion

    cpp_version = CppVersion.resolve_version(args.cpp_version)

    # Prepare compile options
    compile_options_dict = {}

    if args.include_paths:
        compile_options_dict["include_paths"] = args.include_paths

    if args.defines:
        defines = {}
        for define in args.defines:
            if "=" in define:
                name, value = define.split("=", 1)
                defines[name] = value
            else:
                defines[define] = None
        compile_options_dict["defines"] = defines

    if args.warnings:
        compile_options_dict["warnings"] = args.warnings

    if args.optimization:
        compile_options_dict["optimization"] = args.optimization

    if args.debug:
        compile_options_dict["debug"] = True

    if args.pic:
        compile_options_dict["position_independent"] = True

    if args.stdlib:
        compile_options_dict["standard_library"] = args.stdlib

    if args.sanitizers:
        compile_options_dict["sanitizers"] = args.sanitizers

    if args.compile_flags:
        compile_options_dict["extra_flags"] = args.compile_flags

    # Prepare link options
    link_options_dict = {}

    if args.library_paths:
        link_options_dict["library_paths"] = args.library_paths

    if args.libraries:
        link_options_dict["libraries"] = args.libraries

    if args.shared:
        link_options_dict["shared"] = True

    if args.static:
        link_options_dict["static"] = True

    if args.strip:
        link_options_dict["strip_symbols"] = True

    if args.map_file:
        link_options_dict["map_file"] = args.map_file

    if args.link_flags:
        link_options_dict["extra_flags"] = args.link_flags

    # Load configuration from file if provided
    if args.config:
        try:
            config = load_json(args.config)

            # Update compile options
            if "compile_options" in config:
                for key, value in config["compile_options"].items():
                    compile_options_dict[key] = value

            # Update link options
            if "link_options" in config:
                for key, value in config["link_options"].items():
                    link_options_dict[key] = value

            # General options can override specific ones
            if "options" in config:
                if "compiler" in config["options"] and not args.compiler:
                    args.compiler = config["options"]["compiler"]
                if "cpp_version" in config["options"] and cpp_version == "c++17":
                    cpp_version = config["options"]["cpp_version"]
                if "incremental" in config["options"] and not args.incremental:
                    args.incremental = config["options"]["incremental"]
                if "build_dir" in config["options"] and not args.build_dir:
                    args.build_dir = config["options"]["build_dir"]

        except Exception as e:
            logger.error(f"Failed to load configuration file: {e}")
            return 1

    # Combine extra flags if provided
    if args.flags:
        if "extra_flags" not in compile_options_dict:
            compile_options_dict["extra_flags"] = []
        compile_options_dict["extra_flags"].extend(args.flags)

        if "extra_flags" not in link_options_dict:
            link_options_dict["extra_flags"] = []
        link_options_dict["extra_flags"].extend(args.flags)

    # Create proper instances
    compile_options = CompileOptions(**compile_options_dict)
    link_options = LinkOptions(**link_options_dict) if link_options_dict else None

    # Set up build manager
    build_manager = BuildManager(
        compiler_manager=compiler_manager,
        build_dir=args.build_dir,
        parallel=args.parallel,
        max_workers=args.jobs,
    )

    # Execute build
    result = build_manager.build(
        source_files=args.source_files,
        output_file=args.output,
        compiler_name=args.compiler,
        cpp_version=cpp_version,
        compile_options=compile_options,
        link_options=link_options if args.link else None,
        incremental=args.incremental,
        force_rebuild=args.force_rebuild,
    )

    # Print result
    if result.success:
        logger.info(
            f"Build successful: {result.output_file} (took {result.duration_ms:.2f}ms)"
        )
        for warning in result.warnings:
            logger.warning(warning)
        return 0
    else:
        logger.error("Build failed:")
        for error in result.errors:
            logger.error(f"  {error}")
        for warning in result.warnings:
            logger.warning(f"  {warning}")
        return 1
