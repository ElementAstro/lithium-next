#!/usr/bin/env python3
"""
Enhanced utility functions for the compiler helper module.

This module provides comprehensive utilities for file operations, configuration
management, and system interactions with modern Python features.
"""

from __future__ import annotations

import asyncio
import json
import os
import platform
import shutil
import subprocess
import tempfile
import time
from contextlib import asynccontextmanager, contextmanager
from pathlib import Path
from typing import (
    Any, AsyncContextManager, ContextManager, Dict, Generator,
    # Keep Union for PathLike if not imported from core_types directly
    List, Optional, AsyncGenerator, Union
)

import aiofiles
from loguru import logger
from pydantic import BaseModel, ValidationError

from .core_types import (
    CommandResult, CompilerException, PathLike
)


class FileOperationError(CompilerException):
    """Exception raised for file operation errors."""
    pass


class ConfigurationManager:
    """Enhanced configuration management with validation and async support."""

    def __init__(self, config_dir: Optional[PathLike] = None) -> None:
        self.config_dir = Path(
            config_dir) if config_dir else Path.home() / ".compiler_helper"
        self.config_dir.mkdir(parents=True, exist_ok=True)

    async def load_json_async(self, file_path: PathLike) -> Dict[str, Any]:
        """
        Asynchronously load and parse a JSON file with error handling.

        Args:
            file_path: Path to the JSON file

        Returns:
            Parsed JSON data as dictionary

        Raises:
            FileOperationError: If file cannot be read or parsed
        """
        path = Path(file_path)

        if not path.exists():
            raise FileOperationError(
                f"JSON file not found: {path}",
                error_code="FILE_NOT_FOUND",
                file_path=str(path)
            )

        try:
            async with aiofiles.open(path, 'r', encoding='utf-8') as f:
                content = await f.read()
                return json.loads(content)
        except json.JSONDecodeError as e:
            raise FileOperationError(
                f"Invalid JSON in file {path}: {e}",
                error_code="INVALID_JSON",
                file_path=str(path),
                json_error=str(e)
            ) from e
        except OSError as e:
            raise FileOperationError(
                f"Failed to read file {path}: {e}",
                error_code="FILE_READ_ERROR",
                file_path=str(path),
                os_error=str(e)
            ) from e

    def load_json(self, file_path: PathLike) -> Dict[str, Any]:
        """
        Synchronously load and parse a JSON file with error handling.

        Args:
            file_path: Path to the JSON file

        Returns:
            Parsed JSON data as dictionary
        """
        path = Path(file_path)

        if not path.exists():
            raise FileOperationError(
                f"JSON file not found: {path}",
                error_code="FILE_NOT_FOUND",
                file_path=str(path)
            )

        try:
            with path.open('r', encoding='utf-8') as f:
                return json.load(f)
        except json.JSONDecodeError as e:
            raise FileOperationError(
                f"Invalid JSON in file {path}: {e}",
                error_code="INVALID_JSON",
                file_path=str(path),
                json_error=str(e)
            ) from e
        except OSError as e:
            raise FileOperationError(
                f"Failed to read file {path}: {e}",
                error_code="FILE_READ_ERROR",
                file_path=str(path),
                os_error=str(e)
            ) from e

    async def save_json_async(
        self,
        file_path: PathLike,
        data: Dict[str, Any],
        indent: int = 2,
        backup: bool = True
    ) -> None:
        """
        Asynchronously save data to a JSON file with backup support.

        Args:
            file_path: Path to save the JSON file
            data: Data to save
            indent: JSON indentation level
            backup: Whether to create a backup of existing file
        """
        path = Path(file_path)
        path.parent.mkdir(parents=True, exist_ok=True)

        # Create backup if file exists and backup is enabled
        if backup and path.exists():
            backup_path = path.with_suffix(f"{path.suffix}.backup")
            shutil.copy2(path, backup_path)
            logger.debug(f"Created backup: {backup_path}")

        try:
            content = json.dumps(data, indent=indent, ensure_ascii=False)
            async with aiofiles.open(path, 'w', encoding='utf-8') as f:
                await f.write(content)

            logger.debug(f"JSON data saved to {path}")

        except (OSError, TypeError) as e:
            raise FileOperationError(
                f"Failed to save JSON to {path}: {e}",
                error_code="FILE_WRITE_ERROR",
                file_path=str(path),
                error=str(e)
            ) from e

    def save_json(
        self,
        file_path: PathLike,
        data: Dict[str, Any],
        indent: int = 2,
        backup: bool = True
    ) -> None:
        """
        Synchronously save data to a JSON file with backup support.

        Args:
            file_path: Path to save the JSON file
            data: Data to save
            indent: JSON indentation level
            backup: Whether to create a backup of existing file
        """
        path = Path(file_path)
        path.parent.mkdir(parents=True, exist_ok=True)

        # Create backup if file exists and backup is enabled
        if backup and path.exists():
            backup_path = path.with_suffix(f"{path.suffix}.backup")
            shutil.copy2(path, backup_path)
            logger.debug(f"Created backup: {backup_path}")

        try:
            with path.open('w', encoding='utf-8') as f:
                json.dump(data, f, indent=indent, ensure_ascii=False)

            logger.debug(f"JSON data saved to {path}")

        except (OSError, TypeError) as e:
            raise FileOperationError(
                f"Failed to save JSON to {path}: {e}",
                error_code="FILE_WRITE_ERROR",
                file_path=str(path),
                error=str(e)
            ) from e

    def load_config_with_model(
        self,
        file_path: PathLike,
        model_class: type[BaseModel]
    ) -> BaseModel:
        """
        Load and validate configuration using a Pydantic model.

        Args:
            file_path: Path to the configuration file
            model_class: Pydantic model class for validation

        Returns:
            Validated configuration object

        Raises:
            FileOperationError: If file cannot be loaded
            ValidationError: If configuration is invalid
        """
        data = self.load_json(file_path)

        try:
            return model_class.model_validate(data)
        except ValidationError as e:
            raise FileOperationError(
                f"Invalid configuration in {file_path}: {e}",
                error_code="INVALID_CONFIGURATION",
                file_path=str(file_path),
                validation_errors=e.errors()
            ) from e


