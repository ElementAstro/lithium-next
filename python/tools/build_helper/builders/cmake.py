#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CMakeBuilder implementation for building projects using CMake.
"""

import os
import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Union

from loguru import logger

from ..core.base import BuildHelperBase
from ..core.models import BuildStatus, BuildResult
from ..core.errors import ConfigurationError, BuildError, InstallationError, TestError


class CMakeBuilder(BuildHelperBase):
    """
    CMakeBuilder is a utility class to handle building projects using CMake.

    This class provides functionality specific to CMake build systems, including
    configuration, building, installation, testing, and documentation generation.
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
            source_dir,
            build_dir,
            install_prefix,
            cmake_options,
            env_vars,
            verbose,
            parallel
        )
        self.generator = generator
        self.build_type = build_type

        # CMake-specific cache keys
        # self._cmake_version = await self._get_cmake_version() # Cannot call async in __init__

        logger.debug(
            f"CMakeBuilder initialized with generator={generator}, build_type={build_type}")

    async def _get_cmake_version(self) -> str:
        """Get the CMake version string asynchronously."""
        try:
            result = await self.run_command(["cmake", "--version"])
            if result.success:
                version_line = result.output.strip().split('\n')[0]
                logger.debug(f"Detected CMake: {version_line}")
                return version_line
            else:
                logger.warning(
                    f"Failed to determine CMake version: {result.error}")
                return ""
        except Exception as e:
            logger.warning(
                f"Failed to determine CMake version due to exception: {e}")
            return ""

    async def configure(self) -> BuildResult:
        """Configure the CMake build system asynchronously."""
        self.status = BuildStatus.CONFIGURING
        logger.info(f"Configuring CMake build in {self.build_dir}")

        self.build_dir.mkdir(parents=True, exist_ok=True)

        cmake_args = [
            "cmake",
            f"-G{self.generator}",
            f"-DCMAKE_BUILD_TYPE={self.build_type}",
            f"-DCMAKE_INSTALL_PREFIX={self.install_prefix}",
            str(self.source_dir),
        ] + (self.options or [])

        # Fixed: Pass the list directly instead of unpacking with *
        result = await self.run_command(cmake_args)

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success("CMake configuration successful")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"CMake configuration failed: {result.error}")
            raise ConfigurationError(
                f"CMake configuration failed: {result.error}")

        return result

    async def build(self, target: str = "") -> BuildResult:
        """Build the project using CMake asynchronously."""
        self.status = BuildStatus.BUILDING
        logger.info(
            f"Building {'target ' + target if target else 'project'} using CMake")

        build_cmd = [
            "cmake",
            "--build",
            str(self.build_dir),
            "--parallel",
            str(self.parallel)
        ]

        if target:
            build_cmd += ["--target", target]

        if self.verbose:
            build_cmd += ["--verbose"]

        # Fixed: Pass the list directly instead of unpacking with *
        result = await self.run_command(build_cmd)

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success(
                f"Build of {'target ' + target if target else 'project'} successful")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Build failed: {result.error}")
            raise BuildError(f"CMake build failed: {result.error}")

        return result

    async def install(self) -> BuildResult:
        """Install the project to the specified prefix asynchronously."""
        self.status = BuildStatus.INSTALLING
        logger.info(f"Installing project to {self.install_prefix}")

        # Fixed: Pass as a list instead of separate arguments
        result = await self.run_command([
            "cmake",
            "--install",
            str(self.build_dir)
        ])

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success(
                f"Project installed successfully to {self.install_prefix}")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Installation failed: {result.error}")
            raise InstallationError(
                f"CMake installation failed: {result.error}")

        return result

    async def test(self) -> BuildResult:
        """Run tests using CTest with detailed output on failure asynchronously."""
        self.status = BuildStatus.TESTING
        logger.info("Running tests with CTest")

        ctest_cmd = [
            "ctest",
            "--output-on-failure",
            "-C",
            self.build_type,
            "-j",
            str(self.parallel)
        ]

        if self.verbose:
            ctest_cmd.append("-V")

        ctest_cmd.extend(["-S", str(self.build_dir)])

        # Fixed: Pass the list directly instead of unpacking with *
        result = await self.run_command(ctest_cmd)

        if result.success:
            self.status = BuildStatus.COMPLETED
            logger.success("All tests passed")
        else:
            self.status = BuildStatus.FAILED
            logger.error(f"Some tests failed: {result.error}")
            raise TestError(f"CTest tests failed: {result.error}")

        return result

    async def generate_docs(self, doc_target: str = "doc") -> BuildResult:
        """Generate documentation using the specified documentation target asynchronously."""
        self.status = BuildStatus.GENERATING_DOCS
        logger.info(f"Generating documentation with target '{doc_target}'")

        try:
            result = await self.build(doc_target)
            if result.success:
                logger.success(
                    f"Documentation generated successfully with target '{doc_target}'")
            return result
        except BuildError as e:
            logger.error(f"Documentation generation failed: {str(e)}")
            raise e
