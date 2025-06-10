#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Base class for build helpers providing shared functionality.
"""

import os
import json
import shutil
import subprocess
import time
import asyncio
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Dict, List, Any, Optional, cast, Union

from loguru import logger

from .models import BuildStatus, BuildResult, BuildOptions
from .errors import BuildSystemError


class BuildHelperBase(ABC):
    """
    Abstract base class for build helpers providing shared functionality.

    This class defines the common interface and behavior for all build system
    implementations.

    Attributes:
        source_dir (Path): Path to the source directory.
        build_dir (Path): Path to the build directory.
        install_prefix (Path): Directory where the project will be installed.
        options (List[str]): Additional options for the build system.
        env_vars (Dict[str, str]): Environment variables for the build process.
        verbose (bool): Flag to enable verbose output during execution.
        parallel (int): Number of parallel jobs to use for building.
    """

    def __init__(
        self,
        source_dir: Union[Path, str],
        build_dir: Union[Path, str],
        install_prefix: Optional[Union[Path, str]] = None,
        options: Optional[List[str]] = None,
        env_vars: Optional[Dict[str, str]] = None,
        verbose: bool = False,
        parallel: int = os.cpu_count() or 4,
    ) -> None:
        # Convert string paths to Path objects if necessary
        self.source_dir = source_dir if isinstance(
            source_dir, Path) else Path(source_dir)
        self.build_dir = build_dir if isinstance(
            build_dir, Path) else Path(build_dir)
        self.install_prefix = (
            install_prefix if install_prefix is not None
            else self.build_dir / "install"
        )
        if isinstance(self.install_prefix, str):
            self.install_prefix = Path(self.install_prefix)

        self.options = options or []
        self.env_vars = env_vars or {}
        self.verbose = verbose
        self.parallel = parallel

        # Build status tracking
        self.status = BuildStatus.NOT_STARTED
        self.last_result: Optional[BuildResult] = None

        # Setup caching
        self.cache_file = self.build_dir / ".build_cache.json"
        self._cache: Dict[str, Any] = {}
        self._load_cache()

        # Ensure build directory exists
        self.build_dir.mkdir(parents=True, exist_ok=True)

        logger.debug(
            f"Initialized {self.__class__.__name__} with source={self.source_dir}, build={self.build_dir}")

    def _load_cache(self) -> None:
        """Load the build cache from disk if it exists."""
        if self.cache_file.exists():
            try:
                with open(self.cache_file, "r") as f:
                    self._cache = json.load(f)
                logger.debug(f"Loaded build cache from {self.cache_file}")
            except (json.JSONDecodeError, IOError) as e:
                logger.warning(f"Failed to load build cache: {e}")
                self._cache = {}
        else:
            self._cache = {}

    def _save_cache(self) -> None:
        """Save the build cache to disk."""
        try:
            self.cache_file.parent.mkdir(parents=True, exist_ok=True)
            with open(self.cache_file, "w") as f:
                json.dump(self._cache, f)
            logger.debug(f"Saved build cache to {self.cache_file}")
        except IOError as e:
            logger.warning(f"Failed to save build cache: {e}")

    def run_command(self, *cmd: str) -> BuildResult:
        """
        Run a shell command with environment variables and logging.

        Args:
            *cmd (str): The command and its arguments as separate strings.

        Returns:
            BuildResult: Object containing the execution status and details.
        """
        cmd_str = " ".join(cmd)
        logger.info(f"Running: {cmd_str}")

        env = os.environ.copy()
        env.update(self.env_vars)

        start_time = time.time()

        try:
            result = subprocess.run(
                cmd,
                check=True,
                capture_output=True,
                text=True,
                env=env
            )
            end_time = time.time()

            # Create BuildResult object
            build_result = BuildResult(
                success=True,
                output=result.stdout,
                error=result.stderr,
                exit_code=result.returncode,
                execution_time=end_time - start_time
            )

            if self.verbose:
                logger.info(result.stdout)
                if result.stderr:
                    logger.warning(result.stderr)

            self.last_result = build_result
            return build_result

        except subprocess.CalledProcessError as e:
            end_time = time.time()

            # Create BuildResult object for the error
            build_result = BuildResult(
                success=False,
                output=e.stdout if e.stdout else "",
                error=e.stderr if e.stderr else str(e),
                exit_code=e.returncode,
                execution_time=end_time - start_time
            )

            logger.error(f"Command failed: {cmd_str}")
            logger.error(f"Error message: {build_result.error}")

            self.last_result = build_result
            self.status = BuildStatus.FAILED
            return build_result

    async def run_command_async(self, *cmd: str) -> BuildResult:
        """
        Run a shell command asynchronously with environment variables and logging.

        Args:
            *cmd (str): The command and its arguments as separate strings.

        Returns:
            BuildResult: Object containing the execution status and details.
        """
        cmd_str = " ".join(cmd)
        logger.info(f"Running async: {cmd_str}")

        env = os.environ.copy()
        env.update(self.env_vars)

        start_time = time.time()

        try:
            # Create subprocess
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                env=env
            )

            # Wait for the subprocess to complete and capture output
            stdout, stderr = await process.communicate()
            exit_code = process.returncode
            end_time = time.time()

            success = exit_code == 0

            # Create BuildResult object
            build_result = BuildResult(
                success=success,
                output=stdout.decode() if isinstance(stdout, bytes) else str(stdout),
                error=stderr.decode() if isinstance(stderr, bytes) else str(stderr),
                exit_code=exit_code or 0,
                execution_time=end_time - start_time
            )

            if self.verbose:
                if build_result.output:
                    logger.info(build_result.output)
                if build_result.error:
                    logger.warning(build_result.error)

            self.last_result = build_result
            if not success:
                self.status = BuildStatus.FAILED

            return build_result

        except Exception as e:
            end_time = time.time()

            # Create BuildResult object for the error
            build_result = BuildResult(
                success=False,
                output="",
                error=str(e),
                exit_code=1,
                execution_time=end_time - start_time
            )

            logger.error(f"Async command failed: {cmd_str}")
            logger.error(f"Error message: {str(e)}")

            self.last_result = build_result
            self.status = BuildStatus.FAILED
            return build_result

    def clean(self) -> BuildResult:
        """
        Clean the build directory by removing all files and subdirectories.

        Returns:
            BuildResult: Object containing the execution status and details.
        """
        self.status = BuildStatus.CLEANING
        logger.info(f"Cleaning build directory: {self.build_dir}")

        start_time = time.time()
        success = True
        error_message = ""

        try:
            # Save cache to reload after cleaning
            cache_content = None
            if self.cache_file.exists():
                try:
                    with open(self.cache_file, "r") as f:
                        cache_content = f.read()
                except IOError as e:
                    logger.warning(
                        f"Failed to backup cache before cleaning: {e}")

            # Remove all contents of the build directory
            if self.build_dir.exists():
                for item in self.build_dir.iterdir():
                    if item == self.cache_file:
                        # Skip the cache file
                        continue

                    try:
                        if item.is_dir():
                            shutil.rmtree(item)
                        else:
                            item.unlink()
                    except Exception as e:
                        success = False
                        error_message += f"Error removing {item}: {str(e)}\n"
            else:
                # Create the build directory if it doesn't exist
                self.build_dir.mkdir(parents=True, exist_ok=True)

            # Restore cache if it was backed up
            if cache_content is not None:
                try:
                    with open(self.cache_file, "w") as f:
                        f.write(cache_content)
                except IOError as e:
                    logger.warning(
                        f"Failed to restore cache after cleaning: {e}")

        except Exception as e:
            success = False
            error_message = str(e)
            logger.error(f"Error during cleaning: {error_message}")

        end_time = time.time()

        # Create BuildResult object
        build_result = BuildResult(
            success=success,
            output=f"Cleaned build directory: {self.build_dir}" if success else "",
            error=error_message,
            exit_code=0 if success else 1,
            execution_time=end_time - start_time
        )

        if success:
            logger.success(
                f"Successfully cleaned build directory: {self.build_dir}")
            self.status = BuildStatus.COMPLETED
        else:
            logger.error(f"Failed to clean build directory: {self.build_dir}")
            self.status = BuildStatus.FAILED

        self.last_result = build_result
        return build_result

    def get_status(self) -> BuildStatus:
        """
        Get the current build status.

        Returns:
            BuildStatus: Current status of the build process.
        """
        return self.status

    def get_last_result(self) -> Optional[BuildResult]:
        """
        Get the result of the last executed command.

        Returns:
            Optional[BuildResult]: Result object of the last command or None if no command was executed.
        """
        return self.last_result

    @classmethod
    def from_options(cls, options: BuildOptions) -> 'BuildHelperBase':
        """
        Create a BuildHelperBase instance from a BuildOptions dictionary.

        This class method creates an instance of the derived class using
        the provided options dictionary.

        Args:
            options (BuildOptions): Dictionary containing build options.

        Returns:
            BuildHelperBase: Instance of the build helper.
        """
        return cls(
            source_dir=options.get('source_dir', Path('.')),
            build_dir=options.get('build_dir', Path('build')),
            install_prefix=options.get('install_prefix'),
            options=options.get('options', []),
            env_vars=options.get('env_vars', {}),
            verbose=options.get('verbose', False),
            parallel=options.get('parallel', os.cpu_count() or 4)
        )

    # Abstract methods that must be implemented by subclasses
    @abstractmethod
    def configure(self) -> BuildResult:
        """Configure the build system."""
        pass

    @abstractmethod
    def build(self, target: str = "") -> BuildResult:
        """Build the project."""
        pass

    @abstractmethod
    def install(self) -> BuildResult:
        """Install the project to the specified prefix."""
        pass

    @abstractmethod
    def test(self) -> BuildResult:
        """Run the project's tests."""
        pass

    @abstractmethod
    def generate_docs(self, doc_target: str = "doc") -> BuildResult:
        """Generate documentation for the project."""
        pass
