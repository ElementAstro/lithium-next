#!/usr/bin/env python3
"""
Enhanced command execution utilities with modern Python features.

This module provides robust, async-first command execution with comprehensive
error handling, timeout management, and structured logging integration.
"""

from __future__ import annotations

import asyncio
import shutil
import subprocess
import time
from contextlib import asynccontextmanager
from pathlib import Path
from typing import (
    Any,
    AsyncContextManager,
    AsyncGenerator,
    AsyncIterator,
    Callable,
    Dict,
    List,
    Optional,
    Sequence,
    Union,
)

from loguru import logger

from .models import CommandResult, HotspotException


class CommandExecutionError(HotspotException):
    """Raised when command execution fails with specific error context."""

    pass


class CommandTimeoutError(CommandExecutionError):
    """Raised when command execution times out."""

    pass


class CommandNotFoundError(CommandExecutionError):
    """Raised when the command executable is not found."""

    pass


class CommandValidator:
    """Validates commands before execution for security and correctness."""

    ALLOWED_COMMANDS = {
        "nmcli",
        "iw",
        "arp",
        "ip",
        "ifconfig",
        "systemctl",
        "hostapd",
        "dnsmasq",
        "iptables",
        "ufw",
        "firewall-cmd",
    }

    DANGEROUS_PATTERNS = {
        ";",
        "&&",
        "||",
        "|",
        ">",
        ">>",
        "<",
        "$(",
        "`",
        "rm -rf",
        "dd ",
        "mkfs",
        "fdisk",
        "parted",
    }

    @classmethod
    def validate_command(cls, cmd: Sequence[str]) -> None:
        """Validate command for security and correctness."""
        if not cmd:
            raise CommandExecutionError(
                "Empty command provided", error_code="EMPTY_COMMAND"
            )

        command_name = Path(cmd[0]).name

        # Check if command is in allowed list
        if command_name not in cls.ALLOWED_COMMANDS:
            logger.warning(
                f"Command '{command_name}' not in allowed list",
                extra={"command": command_name, "allowed": list(cls.ALLOWED_COMMANDS)},
            )

        # Check for dangerous patterns
        cmd_str = " ".join(cmd)
        for pattern in cls.DANGEROUS_PATTERNS:
            if pattern in cmd_str:
                raise CommandExecutionError(
                    f"Dangerous pattern '{pattern}' detected in command",
                    error_code="DANGEROUS_COMMAND",
                    pattern=pattern,
                    command=cmd_str,
                )

        # Validate command exists
        if not shutil.which(cmd[0]):
            raise CommandNotFoundError(
                f"Command '{cmd[0]}' not found in PATH",
                error_code="COMMAND_NOT_FOUND",
                command=cmd[0],
            )


