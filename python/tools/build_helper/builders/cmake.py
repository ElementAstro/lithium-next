#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CMakeBuilder implementation with enhanced error handling and modern Python features.
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Dict, List, Optional, Union

from loguru import logger

from ..core.base import BuildHelperBase
from ..core.models import BuildStatus, BuildResult
from ..core.errors import (
    ConfigurationError,
    BuildError,
    InstallationError,
    TestError,
    ErrorContext,
    handle_build_error,
)


class CMakeBuilder(BuildHelperBase):
    """
    Enhanced CMakeBuilder with modern Python features and robust error handling.

    This class provides comprehensive functionality for CMake build systems, including
    configuration, building, installation, testing, and documentation generation with
    advanced error context tracking and performance monitoring.
    """

    def __init__(
        self,
        source_dir: Union[Path, str],
        build_dir: Union[Path, str],
        generator: str = "Ninja",
        build_type: str = "Debug",
        install_prefix: Optional[Union[Path, str]] = None,
        cmake_options: Optional[List[str]] = None,
        env_vars: Optional[Dict[str, str]] = None,
        verbose: bool = False,
        parallel: int = os.cpu_count() or 4,
    ) -> None:
        super().__init__(
            source_dir=source_dir,
            build_dir=build_dir,
            install_prefix=install_prefix,
            options=cmake_options,
            env_vars=env_vars,
            verbose=verbose,
            parallel=parallel,
        )
        self.generator = generator
        self.build_type = build_type
        self._cmake_version: Optional[str] = None

        logger.debug(
            f"CMakeBuilder initialized",
            extra={
                "generator": generator,
                "build_type": build_type,
                "source_dir": str(self.source_dir),
                "build_dir": str(self.build_dir),
            },
        )

    async def _get_cmake_version(self) -> str:
        """Get the CMake version string asynchronously with caching."""
        if self._cmake_version is not None:
            return self._cmake_version

        try:
            result = await self.run_command(["cmake", "--version"])
            if result.success and result.output:
                version_line = result.output.strip().split("\n")[0]
                self._cmake_version = version_line
                logger.debug(f"Detected CMake: {version_line}")

                # Cache the version for future use
                self.set_cache_value("cmake_version", version_line)
                return version_line
            else:
                error_msg = f"Failed to determine CMake version: {result.error}"
                logger.warning(error_msg)
                return ""
        except Exception as e:
            error_msg = f"Failed to determine CMake version due to exception: {e}"
            logger.warning(error_msg)
            return ""

    async def _validate_cmake_environment(self) -> None:
        """Validate CMake environment and dependencies."""
        # Check CMake availability
        await self._get_cmake_version()

        # Validate source directory
        if not self.source_dir.exists():
            raise ConfigurationError(
                f"Source directory does not exist: {self.source_dir}",
                context=ErrorContext(working_directory=self.build_dir),
            )

        # Check for CMakeLists.txt
        cmake_file = self.source_dir / "CMakeLists.txt"
        if not cmake_file.exists():
            raise ConfigurationError(
                f"CMakeLists.txt not found in source directory: {self.source_dir}",
                context=ErrorContext(
                    working_directory=self.build_dir,
                    additional_info={"missing_file": str(cmake_file)},
                ),
            )

    async def configure(self) -> BuildResult:
        """Configure the CMake build system with enhanced error handling."""
        self.status = BuildStatus.CONFIGURING
        logger.info(f"Configuring CMake build in {self.build_dir}")

        try:
            # Validate environment before proceeding
            await self._validate_cmake_environment()

            # Ensure build directory exists
            self.build_dir.mkdir(parents=True, exist_ok=True)

            # Build CMake command
            cmake_args = [
                "cmake",
                f"-G{self.generator}",
                f"-DCMAKE_BUILD_TYPE={self.build_type}",
                f"-DCMAKE_INSTALL_PREFIX={self.install_prefix}",
                str(self.source_dir),
            ]

            # Add user-specified options
            if self.options:
                cmake_args.extend(self.options)

            logger.debug(f"CMake configure command: {' '.join(cmake_args)}")

            # Execute configuration
            result = await self.run_command(cmake_args)

            if result.success:
                self.status = BuildStatus.COMPLETED
                logger.success("CMake configuration successful")

                # Cache successful configuration
                self.set_cache_value(
                    "last_configure_success",
                    {
                        "timestamp": result.timestamp,
                        "generator": self.generator,
                        "build_type": self.build_type,
                    },
                )
            else:
                self.status = BuildStatus.FAILED
                error_msg = f"CMake configuration failed: {result.error}"
                logger.error(error_msg)

                raise ConfigurationError(
                    error_msg,
                    context=ErrorContext(
                        command=" ".join(cmake_args),
                        exit_code=result.exit_code,
                        working_directory=self.build_dir,
                        environment_vars=self.env_vars,
                        stderr=result.error,
                        execution_time=result.execution_time,
                    ),
                )

            return result

        except Exception as e:
            if not isinstance(e, ConfigurationError):
                raise handle_build_error(
                    "configure",
                    e,
                    context=ErrorContext(working_directory=self.build_dir),
                    recoverable=True,
                )
            raise

    async def build(self, target: str = "") -> BuildResult:
        """Build the project using CMake with enhanced error handling."""
        self.status = BuildStatus.BUILDING
        build_desc = f"target {target}" if target else "project"
        logger.info(f"Building {build_desc} using CMake")

        try:
            # Build command
            build_cmd = [
                "cmake",
                "--build",
                str(self.build_dir),
                "--parallel",
                str(self.parallel),
            ]

            if target:
                build_cmd.extend(["--target", target])

            if self.verbose:
                build_cmd.append("--verbose")

            logger.debug(f"CMake build command: {' '.join(build_cmd)}")

            # Execute build
            result = await self.run_command(build_cmd)

            if result.success:
                self.status = BuildStatus.COMPLETED
                logger.success(f"Build of {build_desc} successful")

                # Cache successful build info
                self.set_cache_value(
                    "last_build_success",
                    {
                        "timestamp": result.timestamp,
                        "target": target,
                        "execution_time": result.execution_time,
                    },
                )
            else:
                self.status = BuildStatus.FAILED
                error_msg = f"CMake build failed: {result.error}"
                logger.error(error_msg)

                raise BuildError(
                    error_msg,
                    target=target,
                    build_system="cmake",
                    context=ErrorContext(
                        command=" ".join(build_cmd),
                        exit_code=result.exit_code,
                        working_directory=self.build_dir,
                        environment_vars=self.env_vars,
                        stderr=result.error,
                        execution_time=result.execution_time,
                    ),
                )

            return result

        except Exception as e:
            if not isinstance(e, BuildError):
                raise handle_build_error(
                    "build",
                    e,
                    context=ErrorContext(
                        working_directory=self.build_dir,
                        additional_info={"target": target},
                    ),
                    recoverable=True,
                )
            raise

    async def install(self) -> BuildResult:
        """Install the project with enhanced error handling."""
        self.status = BuildStatus.INSTALLING
        logger.info(f"Installing project to {self.install_prefix}")

        try:
            # Ensure install directory is writable
            try:
                self.install_prefix.mkdir(parents=True, exist_ok=True)
                # Test write permissions
                test_file = self.install_prefix / ".write_test"
                test_file.touch()
                test_file.unlink()
            except OSError as e:
                raise InstallationError(
                    f"Cannot write to install directory {self.install_prefix}: {e}",
                    install_prefix=self.install_prefix,
                    permission_error=True,
                    context=ErrorContext(working_directory=self.build_dir),
                )

            # Build install command
            install_cmd = ["cmake", "--install", str(self.build_dir)]

            logger.debug(f"CMake install command: {' '.join(install_cmd)}")

            # Execute installation
            result = await self.run_command(install_cmd)

            if result.success:
                self.status = BuildStatus.COMPLETED
                logger.success(
                    f"Project installed successfully to {self.install_prefix}"
                )
            else:
                self.status = BuildStatus.FAILED
                error_msg = f"CMake installation failed: {result.error}"
                logger.error(error_msg)

                raise InstallationError(
                    error_msg,
                    install_prefix=self.install_prefix,
                    context=ErrorContext(
                        command=" ".join(install_cmd),
                        exit_code=result.exit_code,
                        working_directory=self.build_dir,
                        environment_vars=self.env_vars,
                        stderr=result.error,
                        execution_time=result.execution_time,
                    ),
                )

            return result

        except Exception as e:
            if not isinstance(e, InstallationError):
                raise handle_build_error(
                    "install",
                    e,
                    context=ErrorContext(working_directory=self.build_dir),
                    recoverable=False,
                )
            raise

    async def test(self) -> BuildResult:
        """Run tests using CTest with enhanced error handling and reporting."""
        self.status = BuildStatus.TESTING
        logger.info("Running tests with CTest")

        try:
            # Build CTest command
            ctest_cmd = [
                "ctest",
                "--output-on-failure",
                "-C",
                self.build_type,
                "-j",
                str(self.parallel),
            ]

            if self.verbose:
                ctest_cmd.append("-V")

            # Set working directory for CTest
            ctest_cmd.extend(["--test-dir", str(self.build_dir)])

            logger.debug(f"CTest command: {' '.join(ctest_cmd)}")

            # Execute tests
            result = await self.run_command(ctest_cmd)

            if result.success:
                self.status = BuildStatus.COMPLETED
                logger.success("All tests passed")

                # Try to extract test statistics from output
                test_stats = self._parse_ctest_output(result.output)
                if test_stats:
                    logger.info(f"Test results: {test_stats}")
            else:
                self.status = BuildStatus.FAILED
                error_msg = f"CTest tests failed: {result.error}"
                logger.error(error_msg)

                # Try to extract failure information
                test_stats = self._parse_ctest_output(result.output)

                raise TestError(
                    error_msg,
                    test_suite="ctest",
                    failed_tests=test_stats.get("failed", None) if test_stats else None,
                    total_tests=test_stats.get("total", None) if test_stats else None,
                    context=ErrorContext(
                        command=" ".join(ctest_cmd),
                        exit_code=result.exit_code,
                        working_directory=self.build_dir,
                        environment_vars=self.env_vars,
                        stderr=result.error,
                        stdout=result.output,
                        execution_time=result.execution_time,
                    ),
                )

            return result

        except Exception as e:
            if not isinstance(e, TestError):
                raise handle_build_error(
                    "test",
                    e,
                    context=ErrorContext(working_directory=self.build_dir),
                    recoverable=True,
                )
            raise

    def _parse_ctest_output(self, output: str) -> Optional[Dict[str, int]]:
        """Parse CTest output to extract test statistics."""
        if not output:
            return None

        try:
            lines = output.split("\n")
            for line in lines:
                if "tests passed" in line.lower():
                    # Example: "100% tests passed, 0 tests failed out of 25"
                    import re

                    match = re.search(
                        r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", line
                    )
                    if match:
                        failed = int(match.group(2))
                        total = int(match.group(3))
                        passed = total - failed
                        return {"passed": passed, "failed": failed, "total": total}
        except Exception as e:
            logger.debug(f"Failed to parse CTest output: {e}")

        return None

    async def generate_docs(self, doc_target: str = "doc") -> BuildResult:
        """Generate documentation using the specified documentation target."""
        self.status = BuildStatus.GENERATING_DOCS
        logger.info(f"Generating documentation with target '{doc_target}'")

        try:
            # Use the build method to build documentation target
            result = await self.build(doc_target)

            if result.success:
                logger.success(
                    f"Documentation generated successfully with target '{doc_target}'"
                )

            return result

        except BuildError as e:
            # Re-raise BuildError with additional context for documentation
            logger.error(f"Documentation generation failed: {str(e)}")
            new_context = e.context.additional_info.copy()
            new_context["doc_target"] = doc_target

            raise e.with_context(additional_info=new_context)

        except Exception as e:
            raise handle_build_error(
                "generate_docs",
                e,
                context=ErrorContext(
                    working_directory=self.build_dir,
                    additional_info={"doc_target": doc_target},
                ),
                recoverable=True,
            )

    async def get_build_info(self) -> Dict[str, Any]:
        """Get comprehensive build information and status."""
        cmake_version = await self._get_cmake_version()

        return {
            "builder_type": "cmake",
            "cmake_version": cmake_version,
            "generator": self.generator,
            "build_type": self.build_type,
            "source_dir": str(self.source_dir),
            "build_dir": str(self.build_dir),
            "install_prefix": str(self.install_prefix),
            "parallel": self.parallel,
            "status": self.status.value,
            "cache_info": {
                "last_configure": self.get_cache_value("last_configure_success"),
                "last_build": self.get_cache_value("last_build_success"),
                "cmake_version": self.get_cache_value("cmake_version"),
            },
        }