class SystemInfo:
    """Enhanced system information gathering utilities."""

    @staticmethod
    def get_platform_info() -> Dict[str, str]:
        """Get comprehensive platform information."""
        return {
            "system": platform.system(),
            "machine": platform.machine(),
            "architecture": platform.architecture()[0],
            "processor": platform.processor(),
            "python_version": platform.python_version(),
            "platform": platform.platform(),
            "release": platform.release(),
            "version": platform.version()
        }

    @staticmethod
    def get_cpu_count() -> int:
        """Get the number of available CPU cores."""
        return os.cpu_count() or 1

    @staticmethod
    def get_memory_info() -> Dict[str, Union[int, float]]:
        """Get basic memory information (if available)."""
        try:
            import psutil
            memory = psutil.virtual_memory()
            return {
                "total": memory.total,
                "available": memory.available,
                "percent_used": memory.percent
            }
        except ImportError:
            logger.debug("psutil not available, memory info unavailable")
            return {}

    @staticmethod
    def find_executable(name: str, paths: Optional[List[str]] = None) -> Optional[Path]:
        """
        Find an executable in the system PATH or specified paths.

        Args:
            name: Executable name to find
            paths: Additional paths to search

        Returns:
            Path to executable if found, None otherwise
        """
        # Try system PATH first
        result = shutil.which(name)
        if result:
            return Path(result)

        # Try additional paths
        if paths:
            for path_str in paths:
                path = Path(path_str)
                if path.is_dir():
                    exe_path = path / name
                    if exe_path.is_file() and os.access(exe_path, os.X_OK):
                        return exe_path

        return None

    @staticmethod
    def get_environment_info() -> Dict[str, str]:
        """Get relevant environment variables."""
        relevant_vars = [
            'PATH', 'CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS',
            'PKG_CONFIG_PATH', 'CMAKE_PREFIX_PATH', 'MSVC_VERSION'
        ]

        return {
            var: os.environ.get(var, "")
            for var in relevant_vars
            if var in os.environ
        }