class EnhancedCommandRunner:
    """Enhanced command runner with advanced features and monitoring."""

    def __init__(
        self,
        default_timeout: float = 30.0,
        max_output_size: int = 1024 * 1024,  # 1MB
        validate_commands: bool = True,
    ) -> None:
        self.default_timeout = default_timeout
        self.max_output_size = max_output_size
        self.validate_commands = validate_commands
        self._execution_stats: Dict[str, Any] = {
            "total_executions": 0,
            "successful_executions": 0,
            "failed_executions": 0,
            "average_execution_time": 0.0,
        }

    @property
    def execution_stats(self) -> Dict[str, Any]:
        """Get execution statistics."""
        return self._execution_stats.copy()

    def _update_stats(self, execution_time: float, success: bool) -> None:
        """Update execution statistics."""
        self._execution_stats["total_executions"] += 1

        if success:
            self._execution_stats["successful_executions"] += 1
        else:
            self._execution_stats["failed_executions"] += 1

        # Update average execution time
        total = self._execution_stats["total_executions"]
        current_avg = self._execution_stats["average_execution_time"]
        self._execution_stats["average_execution_time"] = (
            current_avg * (total - 1) + execution_time
        ) / total

    async def run_with_timeout(
        self,
        cmd: Sequence[str],
        timeout: Optional[float] = None,
        input_data: Optional[bytes] = None,
        env: Optional[Dict[str, str]] = None,
        cwd: Optional[Union[str, Path]] = None,
    ) -> CommandResult:
        """
        Execute command with comprehensive timeout and resource management.

        Args:
            cmd: Command and arguments to execute
            timeout: Maximum execution time in seconds
            input_data: Data to send to stdin
            env: Environment variables for the process
            cwd: Working directory for the process

        Returns:
            CommandResult with execution details

        Raises:
            CommandTimeoutError: If command times out
            CommandNotFoundError: If command is not found
            CommandExecutionError: For other execution errors
        """
        timeout = timeout or self.default_timeout
        start_time = time.time()

        # Validate command if enabled
        if self.validate_commands:
            CommandValidator.validate_command(cmd)

        cmd_str = " ".join(str(arg) for arg in cmd)
        logger.debug(
            "Executing command with timeout",
            extra={
                "command": cmd_str,
                "timeout": timeout,
                "cwd": str(cwd) if cwd else None,
                "has_input": input_data is not None,
            },
        )

        try:
            # Create subprocess with enhanced configuration
            proc = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                stdin=asyncio.subprocess.PIPE if input_data else None,
                env=env,
                cwd=cwd,
                limit=self.max_output_size,
            )

            # Execute with timeout
            try:
                stdout, stderr = await asyncio.wait_for(
                    proc.communicate(input=input_data), timeout=timeout
                )
            except asyncio.TimeoutError:
                # Kill the process if it times out
                try:
                    proc.kill()
                    await proc.wait()
                except ProcessLookupError:
                    pass  # Process already terminated

                execution_time = time.time() - start_time
                self._update_stats(execution_time, False)

                raise CommandTimeoutError(
                    f"Command timed out after {timeout}s: {cmd_str}",
                    error_code="COMMAND_TIMEOUT",
                    command=cmd_str,
                    timeout=timeout,
                    execution_time=execution_time,
                )

            execution_time = time.time() - start_time
            success = proc.returncode == 0

            result = CommandResult(
                success=success,
                stdout=stdout.decode("utf-8", errors="replace").strip(),
                stderr=stderr.decode("utf-8", errors="replace").strip(),
                return_code=proc.returncode or 0,
                command=list(cmd),
                execution_time=execution_time,
            )

            self._update_stats(execution_time, success)

            # Enhanced logging with context
            if success:
                logger.debug(
                    "Command executed successfully",
                    extra={
                        "command": cmd_str,
                        "execution_time": execution_time,
                        "return_code": result.return_code,
                    },
                )
            else:
                logger.error(
                    "Command execution failed",
                    extra={
                        "command": cmd_str,
                        "return_code": result.return_code,
                        "stderr": result.stderr,
                        "execution_time": execution_time,
                    },
                )

            return result

        except FileNotFoundError:
            execution_time = time.time() - start_time
            self._update_stats(execution_time, False)

            raise CommandNotFoundError(
                f"Command not found: {cmd[0]}",
                error_code="COMMAND_NOT_FOUND",
                command=cmd[0],
            )
        except Exception as e:
            execution_time = time.time() - start_time
            self._update_stats(execution_time, False)

            if isinstance(e, (CommandExecutionError, CommandTimeoutError)):
                raise

            raise CommandExecutionError(
                f"Unexpected error executing command: {e}",
                error_code="COMMAND_EXECUTION_ERROR",
                command=cmd_str,
                original_error=str(e),
            ) from e

    @asynccontextmanager
    async def managed_process(
        self, cmd: Sequence[str], **kwargs: Any
    ) -> AsyncGenerator[asyncio.subprocess.Process, None]:
        """Context manager for process lifecycle management."""
        proc = None
        try:
            proc = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                **kwargs,
            )
            yield proc
        finally:
            if proc and proc.returncode is None:
                try:
                    proc.terminate()
                    await asyncio.wait_for(proc.wait(), timeout=5.0)
                except (ProcessLookupError, asyncio.TimeoutError):
                    try:
                        proc.kill()
                        await proc.wait()
                    except ProcessLookupError:
                        pass


# Global command runner instance
_default_runner = EnhancedCommandRunner()


async def run_command_async(
    cmd: Sequence[str],
    timeout: Optional[float] = None,
    input_data: Optional[bytes] = None,
    env: Optional[Dict[str, str]] = None,
    cwd: Optional[Union[str, Path]] = None,
    runner: Optional[EnhancedCommandRunner] = None,
) -> CommandResult:
    """
    Execute a command asynchronously with enhanced error handling and timeout management.

    Args:
        cmd: Command and arguments to execute
        timeout: Maximum execution time in seconds (default: 30.0)
        input_data: Data to send to stdin
        env: Environment variables for the process
        cwd: Working directory for the process
        runner: Custom command runner instance

    Returns:
        CommandResult with detailed execution information

    Example:
        >>> result = await run_command_async(["nmcli", "--version"])
        >>> if result.success:
        ...     print(f"Output: {result.stdout}")
    """
    runner = runner or _default_runner
    return await runner.run_with_timeout(
        cmd=cmd, timeout=timeout, input_data=input_data, env=env, cwd=cwd
    )


