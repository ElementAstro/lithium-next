#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Base class for build helpers providing shared asynchronous functionality with modern Python features.
"""

from __future__ import annotations

import os
import json
import shutil
import asyncio
import time
import resource
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Dict, List, Any, Optional, Union, Callable, Awaitable, AsyncContextManager
from contextlib import asynccontextmanager

from loguru import logger

from .models import BuildStatus, BuildResult, BuildOptions, BuildSession
from .errors import BuildError, ErrorContext, handle_build_error


class BuildHelperBase(ABC):
    """
    Abstract base class for build helpers providing shared asynchronous functionality.

    This class defines the common interface and behavior for all build system
    implementations, leveraging asyncio for non-blocking operations and modern
    Python features for enhanced performance and maintainability.

    Attributes:
        source_dir: Path to the source directory.
        build_dir: Path to the build directory.
        install_prefix: Directory where the project will be installed.
        options: Additional options for the build system.
        env_vars: Environment variables for the build process.
        verbose: Flag to enable verbose output during execution.
        parallel: Number of parallel jobs to use for building.
        run_command: The asynchronous command runner.
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
        command_runner: Optional[Callable[[List[str]], Awaitable[BuildResult]]] = None,
    ) -> None:
        self.source_dir = Path(source_dir).resolve()
        self.build_dir = Path(build_dir).resolve()
        self.install_prefix = (
            Path(install_prefix).resolve() 
            if install_prefix 
            else self.build_dir / "install"
        )

        self.options = options or []
        self.env_vars = env_vars or {}
        self.verbose = verbose
        self.parallel = max(1, parallel)  # Ensure at least 1 parallel job

        self.status = BuildStatus.NOT_STARTED
        self.last_result: Optional[BuildResult] = None

        self.cache_file = self.build_dir / ".build_cache.json"
        self._cache: Dict[str, Any] = {}
        self._load_cache()

        # Ensure build directory exists
        self.build_dir.mkdir(parents=True, exist_ok=True)

        # Use a provided command runner or default to internal async runner
        self.run_command = command_runner or self._default_run_command_async

        logger.debug(
            f"Initialized {self.__class__.__name__}",
            extra={
                "source_dir": str(self.source_dir),
                "build_dir": str(self.build_dir),
                "install_prefix": str(self.install_prefix),
                "parallel": self.parallel,
                "verbose": self.verbose
            }
        )

    def _load_cache(self) -> None:
        """Load build cache from disk with error handling."""
        if not self.cache_file.exists():
            self._cache = {}
            return

        try:
            cache_data = self.cache_file.read_text(encoding='utf-8')
            self._cache = json.loads(cache_data)
            logger.debug(f"Loaded build cache from {self.cache_file}")
        except (json.JSONDecodeError, OSError, UnicodeDecodeError) as e:
            logger.warning(f"Failed to load build cache: {e}")
            self._cache = {}

    def _save_cache(self) -> None:
        """Save build cache to disk with error handling."""
        try:
            self.cache_file.parent.mkdir(parents=True, exist_ok=True)
            cache_data = json.dumps(self._cache, indent=2, ensure_ascii=False)
            self.cache_file.write_text(cache_data, encoding='utf-8')
            logger.debug(f"Saved build cache to {self.cache_file}")
        except (OSError, UnicodeEncodeError) as e:
            logger.warning(f"Failed to save build cache: {e}")

    def _get_resource_usage(self) -> Dict[str, float]:
        """Get current resource usage metrics."""
        try:
            usage = resource.getrusage(resource.RUSAGE_SELF)
            return {
                "user_time": usage.ru_utime,
                "system_time": usage.ru_stime,
                "max_memory_kb": usage.ru_maxrss,
                "page_faults": float(usage.ru_majflt + usage.ru_minflt),
            }
        except (OSError, AttributeError):
            return {}

    async def _default_run_command_async(self, cmd: List[str]) -> BuildResult:
        """Default asynchronous command runner with enhanced error handling and metrics."""
        cmd_str = " ".join(cmd)
        logger.info(f"Running async: {cmd_str}")

        # Prepare environment
        env = os.environ.copy()
        env.update(self.env_vars)

        # Track timing and resources
        start_time = time.time()
        start_resources = self._get_resource_usage()

        try:
            # Create subprocess with better error handling
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                env=env,
                cwd=self.build_dir
            )

            # Wait for process completion with timeout
            try:
                stdout, stderr = await asyncio.wait_for(
                    process.communicate(),
                    timeout=3600  # 1 hour timeout
                )
            except asyncio.TimeoutError:
                process.kill()
                await process.wait()
                raise BuildError(
                    f"Command timed out after 1 hour: {cmd_str}",
                    context=ErrorContext(
                        command=cmd_str,
                        working_directory=self.build_dir,
                        environment_vars=self.env_vars
                    )
                )

            exit_code = process.returncode or 0
            end_time = time.time()
            end_resources = self._get_resource_usage()

            # Calculate resource metrics
            memory_usage = None
            cpu_time = None
            if start_resources and end_resources:
                memory_usage = int(end_resources.get("max_memory_kb", 0) * 1024)  # Convert to bytes
                cpu_time = (
                    end_resources.get("user_time", 0) - start_resources.get("user_time", 0) +
                    end_resources.get("system_time", 0) - start_resources.get("system_time", 0)
                )

            success = exit_code == 0
            execution_time = end_time - start_time

            build_result = BuildResult(
                success=success,
                output=stdout.decode('utf-8', errors='replace').strip(),
                error=stderr.decode('utf-8', errors='replace').strip(),
                exit_code=exit_code,
                execution_time=execution_time,
                memory_usage=memory_usage,
                cpu_time=cpu_time
            )

            # Enhanced logging
            if self.verbose:
                if build_result.output:
                    logger.info(f"Command output: {build_result.output}")
                if build_result.error and not success:
                    logger.warning(f"Command stderr: {build_result.error}")

            # Log result with metrics
            build_result.log_result(f"command: {cmd[0]}")

            self.last_result = build_result
            if not success:
                self.status = BuildStatus.FAILED

            return build_result

        except FileNotFoundError as e:
            error_context = ErrorContext(
                command=cmd_str,
                working_directory=self.build_dir,
                environment_vars=self.env_vars,
                execution_time=time.time() - start_time
            )
            raise handle_build_error("_default_run_command_async", e, context=error_context)

        except Exception as e:
            error_context = ErrorContext(
                command=cmd_str,
                working_directory=self.build_dir,
                environment_vars=self.env_vars,
                execution_time=time.time() - start_time
            )
            self.status = BuildStatus.FAILED
            raise handle_build_error("_default_run_command_async", e, context=error_context)

    async def clean(self) -> BuildResult:
        """Clean build directory with improved error handling and preservation of important files."""
        self.status = BuildStatus.CLEANING
        logger.info(f"Cleaning build directory: {self.build_dir}")

        start_time = time.time()
        preserved_files = {self.cache_file}  # Files to preserve during cleaning
        errors: List[str] = []

        try:
            if self.build_dir.exists():
                # Backup important files
                backups: Dict[Path, bytes] = {}
                for file_path in preserved_files:
                    if file_path.exists():
                        try:
                            backups[file_path] = file_path.read_bytes()
                        except OSError as e:
                            logger.warning(f"Failed to backup {file_path}: {e}")

                # Remove all contents
                for item in self.build_dir.iterdir():
                    try:
                        if item.is_dir():
                            shutil.rmtree(item)
                        else:
                            item.unlink()
                    except OSError as e:
                        errors.append(f"Error removing {item}: {e}")
                        logger.warning(f"Failed to remove {item}: {e}")

                # Restore backed up files
                for file_path, content in backups.items():
                    try:
                        file_path.parent.mkdir(parents=True, exist_ok=True)
                        file_path.write_bytes(content)
                    except OSError as e:
                        errors.append(f"Error restoring {file_path}: {e}")
                        logger.warning(f"Failed to restore {file_path}: {e}")
            else:
                self.build_dir.mkdir(parents=True, exist_ok=True)

        except Exception as e:
            errors.append(str(e))
            logger.error(f"Unexpected error during cleaning: {e}")

        end_time = time.time()
        success = len(errors) == 0
        error_message = "\n".join(errors) if errors else ""

        build_result = BuildResult(
            success=success,
            output=f"Cleaned build directory: {self.build_dir}" if success else "",
            error=error_message,
            exit_code=0 if success else 1,
            execution_time=end_time - start_time,
        )

        if success:
            logger.success(f"Successfully cleaned build directory: {self.build_dir}")
            logger.success(f"Successfully cleaned build directory: {self.build_dir}")
            self.status = BuildStatus.COMPLETED
        else:
            logger.error(f"Failed to clean build directory: {self.build_dir}")
            self.status = BuildStatus.FAILED

        self.last_result = build_result
        return build_result

    @asynccontextmanager
    async def build_session(self, session_id: str) -> AsyncContextManager[BuildSession]:
        """Context manager for tracking build sessions."""
        session = BuildSession(session_id=session_id)
        async with session:
            yield session

    def get_status(self) -> BuildStatus:
        """Get current build status."""
        return self.status

    def get_last_result(self) -> Optional[BuildResult]:
        """Get last build result."""
        return self.last_result

    def get_cache_value(self, key: str, default: Any = None) -> Any:
        """Get value from build cache."""
        return self._cache.get(key, default)

    def set_cache_value(self, key: str, value: Any) -> None:
        """Set value in build cache and save."""
        self._cache[key] = value
        self._save_cache()

    @classmethod
    def from_options(cls, options: BuildOptions) -> BuildHelperBase:
        """Create builder instance from BuildOptions."""
        return cls(
            source_dir=options.source_dir,
            build_dir=options.build_dir,
            install_prefix=options.install_prefix,
            options=options.options,
            env_vars=options.env_vars,
            verbose=options.verbose,
            parallel=options.parallel
        )

    # Abstract methods that must be implemented by subclasses
    @abstractmethod
    async def configure(self) -> BuildResult:
        """Configure the build system."""
        pass

    @abstractmethod
    async def build(self, target: str = "") -> BuildResult:
        """Build the project with optional target."""
        pass

    @abstractmethod
    async def install(self) -> BuildResult:
        """Install the project."""
        pass

    @abstractmethod
    async def test(self) -> BuildResult:
        """Run project tests."""
        pass

    @abstractmethod
    async def generate_docs(self, doc_target: str = "doc") -> BuildResult:
        """Generate project documentation."""
        pass

    async def full_build_workflow(
        self,
        *,
        clean_first: bool = False,
        run_tests: bool = True,
        install_after_build: bool = False,
        generate_docs: bool = False,
        target: str = ""
    ) -> List[BuildResult]:
        """
        Execute a complete build workflow with configurable steps.
        
        Args:
            clean_first: Whether to clean before building
            run_tests: Whether to run tests after building
            install_after_build: Whether to install after building
            generate_docs: Whether to generate documentation
            target: Specific build target
            
        Returns:
            List of BuildResult objects for each step
        """
        results: List[BuildResult] = []
        
        try:
            if clean_first:
                results.append(await self.clean())
                
            results.append(await self.configure())
            results.append(await self.build(target))
            
            if run_tests:
                results.append(await self.test())
                
            if generate_docs:
                results.append(await self.generate_docs())
                
            if install_after_build:
                results.append(await self.install())
                
        except BuildError as e:
            logger.error(f"Build workflow failed: {e}")
            raise
            
        return results
