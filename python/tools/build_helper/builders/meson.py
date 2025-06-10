#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MesonBuilder implementation for building projects using Meson.
"""

import os
import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Union

from loguru import logger

from ..core.base import BuildHelperBase
from ..core.models import BuildStatus, BuildResult
from ..core.errors import ConfigurationError, BuildError, InstallationError, TestError


class MesonBuilder(BuildHelperBase):
    """
    MesonBuilder is a utility class to handle building projects using Meson.

    This class provides functionality specific to Meson build systems, including
    configuration, building, installation, testing, and documentation generation.
    """

    def __init__(
        self,
        source_dir: Union[Path, str],
        build_dir: Union[Path, str],
        build_type: str = "debug",
        install_prefix: Optional[Union[Path, str]] = None,
        meson_options: Optional[List[str]] = None,
        env_vars: Optional[Dict[str, str]] = None,
        verbose: bool = False,
        parallel: int = os.cpu_count() or 4,
    ) -> None:
        super().__init__(
            source_dir,
            build_dir,
            install_prefix,
            meson_options,
            env_vars,
            verbose,
            parallel
        )
        self.build_type = build_type

        # Meson-specific cache keys
        self._meson_version = self._get_meson_version()

        logger.debug(f"MesonBuilder initialized with build_type={build_type}")

    def _get_meson_version(self) -> str:
        """Get the Meson version string."""
        try:
            result = subprocess.run(
                ["meson", "--version"],
                capture_output=True,
                text=True,
                check=True
            )
            version = result.stdout.strip()
            logger.debug(f"Detected Meson: {version}")
            return version
        except subprocess.SubprocessError:
            logger.warning("Failed to determine Meson version")
            return ""

    def configure(self) -> BuildResult:
        """Configure the Meson build system."""
        self.status = BuildStatus.CONFIGURING
        logger.info(f"Configuring Meson build in {self.build_dir}")

        # Create build directory if it doesn't exist
        self.build_dir.mkdir(parents=True, exist_ok=True)

        # Construct Meson setup command
        meson_args = [
            "meson",
            "setup",
            str(self.build_dir),
            str(self.source_dir),
            f"--buildtype={self.build_type}",
            f"--prefix={self.install_prefix}",
        ] + self.options

        # Add verbosity flag if requested
        if self.verbose:
            meson_args.append("--verbose")

        # Run Meson setup
        result = self.run_command(*meson_args)

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success("Meson configuration successful")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Meson configuration failed: {result.error}")
            raise ConfigurationError(
                f"Meson configuration failed: {result.error}")

        return result

    def build(self, target: str = "") -> BuildResult:
        """Build the project using Meson."""
        self.status = BuildStatus.BUILDING
        logger.info(
            f"Building {'target ' + target if target else 'project'} using Meson")

        # Construct Meson compile command
        build_cmd = [
            "meson",
            "compile",
            "-C",
            str(self.build_dir),
            f"-j{self.parallel}"
        ]

        # Add target if specified
        if target:
            build_cmd.append(target)

        # Add verbosity flag if requested
        if self.verbose:
            build_cmd.append("--verbose")

        # Run Meson compile
        result = self.run_command(*build_cmd)

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success(
                f"Build of {'target ' + target if target else 'project'} successful")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Build failed: {result.error}")
            raise BuildError(f"Meson build failed: {result.error}")

        return result

    def install(self) -> BuildResult:
        """Install the project to the specified prefix."""
        self.status = BuildStatus.INSTALLING
        logger.info(f"Installing project to {self.install_prefix}")

        # Run Meson install
        result = self.run_command(
            "meson", "install", "-C", str(self.build_dir))

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success(
                f"Project installed successfully to {self.install_prefix}")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Installation failed: {result.error}")
            raise InstallationError(
                f"Meson installation failed: {result.error}")

        return result

    def test(self) -> BuildResult:
        """Run tests using Meson, with error logs printed on failures."""
        self.status = BuildStatus.TESTING
        logger.info("Running tests with Meson")

        # Construct Meson test command
        test_cmd = [
            "meson",
            "test",
            "-C",
            str(self.build_dir),
            "--print-errorlogs"
        ]

        if self.verbose:
            test_cmd.append("-v")

        # Run Meson test
        result = self.run_command(*test_cmd)

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success("All tests passed")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Some tests failed: {result.error}")
            raise TestError(f"Meson tests failed: {result.error}")

        return result

    def generate_docs(self, doc_target: str = "doc") -> BuildResult:
        """Generate documentation using the specified documentation target."""
        self.status = BuildStatus.GENERATING_DOCS
        logger.info(f"Generating documentation with target '{doc_target}'")

        try:
            # Build the documentation target
            result = self.build(doc_target)
            if result.success:
                logger.success(
                    f"Documentation generated successfully with target '{doc_target}'")
            return result
        except BuildError as e:
            logger.error(f"Documentation generation failed: {str(e)}")
            raise e