def run_command(
    cmd: Sequence[str],
    timeout: Optional[float] = None,
    input_data: Optional[bytes] = None,
    env: Optional[Dict[str, str]] = None,
    cwd: Optional[Union[str, Path]] = None,
) -> CommandResult:
    """
    Synchronous wrapper around run_command_async for backward compatibility.

    Args:
        cmd: Command and arguments to execute
        timeout: Maximum execution time in seconds
        input_data: Data to send to stdin
        env: Environment variables for the process
        cwd: Working directory for the process

    Returns:
        CommandResult with execution details

    Note:
        This function creates a new event loop if none exists. For async contexts,
        prefer using run_command_async directly.
    """
    try:
        loop = asyncio.get_running_loop()
        # If we're in an async context, we can't use asyncio.run
        raise RuntimeError(
            "run_command() cannot be called from an async context. "
            "Use run_command_async() instead."
        )
    except RuntimeError:
        # No running loop, safe to create one
        return asyncio.run(
            run_command_async(
                cmd=cmd, timeout=timeout, input_data=input_data, env=env, cwd=cwd
            )
        )


async def run_command_with_retry(
    cmd: Sequence[str],
    max_retries: int = 3,
    retry_delay: float = 1.0,
    exponential_backoff: bool = True,
    **kwargs: Any,
) -> CommandResult:
    """
    Execute command with retry logic and exponential backoff.

    Args:
        cmd: Command to execute
        max_retries: Maximum number of retry attempts
        retry_delay: Initial delay between retries in seconds
        exponential_backoff: Whether to use exponential backoff
        **kwargs: Additional arguments passed to run_command_async

    Returns:
        CommandResult from the successful execution or last failed attempt
    """
    last_result = None
    current_delay = retry_delay

    for attempt in range(max_retries + 1):
        try:
            result = await run_command_async(cmd, **kwargs)

            if result.success:
                if attempt > 0:
                    logger.info(
                        f"Command succeeded on attempt {attempt + 1}/{max_retries + 1}",
                        extra={"command": " ".join(str(arg) for arg in cmd)},
                    )
                return result

            last_result = result

            if attempt < max_retries:
                logger.warning(
                    f"Command failed (attempt {attempt + 1}/{max_retries + 1}), "
                    f"retrying in {current_delay}s",
                    extra={
                        "command": " ".join(str(arg) for arg in cmd),
                        "return_code": result.return_code,
                        "stderr": result.stderr,
                    },
                )
                await asyncio.sleep(current_delay)

                if exponential_backoff:
                    current_delay *= 2

        except CommandExecutionError as e:
            last_result = CommandResult(success=False, stderr=str(e), command=list(cmd))

            if attempt < max_retries:
                logger.warning(
                    f"Command error (attempt {attempt + 1}/{max_retries + 1}), "
                    f"retrying in {current_delay}s: {e}",
                    extra={"command": " ".join(str(arg) for arg in cmd)},
                )
                await asyncio.sleep(current_delay)

                if exponential_backoff:
                    current_delay *= 2
            else:
                raise

    logger.error(
        f"Command failed after {max_retries + 1} attempts",
        extra={"command": " ".join(str(arg) for arg in cmd)},
    )

    return last_result or CommandResult(
        success=False, stderr="All retry attempts failed", command=list(cmd)
    )


async def stream_command_output(
    cmd: Sequence[str], callback: Callable[[str], None], **kwargs: Any
) -> AsyncIterator[str]:
    """
    Stream command output line by line with real-time processing.

    Args:
        cmd: Command to execute
        callback: Function to call for each output line
        **kwargs: Additional subprocess arguments

    Yields:
        Output lines as they become available
    """
    logger.debug(f"Streaming output for command: {' '.join(str(arg) for arg in cmd)}")

    async with _default_runner.managed_process(cmd, **kwargs) as proc:
        assert proc.stdout is not None

        async for line in proc.stdout:
            line_str = line.decode("utf-8", errors="replace").rstrip("\n\r")
            callback(line_str)
            yield line_str

        await proc.wait()


def get_command_runner_stats() -> Dict[str, Any]:
    """Get execution statistics from the default command runner."""
    return _default_runner.execution_stats
