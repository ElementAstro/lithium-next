#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Enhanced command-line interface with modern Python features and robust error handling.
"""

from __future__ import annotations

import argparse
import os
import sys
import asyncio
from pathlib import Path
from typing import Dict, List, Any, Optional

from loguru import logger

from .core.errors import (
    BuildSystemError, ConfigurationError,
    BuildError, TestError, InstallationError
)
from .builders.cmake import CMakeBuilder
from .builders.meson import MesonBuilder
from .builders.bazel import BazelBuilder
from .utils.config import BuildConfig
from .utils.factory import BuilderFactory
from .core.models import BuildOptions, BuildSession
from . import __version__


def setup_logging(args: argparse.Namespace) -> None:
    """Set up enhanced logging with structured output and multiple sinks."""
    # Remove default handler
    logger.remove()

    # Determine log level
    log_level = args.log_level
    if args.verbose and log_level == "INFO":
        log_level = "DEBUG"

    # Enhanced formatting based on log level
    if log_level in ["DEBUG", "TRACE"]:
        log_format = (
            "<green>{time:YYYY-MM-DD HH:mm:ss.SSS}</green> | "
            "<level>{level: <8}</level> | "
            "<cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> | "
            "<level>{message}</level>"
        )
    else:
        log_format = (
            "<green>{time:HH:mm:ss}</green> | "
            "<level>{level: <8}</level> | "
            "<level>{message}</level>"
        )

    # Console sink
    logger.add(
        sys.stderr,
        level=log_level,
        format=log_format,
        colorize=True,
        enqueue=True  # Thread-safe logging
    )

    # File sink if specified
    if args.log_file:
        logger.add(
            args.log_file,
            level=log_level,
            format=log_format,
            rotation="10 MB",
            retention=3,
            compression="gz",
            enqueue=True
        )

    # Performance monitoring sink for DEBUG level
    if log_level in ["DEBUG", "TRACE"]:
        logger.add(
            args.build_dir / "build_performance.log" if args.build_dir else "build_performance.log",
            level="DEBUG",
            format="{time:YYYY-MM-DD HH:mm:ss.SSS} | {level} | {message}",
            filter=lambda record: "execution_time" in record["extra"],
            rotation="5 MB",
            retention=2
        )

    logger.debug(f"Logging initialized at {log_level} level")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments with enhanced validation."""
    parser = argparse.ArgumentParser(
        description="Advanced Build System Helper with auto-detection and enhanced error handling",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        epilog="Examples:\n"
               "  %(prog)s --builder cmake --source_dir . --build_dir build\n"
               "  %(prog)s --auto-detect --clean --test\n"
               "  %(prog)s --config build.json --install",
        add_help=False  # Custom help handling
    )

    # Help and version
    help_group = parser.add_argument_group("Help and Information")
    help_group.add_argument("-h", "--help", action="help", 
                           help="Show this help message and exit")
    help_group.add_argument("--version", action="version",
                           version=f"Build System Helper v{__version__}")
    help_group.add_argument("--list-builders", action="store_true",
                           help="List available build systems and exit")

    # Basic options
    basic_group = parser.add_argument_group("Basic Configuration")
    basic_group.add_argument("--source_dir", type=Path,
                            default=Path(".").resolve(), 
                            help="Source directory")
    basic_group.add_argument("--build_dir", type=Path,
                            default=Path("build").resolve(), 
                            help="Build directory")
    basic_group.add_argument("--builder", 
                            choices=BuilderFactory.get_available_builders(),
                            help="Choose the build system")
    basic_group.add_argument("--auto-detect", action="store_true",
                            help="Auto-detect build system from source directory")

    # Build system specific options
    cmake_group = parser.add_argument_group("CMake Options")
    cmake_group.add_argument("--generator", 
                            choices=["Ninja", "Unix Makefiles", "Visual Studio 16 2019"], 
                            default="Ninja",
                            help="CMake generator to use")
    cmake_group.add_argument("--build_type", 
                            choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], 
                            default="Debug",
                            help="Build type for CMake")

    meson_group = parser.add_argument_group("Meson Options")
    meson_group.add_argument("--meson_build_type", 
                            choices=["debug", "release", "debugoptimized"], 
                            default="debug",
                            help="Build type for Meson")

    bazel_group = parser.add_argument_group("Bazel Options")
    bazel_group.add_argument("--bazel_mode", 
                            choices=["opt", "dbg"], 
                            default="dbg",
                            help="Build mode for Bazel")

    # Build actions
    actions_group = parser.add_argument_group("Build Actions")
    actions_group.add_argument("--target", default="", 
                              help="Specify a build target")
    actions_group.add_argument("--clean", action="store_true",
                              help="Clean the build directory before building")
    actions_group.add_argument("--install", action="store_true",
                              help="Install the project after building")
    actions_group.add_argument("--test", action="store_true", 
                              help="Run tests after building")
    actions_group.add_argument("--generate_docs", action="store_true",
                              help="Generate documentation")
    actions_group.add_argument("--doc_target", default="doc",
                              help="Documentation target name")

    # Build options
    options_group = parser.add_argument_group("Build Options")
    options_group.add_argument("--cmake_options", nargs="*", default=[],
                              help="Custom CMake options (e.g. -DVAR=VALUE)")
    options_group.add_argument("--meson_options", nargs="*", default=[],
                              help="Custom Meson options (e.g. -Dvar=value)")
    options_group.add_argument("--bazel_options", nargs="*", default=[],
                              help="Custom Bazel options")

    # Environment and build settings
    env_group = parser.add_argument_group("Environment and Performance")
    env_group.add_argument("--env", nargs="*", default=[],
                          help="Set environment variables (e.g. VAR=value)")
    env_group.add_argument("--parallel", type=int, 
                          default=os.cpu_count() or 4,
                          help="Number of parallel jobs for building")
    env_group.add_argument("--install_prefix", type=Path,
                          help="Installation prefix")

    # Logging and debugging
    logging_group = parser.add_argument_group("Logging and Debugging")
    logging_group.add_argument("--verbose", action="store_true",
                              help="Enable verbose output")
    logging_group.add_argument("--log_level", 
                              choices=["TRACE", "DEBUG", "INFO", "SUCCESS", "WARNING", "ERROR", "CRITICAL"],
                              default="INFO", 
                              help="Set the logging level")
    logging_group.add_argument("--log_file", type=Path,
                              help="Log to file instead of stderr")

    # Configuration
    config_group = parser.add_argument_group("Configuration")
    config_group.add_argument("--config", type=Path,
                             help="Load configuration from file")
    config_group.add_argument("--auto-config", action="store_true",
                             help="Auto-discover configuration file")
    config_group.add_argument("--validate-config", action="store_true",
                             help="Validate configuration and exit")

    # Advanced options
    advanced_group = parser.add_argument_group("Advanced Options")
    advanced_group.add_argument("--dry-run", action="store_true",
                               help="Show what would be done without executing")
    advanced_group.add_argument("--continue-on-error", action="store_true",
                               help="Continue build process even if some steps fail")

    args = parser.parse_args()

    # Validation
    if not args.auto_detect and not args.builder and not args.list_builders:
        parser.error("Must specify either --builder or --auto-detect")

    if args.parallel < 1:
        parser.error("--parallel must be at least 1")

    return args


