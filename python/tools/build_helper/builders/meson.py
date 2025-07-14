#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MesonBuilder implementation for building projects using Meson.
"""

import os
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
            parallel,
        )
        self.build_type = build_type

        # Meson-specific cache keys
        # self._meson_version = await self._get_meson_version() # Cannot call async in __init__

        logger.debug(f"MesonBuilder initialized with build_type={build_type}")

    async def _get_meson_version(self) -> str:
        """Get the Meson version string asynchronously."""
        try:
            result = await self.run_command(["meson", "--version"])
            if result.success:
                version = result.output.strip()  # Changed from stdout to output
                logger.debug(f"Detected Meson: {version}")
                return version
            else:
                logger.warning(f"Failed to determine Meson version: {result.error}")
                return ""
        except Exception as e:
            logger.warning(f"Failed to determine Meson version due to exception: {e}")
            return ""

    async def configure(self) -> BuildResult:
        """Configure the Meson build system asynchronously."""
        self.status = BuildStatus.CONFIGURING
        logger.info(f"Configuring Meson build in {self.build_dir}")

        self.build_dir.mkdir(parents=True, exist_ok=True)

        meson_args = [
            "meson",
            "setup",
            str(self.build_dir),
            str(self.source_dir),
            f"--buildtype={self.build_type}",
            f"--prefix={self.install_prefix}",
        ] + (
            self.options or []
        )  # Ensure options is not None

        if self.verbose:
            meson_args.append("--verbose")

        # Fixed: Pass the list directly instead of unpacking with *
        result = await self.run_command(meson_args)

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success("Meson configuration successful")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Meson configuration failed: {result.error}")
            raise ConfigurationError(f"Meson configuration failed: {result.error}")

        return result

    async def build(self, target: str = "") -> BuildResult:
        """Build the project using Meson asynchronously."""
        self.status = BuildStatus.BUILDING
        logger.info(
            f"Building {'target ' + target if target else 'project'} using Meson"
        )

        build_cmd = [
            "meson",
            "compile",
            "-C",
            str(self.build_dir),
            f"-j{self.parallel}",
        ]

        if target:
            build_cmd.append(target)

        if self.verbose:
            build_cmd.append("--verbose")

        # Fixed: Pass the list directly instead of unpacking with *
        result = await self.run_command(build_cmd)

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success(
                f"Build of {'target ' + target if target else 'project'} successful"
            )
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Build failed: {result.error}")
            raise BuildError(f"Meson build failed: {result.error}")

        return result

    async def install(self) -> BuildResult:
        """Install the project to the specified prefix asynchronously."""
        self.status = BuildStatus.INSTALLING
        logger.info(f"Installing project to {self.install_prefix}")

        # Fixed: Pass as a list instead of separate arguments
        result = await self.run_command(["meson", "install", "-C", str(self.build_dir)])

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success(f"Project installed successfully to {self.install_prefix}")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Installation failed: {result.error}")
            raise InstallationError(f"Meson installation failed: {result.error}")

        return result

    async def test(self) -> BuildResult:
        """Run tests using Meson, with error logs printed on failures asynchronously."""
        self.status = BuildStatus.TESTING
        logger.info("Running tests with Meson")

        test_cmd = ["meson", "test", "-C", str(self.build_dir), "--print-errorlogs"]

        if self.verbose:
            test_cmd.append("-v")

        # Fixed: Pass the list directly instead of unpacking with *
        result = await self.run_command(test_cmd)

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success("All tests passed")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Some tests failed: {result.error}")
            raise TestError(f"Meson tests failed: {result.error}")

        return result

    async def generate_docs(self, doc_target: str = "doc") -> BuildResult:
        """Generate documentation using the specified documentation target asynchronously."""
        self.status = BuildStatus.GENERATING_DOCS
        logger.info(f"Generating documentation with target '{doc_target}'")

        try:
            result = await self.build(doc_target)
            if result.success:
                logger.success(
                    f"Documentation generated successfully with target '{doc_target}'"
                )
            return result
        except BuildError as e:
            logger.error(f"Documentation generation failed: {str(e)}")
            raise e