class FileManager:
    """Enhanced file management utilities with async support."""

    @staticmethod
    @contextmanager
    def temporary_directory() -> Generator[Path, Any, Any]:
        """Context manager for temporary directory that's automatically cleaned up."""
        temp_dir = None
        try:
            temp_dir = Path(tempfile.mkdtemp(prefix="compiler_helper_"))
            logger.debug(f"Created temporary directory: {temp_dir}")
            yield temp_dir
        finally:
            if temp_dir and temp_dir.exists():
                shutil.rmtree(temp_dir, ignore_errors=True)
                logger.debug(f"Cleaned up temporary directory: {temp_dir}")

    @staticmethod
    @asynccontextmanager
    async def temporary_directory_async() -> AsyncGenerator[Path, Any]:
        """Async context manager for temporary directory."""
        temp_dir = None
        try:
            temp_dir = Path(tempfile.mkdtemp(prefix="compiler_helper_"))
            logger.debug(f"Created temporary directory: {temp_dir}")
            yield temp_dir
        finally:
            if temp_dir and temp_dir.exists():
                await asyncio.get_event_loop().run_in_executor(
                    None, shutil.rmtree, temp_dir, True
                )
                logger.debug(f"Cleaned up temporary directory: {temp_dir}")

    @staticmethod
    def ensure_directory(path: PathLike, mode: int = 0o755) -> Path:
        """
        Ensure a directory exists, creating it if necessary.

        Args:
            path: Directory path to ensure
            mode: Directory permissions

        Returns:
            Path object for the directory
        """
        dir_path = Path(path)
        dir_path.mkdir(parents=True, exist_ok=True, mode=mode)
        return dir_path

    @staticmethod
    def safe_copy(
        src: PathLike,
        dst: PathLike,
        preserve_metadata: bool = True
    ) -> None:
        """
        Safely copy a file with error handling.

        Args:
            src: Source file path
            dst: Destination file path
            preserve_metadata: Whether to preserve file metadata
        """
        src_path = Path(src)
        dst_path = Path(dst)

        if not src_path.exists():
            raise FileOperationError(
                f"Source file does not exist: {src_path}",
                error_code="SOURCE_NOT_FOUND",
                source=str(src_path)
            )

        try:
            dst_path.parent.mkdir(parents=True, exist_ok=True)

            if preserve_metadata:
                shutil.copy2(src_path, dst_path)
            else:
                shutil.copy(src_path, dst_path)

            logger.debug(f"Copied {src_path} -> {dst_path}")

        except OSError as e:
            raise FileOperationError(
                f"Failed to copy {src_path} to {dst_path}: {e}",
                error_code="COPY_FAILED",
                source=str(src_path),
                destination=str(dst_path),
                os_error=str(e)
            ) from e

    @staticmethod
    def get_file_info(path: PathLike) -> Dict[str, Any]:
        """
        Get comprehensive file information.

        Args:
            path: File path to analyze

        Returns:
            Dictionary with file information
        """
        file_path = Path(path)

        if not file_path.exists():
            return {"exists": False}

        stat = file_path.stat()

        return {
            "exists": True,
            "is_file": file_path.is_file(),
            "is_dir": file_path.is_dir(),
            "is_symlink": file_path.is_symlink(),
            "size": stat.st_size,
            "modified_time": stat.st_mtime,
            "created_time": getattr(stat, 'st_birthtime', stat.st_ctime),
            "permissions": oct(stat.st_mode)[-3:],
            "is_executable": os.access(file_path, os.X_OK)
        }


