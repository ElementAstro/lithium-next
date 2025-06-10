#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Command-line interface for the build system helper.
"""

import argparse
import os
import sys
from pathlib import Path
from typing import Dict, List, Any

from loguru import logger

from .core.errors import (
    BuildSystemError, ConfigurationError,
    BuildError, TestError, InstallationError
)
from .builders.cmake import CMakeBuilder
from .builders.meson import MesonBuilder
from .builders.bazel import BazelBuilder
from .utils.config import BuildConfig
from . import __version__


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Advanced Build System Helper",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    # Basic options
    parser.add_argument("--source_dir", type=Path,
                        default=Path(".").resolve(), help="Source directory")
    parser.add_argument("--build_dir", type=Path,
                        default=Path("build").resolve(), help="Build directory")
    parser.add_argument(
        "--builder", choices=["cmake", "meson", "bazel"], required=True,
        help="Choose the build system")

    # Build system specific options
    cmake_group = parser.add_argument_group("CMake options")
    cmake_group.add_argument(
        "--generator", choices=["Ninja", "Unix Makefiles"], default="Ninja",
        help="CMake generator to use")
    cmake_group.add_argument("--build_type", choices=[
        "Debug", "Release", "RelWithDebInfo", "MinSizeRel"], default="Debug",
        help="Build type for CMake")

    meson_group = parser.add_argument_group("Meson options")
    meson_group.add_argument("--meson_build_type", choices=[
        "debug", "release", "debugoptimized"], default="debug",
        help="Build type for Meson")

    bazel_group = parser.add_argument_group("Bazel options")
    bazel_group.add_argument("--bazel_mode", choices=[
        "opt", "dbg"], default="dbg",
        help="Build mode for Bazel")

    # Build actions
    parser.add_argument("--target", default="", help="Specify a build target")
    parser.add_argument("--install", action="store_true",
                        help="Install the project")
    parser.add_argument("--clean", action="store_true",
                        help="Clean the build directory")
    parser.add_argument("--test", action="store_true", help="Run the tests")

    # Options
    parser.add_argument("--cmake_options", nargs="*", default=[],
                        help="Custom CMake options (e.g. -DVAR=VALUE)")
    parser.add_argument("--meson_options", nargs="*", default=[],
                        help="Custom Meson options (e.g. -Dvar=value)")
    parser.add_argument("--bazel_options", nargs="*", default=[],
                        help="Custom Bazel options")
    parser.add_argument("--generate_docs", action="store_true",
                        help="Generate documentation")
    parser.add_argument("--doc_target", default="doc",
                        help="Documentation target name")

    # Environment and build settings
    parser.add_argument("--env", nargs="*", default=[],
                        help="Set environment variables (e.g. VAR=value)")
    parser.add_argument("--verbose", action="store_true",
                        help="Enable verbose output")
    parser.add_argument("--parallel", type=int, default=os.cpu_count() or 4,
                        help="Number of parallel jobs for building")
    parser.add_argument("--install_prefix", type=Path,
                        help="Installation prefix")

    # Logging options
    parser.add_argument("--log_level", choices=["TRACE", "DEBUG", "INFO", "SUCCESS", "WARNING", "ERROR", "CRITICAL"],
                        default="INFO", help="Set the logging level")
    parser.add_argument("--log_file", type=Path,
                        help="Log to file instead of stderr")

    # Configuration file
    parser.add_argument("--config", type=Path,
                        help="Load configuration from file (JSON, YAML, or INI)")

    # Version information
    parser.add_argument("--version", action="version",
                        version=f"Build System Helper v{__version__}")

    return parser.parse_args()


def setup_logging(args: argparse.Namespace) -> None:
    """Set up logging based on command-line arguments."""
    # Remove default handler
    logger.remove()

    # Set log level
    log_level = args.log_level
    if args.verbose and log_level == "INFO":
        log_level = "DEBUG"

    # Setup formatting
    log_format = "<green>{time:HH:mm:ss}</green> | <level>{level: <8}</level> | <level>{message}</level>"

    if log_level in ["DEBUG", "TRACE"]:
        log_format = "<green>{time:HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>"

    # Setup output sink
    if args.log_file:
        logger.add(
            args.log_file,
            level=log_level,
            format=log_format,
            rotation="10 MB",
            retention=3
        )
    else:
        logger.add(
            sys.stderr,
            level=log_level,
            format=log_format,
            colorize=True
        )

    logger.debug(f"Logging initialized at {log_level} level")


def main() -> int:
    """Main function to run the build system helper from command line."""
    args = parse_args()
    setup_logging(args)

    logger.info(f"Build System Helper v{__version__}")

    try:
        # Load configuration from file if specified
        if args.config:
            try:
                config = BuildConfig.load_from_file(args.config)
                logger.info(f"Loaded configuration from {args.config}")

                # Override file configuration with command-line arguments
                for key, value in vars(args).items():
                    if value is not None and key != "config":
                        config[key] = value

            except ValueError as e:
                logger.error(f"Failed to load configuration: {e}")
                return 1
        else:
            # Use command-line arguments
            config = {}

        # Parse environment variables from the command line
        env_vars = {}
        for var in args.env:
            try:
                name, value = var.split("=", 1)
                env_vars[name] = value
                logger.debug(f"Setting environment variable: {name}={value}")
            except ValueError:
                logger.warning(
                    f"Invalid environment variable format: {var} (expected VAR=value)")

        # Create the builder based on the specified build system
        match args.builder:
            case "cmake":
                with logger.contextualize(builder="cmake"):
                    builder = CMakeBuilder(
                        source_dir=args.source_dir,
                        build_dir=args.build_dir,
                        generator=args.generator,
                        build_type=args.build_type,
                        install_prefix=args.install_prefix,
                        cmake_options=args.cmake_options,
                        env_vars=env_vars,
                        verbose=args.verbose,
                        parallel=args.parallel,
                    )
            case "meson":
                with logger.contextualize(builder="meson"):
                    builder = MesonBuilder(
                        source_dir=args.source_dir,
                        build_dir=args.build_dir,
                        build_type=args.meson_build_type,
                        install_prefix=args.install_prefix,
                        meson_options=args.meson_options,
                        env_vars=env_vars,
                        verbose=args.verbose,
                        parallel=args.parallel,
                    )
            case "bazel":
                with logger.contextualize(builder="bazel"):
                    builder = BazelBuilder(
                        source_dir=args.source_dir,
                        build_dir=args.build_dir,
                        build_mode=args.bazel_mode,
                        install_prefix=args.install_prefix,
                        bazel_options=args.bazel_options,
                        env_vars=env_vars,
                        verbose=args.verbose,
                        parallel=args.parallel,
                    )
            case _:
                logger.error(f"Unsupported builder type: {args.builder}")
                return 1

        # Execute build operations with logging context
        with logger.contextualize(builder=args.builder):
            # Perform cleaning if requested
            if args.clean:
                try:
                    builder.clean()
                except Exception as e:
                    logger.error(f"Failed to clean build directory: {e}")
                    return 1

            # Configure the build system
            try:
                builder.configure()
            except ConfigurationError as e:
                logger.error(f"Configuration failed: {e}")
                return 1

            # Build the project with the specified target
            try:
                builder.build(args.target)
            except BuildError as e:
                logger.error(f"Build failed: {e}")
                return 1

            # Run tests if requested
            if args.test:
                try:
                    builder.test()
                except TestError as e:
                    logger.error(f"Tests failed: {e}")
                    return 1

            # Generate documentation if requested
            if args.generate_docs:
                try:
                    builder.generate_docs(args.doc_target)
                except BuildError as e:
                    logger.error(f"Documentation generation failed: {e}")
                    return 1

            # Install the project if requested
            if args.install:
                try:
                    builder.install()
                except InstallationError as e:
                    logger.error(f"Installation failed: {e}")
                    return 1

        logger.success("Build process completed successfully")
        return 0

    except Exception as e:
        logger.error(f"An error occurred: {e}")
        if args.verbose:
            logger.exception("Detailed error information:")
        return 1


if __name__ == "__main__":
    sys.exit(main())
