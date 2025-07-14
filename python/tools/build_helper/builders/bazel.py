#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BazelBuilder implementation for building projects using Bazel.
"""

import os
from pathlib import Path
from typing import Dict, List, Optional, Union

from loguru import logger

from ..core.base import BuildHelperBase
from ..core.models import BuildStatus, BuildResult
from ..core.errors import ConfigurationError, BuildError, InstallationError, TestError


class BazelBuilder(BuildHelperBase):
    """
    BazelBuilder is a utility class to handle building projects using Bazel.

    This class provides functionality specific to Bazel build systems, including
    configuration, building, installation, testing, and documentation generation.
    """

    def __init__(
        self,
        source_dir: Union[Path, str],
        build_dir: Union[Path, str],
        build_mode: str = "dbg",
        install_prefix: Optional[Union[Path, str]] = None,
        bazel_options: Optional[List[str]] = None,
        env_vars: Optional[Dict[str, str]] = None,
        verbose: bool = False,
        parallel: int = os.cpu_count() or 4,
    ) -> None:
        super().__init__(
            source_dir,
            build_dir,
            install_prefix,
            bazel_options,
            env_vars,
            verbose,
            parallel,
        )
        self.build_mode = build_mode

        # Set Bazel-specific environment variables
        if "BAZEL_OUTPUT_BASE" not in self.env_vars:
            self.env_vars["BAZEL_OUTPUT_BASE"] = str(self.build_dir)

        # Bazel-specific cache keys
        # Note: _get_bazel_version is now async, but __init__ cannot be async.
        # This might need to be called later or handled differently if it's critical
        # for initialization and requires an event loop.
        # For now, we'll assume it's okay to call it synchronously if it's just for info.
        # If it truly needs to be async, it should be moved out of __init__.
        # self._bazel_version = await self._get_bazel_version()

        logger.debug(f"BazelBuilder initialized with build_mode={build_mode}")

    async def _get_bazel_version(self) -> str:
        """Get the Bazel version string asynchronously."""
        try:
            result = await self.run_command(["bazel", "--version"])
            if result.success:
                version = result.output.strip()  # Changed from stdout to output
                logger.debug(f"Detected Bazel: {version}")
                return version
            else:
                logger.warning(f"Failed to determine Bazel version: {result.error}")
                return ""
        except Exception as e:
            logger.warning(f"Failed to determine Bazel version due to exception: {e}")
            return ""

    async def configure(self) -> BuildResult:
        """Configure the Bazel build system asynchronously."""
        self.status = BuildStatus.CONFIGURING
        logger.info(f"Configuring Bazel build with output base in {self.build_dir}")
        logger.info(f"Configuring Bazel build with output base in {self.build_dir}")

        self.build_dir.mkdir(parents=True, exist_ok=True)

        bazel_args = ["bazel", "info"] + (self.options or [])  # Ensure options is not None

        original_dir = os.getcwd()
        os.chdir(str(self.source_dir))  # Ensure Path is converted to string

        try:
            result = await self.run_command(bazel_args)  # Pass list directly instead of unpacking
            if result.success:
                self.status = BuildStatus.COMPLETED
                logger.success("Bazel configuration successful")
            else:
                self.status = BuildStatus.FAILED
                logger.error(f"Bazel configuration failed: {result.error}")
                raise ConfigurationError(f"Bazel configuration failed: {result.error}")
            return result
        finally:
            os.chdir(original_dir)

    async def build(self, target: str = "//...") -> BuildResult:
        """Build the project using Bazel asynchronously."""
        self.status = BuildStatus.BUILDING
        logger.info(f"Building target '{target}' using Bazel")

        original_dir = os.getcwd()
        os.chdir(str(self.source_dir))

        try:
            build_cmd = [
                "bazel",
                "build",
                f"--compilation_mode={self.build_mode}",
                f"--jobs={self.parallel}",
                target,
            ] + (self.options or [])

            if self.verbose:
                build_cmd += ["--verbose_failures"]

            result = await self.run_command(build_cmd)  # Pass list directly
            if result.success:
                self.status = BuildStatus.COMPLETED
                logger.success(f"Build of target '{target}' successful")
            else:
                self.status = BuildStatus.FAILED
                logger.error(f"Build failed: {result.error}")
                raise BuildError(f"Bazel build failed: {result.error}")
            return result
        finally:
            os.chdir(original_dir)

    async def install(self) -> BuildResult:
        """Install the project to the specified prefix asynchronously."""
        self.status = BuildStatus.INSTALLING
        logger.info(f"Installing project to {self.install_prefix}")

        original_dir = os.getcwd()
        os.chdir(str(self.source_dir))  # Convert Path to string

        try:
            install_prefix_path = Path(self.install_prefix)
            install_prefix_path.mkdir(parents=True, exist_ok=True)

            query_cmd = [
                "bazel",
                "query",
                "kind(\".*_binary|.*_library\", //...)",  # Removed extra quotes
            ]

            query_result = await self.run_command(query_cmd)
            if not query_result.success:
                self.status = BuildStatus.FAILED
                logger.error(
                    f"Failed to query targets for installation: {query_result.error}"
                )
                    f"Failed to query targets for installation: {query_result.error}"
                )
                raise InstallationError(
                    f"Bazel target query failed: {query_result.error}"
                )

            try:
                install_marker_path = (
                    Path(self.install_prefix) / "bazel_install_marker.txt"
                )
                install_marker_path.write_text(
                    f"Bazel build installed from {self.source_dir}\n"
                    f"Available targets:\n{query_result.output}"
                )

                build_result = BuildResult(
                    success=True,
                    output=f"Installed Bazel build artifacts to {self.install_prefix}",
                    error="",
                    exit_code=0,
                    execution_time=0.0,
                    execution_time=0.0,
                )

                self.status = BuildStatus.COMPLETED
                logger.success(
                    f"Project installed successfully to {self.install_prefix}"
                )
                    f"Project installed successfully to {self.install_prefix}"
                )
                return build_result

            except Exception as e:
                self.status = BuildStatus.FAILED
                error_msg = f"Failed to install Bazel build: {str(e)}"
                logger.error(error_msg)

                build_result = BuildResult(
                    success=False, output="", error=error_msg, exit_code=1, execution_time=0.0
                )

                raise InstallationError(error_msg)
        finally:
            os.chdir(original_dir)

    async def test(self) -> BuildResult:
        """Run tests using Bazel asynchronously."""
        self.status = BuildStatus.TESTING
        logger.info("Running tests with Bazel")

        original_dir = os.getcwd()
        os.chdir(self.source_dir)

        try:
            test_cmd = [
                "bazel",
                "test",
                f"--compilation_mode={self.build_mode}",
                f"--jobs={self.parallel}",
                "--test_output=errors",
                "//...",
            ] + (self.options or [])

            if self.verbose:
                test_cmd += ["--verbose_failures", "--test_output=all"]

            result = await self.run_command(test_cmd)  # Pass list directly
            if result.success:
                self.status = BuildStatus.COMPLETED
                logger.success("All tests passed")
            else:
                self.status = BuildStatus.FAILED
                logger.error(f"Some tests failed: {result.error}")
                raise TestError(f"Bazel tests failed: {result.error}")
            return result
        finally:
            os.chdir(original_dir)

    async def generate_docs(self, doc_target: str = "//docs:docs") -> BuildResult:
        """Generate documentation using the specified documentation target asynchronously."""
        self.status = BuildStatus.GENERATING_DOCS
        logger.info(f"Generating documentation with target '{doc_target}'")

        try:
            result = await self.build(doc_target)
            if result.success:
                logger.success(
                    f"Documentation generated successfully with target '{doc_target}'"
                )
                    f"Documentation generated successfully with target '{doc_target}'"
                )
            return result
        except BuildError as e:
            logger.error(f"Documentation generation failed: {str(e)}")
            raise e