class ProcessManager:
    """Enhanced process execution utilities with async support."""

    @staticmethod
    async def run_command_async(
        command: List[str],
        timeout: Optional[float] = None,
        cwd: Optional[PathLike] = None,
        env: Optional[Dict[str, str]] = None,
        input_data: Optional[bytes] = None
    ) -> CommandResult:
        """
        Run a command asynchronously with enhanced error handling.

        Args:
            command: Command and arguments to execute
            timeout: Command timeout in seconds
            cwd: Working directory for the command
            env: Environment variables
            input_data: Data to send to stdin

        Returns:
            CommandResult with execution details
        """
        start_time = time.time()

        logger.debug(
            f"Executing command: {' '.join(command)}",
            extra={
                "command": command,
                "timeout": timeout,
                "cwd": str(cwd) if cwd else None
            }
        )

        try:
            # Merge environment variables
            final_env = os.environ.copy()
            if env:
                final_env.update(env)

            # Create subprocess
            process = await asyncio.create_subprocess_exec(
                *command,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                stdin=asyncio.subprocess.PIPE if input_data else None,
                cwd=cwd,
                env=final_env
            )

            # Execute with timeout
            try:
                stdout, stderr = await asyncio.wait_for(
                    process.communicate(input=input_data),
                    timeout=timeout
                )
            except asyncio.TimeoutError:
                process.kill()
                await process.wait()

                execution_time = time.time() - start_time
                return CommandResult(
                    success=False,
                    stderr=f"Command timed out after {timeout}s",
                    return_code=-1,
                    command=command,
                    execution_time=execution_time
                )

            execution_time = time.time() - start_time
            success = process.returncode == 0

            result = CommandResult(
                success=success,
                stdout=stdout.decode('utf-8', errors='replace').strip(),
                stderr=stderr.decode('utf-8', errors='replace').strip(),
                return_code=process.returncode or 0,
                command=command,
                execution_time=execution_time
            )

            if success:
                logger.debug(
                    f"Command completed successfully in {execution_time:.2f}s")
            else:
                logger.error(
                    f"Command failed with code {result.return_code} in {execution_time:.2f}s",
                    extra={
                        "command": ' '.join(command),
                        "stderr": result.stderr
                    }
                )

            return result

        except FileNotFoundError:
            return CommandResult(
                success=False,
                stderr=f"Command not found: {command[0]}",
                return_code=-1,
                command=command,
                execution_time=time.time() - start_time
            )
        except Exception as e:
            return CommandResult(
                success=False,
                stderr=f"Unexpected error: {e}",
                return_code=-1,
                command=command,
                execution_time=time.time() - start_time
            )

    @staticmethod
    def run_command(
        command: List[str],
        timeout: Optional[float] = None,
        cwd: Optional[PathLike] = None,
        env: Optional[Dict[str, str]] = None,
        input_data: Optional[bytes] = None
    ) -> CommandResult:
        """
        Run a command synchronously.

        Args:
            command: Command and arguments to execute
            timeout: Command timeout in seconds
            cwd: Working directory for the command
            env: Environment variables
            input_data: Data to send to stdin

        Returns:
            CommandResult with execution details
        """
        start_time = time.time()

        logger.debug(f"Executing command: {' '.join(command)}")

        try:
            # Merge environment variables
            final_env = os.environ.copy()
            if env:
                final_env.update(env)

            # Execute command
            result = subprocess.run(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                input=input_data,
                timeout=timeout,
                cwd=cwd,
                env=final_env,
                text=False  # Keep as bytes for proper encoding handling
            )

            execution_time = time.time() - start_time
            success = result.returncode == 0

            cmd_result = CommandResult(
                success=success,
                stdout=result.stdout.decode('utf-8', errors='replace').strip(),
                stderr=result.stderr.decode('utf-8', errors='replace').strip(),
                return_code=result.returncode,
                command=command,
                execution_time=execution_time
            )

            if success:
                logger.debug(
                    f"Command completed successfully in {execution_time:.2f}s")
            else:
                logger.error(
                    f"Command failed with code {cmd_result.return_code} in {execution_time:.2f}s",
                    extra={
                        "command": ' '.join(command),
                        "stderr": cmd_result.stderr
                    }
                )

            return cmd_result

        except subprocess.TimeoutExpired:
            return CommandResult(
                success=False,
                stderr=f"Command timed out after {timeout}s",
                return_code=-1,
                command=command,
                execution_time=time.time() - start_time
            )
        except FileNotFoundError:
            return CommandResult(
                success=False,
                stderr=f"Command not found: {command[0]}",
                return_code=-1,
                command=command,
                execution_time=time.time() - start_time
            )
        except Exception as e:
            return CommandResult(
                success=False,
                stderr=f"Unexpected error: {e}",
                return_code=-1,
                command=command,
                execution_time=time.time() - start_time
            )


# Convenience functions for backward compatibility and ease of use
def load_json(file_path: PathLike) -> Dict[str, Any]:
    """Load JSON file using the default configuration manager."""
    config_manager = ConfigurationManager()
    return config_manager.load_json(file_path)


def save_json(
    file_path: PathLike,
    data: Dict[str, Any],
    indent: int = 2
) -> None:
    """Save JSON file using the default configuration manager."""
    config_manager = ConfigurationManager()
    config_manager.save_json(file_path, data, indent)


# Create default instances for convenience
default_config_manager = ConfigurationManager()
default_file_manager = FileManager()
default_process_manager = ProcessManager()
default_system_info = SystemInfo()