def validate_environment(args: argparse.Namespace) -> None:
    """Validate the build environment before proceeding."""
    # Check source directory
    if not args.source_dir.exists():
        logger.error(f"Source directory does not exist: {args.source_dir}")
        sys.exit(1)

    # Validate builder requirements if specified
    if args.builder:
        errors = BuilderFactory.validate_builder_requirements(args.builder, args.source_dir)
        if errors:
            logger.error("Builder validation failed:")
            for error in errors:
                logger.error(f"  - {error}")
            sys.exit(1)


async def amain() -> int:
    """Main asynchronous function with enhanced error handling and workflow management."""
    args = parse_args()

    # Handle special cases first
    if args.list_builders:
        print("Available build systems:")
        for builder in BuilderFactory.get_available_builders():
            info = BuilderFactory.get_builder_info(builder)
            print(f"  {builder}: {info.get('description', 'No description')}")
        return 0

    setup_logging(args)
    logger.info(f"Build System Helper v{__version__} starting")

    try:
        # Load and merge configuration
        config_options = None
        
        if args.config:
            try:
                config_options = BuildConfig.load_from_file(args.config)
                logger.info(f"Loaded configuration from {args.config}")
            except ConfigurationError as e:
                logger.error(f"Failed to load configuration: {e}")
                return 1
        elif args.auto_config:
            config_options = BuildConfig.auto_discover_config(args.source_dir)
            if config_options:
                logger.info("Auto-discovered configuration file")

        # Create BuildOptions from command line arguments
        cmd_options = BuildOptions({
            "source_dir": args.source_dir,
            "build_dir": args.build_dir,
            "install_prefix": args.install_prefix,
            "build_type": args.build_type,
            "generator": args.generator,
            "options": getattr(args, f"{args.builder}_options", []) if args.builder else [],
            "verbose": args.verbose,
            "parallel": args.parallel,
        })

        # Merge configurations (command line takes precedence)
        if config_options:
            final_options = BuildConfig.merge_configs(config_options, cmd_options)
        else:
            final_options = cmd_options

        # Validate configuration if requested
        if args.validate_config:
            warnings = BuildConfig.validate_config(final_options)
            if warnings:
                logger.warning("Configuration validation warnings:")
                for warning in warnings:
                    logger.warning(f"  - {warning}")
            else:
                logger.success("Configuration validation passed")
            return 0

        # Environment validation
        validate_environment(args)

        # Parse environment variables
        env_vars = {}
        for var in args.env:
            try:
                name, value = var.split("=", 1)
                env_vars[name] = value
                logger.debug(f"Setting environment variable: {name}={value}")
            except ValueError:
                logger.warning(f"Invalid environment variable format: {var} (expected VAR=value)")

        # Determine builder type
        builder_type = args.builder
        if args.auto_detect:
            builder_type = BuilderFactory.detect_build_system(args.source_dir)
            if not builder_type:
                logger.error(f"No supported build system detected in {args.source_dir}")
                return 1

        # Create builder instance
        try:
            if config_options:
                builder = BuilderFactory.create_from_options(builder_type, final_options)
            else:
                builder_kwargs = {
                    "install_prefix": args.install_prefix,
                    "env_vars": env_vars,
                    "verbose": args.verbose,
                    "parallel": args.parallel,
                }

                # Add builder-specific options
                if builder_type == "cmake":
                    builder_kwargs.update({
                        "generator": args.generator,
                        "build_type": args.build_type,
                        "cmake_options": args.cmake_options,
                    })
                elif builder_type == "meson":
                    builder_kwargs.update({
                        "build_type": args.meson_build_type,
                        "meson_options": args.meson_options,
                    })
                elif builder_type == "bazel":
                    builder_kwargs.update({
                        "build_mode": args.bazel_mode,
                        "bazel_options": args.bazel_options,
                    })

                builder = BuilderFactory.create_builder(
                    builder_type=builder_type,
                    source_dir=args.source_dir,
                    build_dir=args.build_dir,
                    **builder_kwargs
                )
        except ConfigurationError as e:
            logger.error(f"Failed to create builder: {e}")
            return 1

        # Execute build workflow
        session_id = f"{builder_type}_{args.source_dir.name}_{hash(str(args.source_dir))}"
        
        async with builder.build_session(session_id) as session:
            try:
                if args.dry_run:
                    logger.info("DRY RUN MODE - showing planned operations:")
                    operations = []
                    if args.clean:
                        operations.append("Clean build directory")
                    operations.extend(["Configure", "Build"])
                    if args.test:
                        operations.append("Run tests")
                    if args.generate_docs:
                        operations.append("Generate documentation")
                    if args.install:
                        operations.append("Install")
                    
                    for i, op in enumerate(operations, 1):
                        logger.info(f"  {i}. {op}")
                    return 0

                # Execute build workflow
                results = await builder.full_build_workflow(
                    clean_first=args.clean,
                    run_tests=args.test,
                    install_after_build=args.install,
                    generate_docs=args.generate_docs,
                    target=args.target
                )

                # Add results to session
                for result in results:
                    session.add_result(result)

                # Check for failures
                failed_results = [r for r in results if r.failed]
                if failed_results and not args.continue_on_error:
                    logger.error(f"Build workflow failed with {len(failed_results)} error(s)")
                    return 1

                logger.success("Build workflow completed successfully")
                
                # Log performance summary
                total_time = sum(r.execution_time for r in results)
                logger.info(f"Total execution time: {total_time:.2f}s")
                logger.info(f"Session success rate: {session.success_rate:.1%}")

                return 0

            except BuildSystemError as e:
                logger.error(f"Build failed: {e}")
                if args.verbose and e.context:
                    logger.debug(f"Error context: {e.context.to_dict()}")
                return 1

    except KeyboardInterrupt:
        logger.warning("Build interrupted by user")
        return 130  # Standard exit code for SIGINT
    except Exception as e:
        logger.exception(f"Unexpected error occurred: {e}")
        return 1


def main() -> int:
    """Main function to run the build system helper from command line."""
    try:
        return asyncio.run(amain())
    except KeyboardInterrupt:
        return 130
    except Exception as e:
        print(f"Fatal error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
