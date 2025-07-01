#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Base class for build helpers providing shared asynchronous functionality.
"""

import os
import json
import shutil
import asyncio
import time
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Dict, List, Any, Optional, Union, Callable, Awaitable

from loguru import logger

from .models import BuildStatus, BuildResult, BuildOptions
from .errors import BuildError  # Changed from BuildSystemError to BuildError


class BuildHelperBase(ABC):
    """
    Abstract base class for build helpers providing shared asynchronous functionality.

    This class defines the common interface and behavior for all build system
    implementations, leveraging asyncio for non-blocking operations.

    Attributes:
        source_dir (Path): Path to the source directory.
        build_dir (Path): Path to the build directory.
        install_prefix (Path): Directory where the project will be installed.
        options (List[str]): Additional options for the build system.
        env_vars (Dict[str, str]): Environment variables for the build process.
        verbose (bool): Flag to enable verbose output during execution.
        parallel (int): Number of parallel jobs to use for building.
        run_command (Callable): The asynchronous command runner.
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
        command_runner: Optional[Callable[[List[str]], Awaitable[Any]]] = None,
    ) -> None:
        self.source_dir = Path(source_dir)
        self.build_dir = Path(build_dir)
        self.install_prefix = Path(install_prefix) if install_prefix else self.build_dir / "install"

        self.options = options or []
        self.env_vars = env_vars or {}
        self.verbose = verbose
        self.parallel = parallel

        self.status = BuildStatus.NOT_STARTED
        self.last_result: Optional[BuildResult] = None

        self.cache_file = self.build_dir / ".build_cache.json"
        self._cache: Dict[str, Any] = {}
        self._load_cache()

        self.build_dir.mkdir(parents=True, exist_ok=True)

        # Use a provided command runner or default to internal async runner
        self.run_command = command_runner or self._default_run_command_async

        logger.debug(
            f"Initialized {self.__class__.__name__} with source={self.source_dir}, build={self.build_dir}"
        )

    def _load_cache(self) -> None:
        if self.cache_file.exists():
            try:
                self._cache = json.loads(self.cache_file.read_text())
                logger.debug(f"Loaded build cache from {self.cache_file}")
            except (json.JSONDecodeError, IOError) as e:
                logger.warning(f"Failed to load build cache: {e}")
                self._cache = {}

    def _save_cache(self) -> None:
        try:
            self.cache_file.parent.mkdir(parents=True, exist_ok=True)
            self.cache_file.write_text(json.dumps(self._cache))
            logger.debug(f"Saved build cache to {self.cache_file}")
        except IOError as e:
            logger.warning(f"Failed to save build cache: {e}")

    async def _default_run_command_async(self, cmd: List[str]) -> BuildResult:
        cmd_str = " ".join(cmd)
        logger.info(f"Running async: {cmd_str}")

        env = os.environ.copy()
        env.update(self.env_vars)

        start_time = time.time()

        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                env=env
            )

            stdout, stderr = await process.communicate()
            exit_code = process.returncode if process.returncode is not None else 1  # Fixed: Ensure exit_code is never None
            end_time = time.time()

            success = exit_code == 0

            build_result = BuildResult(
                success=success,
                output=stdout.decode().strip(),
                error=stderr.decode().strip(),
                exit_code=exit_code,  # Now guaranteed to be int
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

        except FileNotFoundError:
            error_msg = f"Command not found: {cmd[0]}. Please ensure it is installed and in your PATH."
            logger.error(error_msg)
            self.status = BuildStatus.FAILED
            return BuildResult(
                success=False, output="", error=error_msg, exit_code=1, execution_time=time.time() - start_time
            )
        except Exception as e:
            error_msg = f"An unexpected error occurred while running '{cmd_str}': {e}"
            logger.exception(error_msg)
            self.status = BuildStatus.FAILED
            return BuildResult(
                success=False, output="", error=error_msg, exit_code=1, execution_time=time.time() - start_time
            )

    async def clean(self) -> BuildResult:
        self.status = BuildStatus.CLEANING
        logger.info(f"Cleaning build directory: {self.build_dir}")

        start_time = time.time()
        success = True
        error_message = ""

        try:
            if self.build_dir.exists():
                # Preserve the cache file if it exists
                cache_content = None
                if self.cache_file.exists():
                    cache_content = self.cache_file.read_bytes()

                # Remove all contents except the cache file itself
                for item in self.build_dir.iterdir():
                    if item != self.cache_file:
                        try:
                            if item.is_dir():
                                shutil.rmtree(item)
                            else:
                                item.unlink()
                        except Exception as e:
                            success = False
                            error_message += f"Error removing {item}: {e}\n"

                # Restore cache file if it was backed up
                if cache_content is not None:
                    self.cache_file.write_bytes(cache_content)
            else:
                self.build_dir.mkdir(parents=True, exist_ok=True)

        except Exception as e:
            success = False
            error_message = str(e)
            logger.error(f"Error during cleaning: {error_message}")

        end_time = time.time()

        build_result = BuildResult(
            success=success,
            output=f"Cleaned build directory: {self.build_dir}" if success else "",
            error=error_message,
            exit_code=0 if success else 1,
            execution_time=end_time - start_time
        )

        if success:
            logger.success(f"Successfully cleaned build directory: {self.build_dir}")
            self.status = BuildStatus.COMPLETED
        else:
            logger.error(f"Failed to clean build directory: {self.build_dir}")
            self.status = BuildStatus.FAILED

        self.last_result = build_result
        return build_result

    def get_status(self) -> BuildStatus:
        return self.status

    def get_last_result(self) -> Optional[BuildResult]:
        return self.last_result

    @classmethod
    def from_options(cls, options: BuildOptions) -> 'BuildHelperBase':
        return cls(
            source_dir=options.get('source_dir', Path('.')),
            build_dir=options.get('build_dir', Path('build')),
            install_prefix=options.get('install_prefix'),
            options=options.get('options', []),
            env_vars=options.get('env_vars', {}),
            verbose=options.get('verbose', False),
            parallel=options.get('parallel', os.cpu_count() or 4)
        )

    @abstractmethod
    async def configure(self) -> BuildResult:
        pass

    @abstractmethod
    async def build(self, target: str = "") -> BuildResult:
        pass

    @abstractmethod
    async def install(self) -> BuildResult:
        pass

    @abstractmethod
    async def test(self) -> BuildResult:
        pass

    @abstractmethod
    async def generate_docs(self, doc_target: str = "doc") -> BuildResult:
        pass
