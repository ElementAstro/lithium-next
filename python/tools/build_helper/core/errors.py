#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Exception hierarchy for build system helpers with enhanced error context.
"""

from __future__ import annotations

import traceback
from pathlib import Path
from typing import Any, Dict, Optional, Union
from dataclasses import dataclass, field

from loguru import logger


@dataclass(frozen=True)
class ErrorContext:
    """Context information for build system errors."""

    command: Optional[str] = None
    exit_code: Optional[int] = None
    working_directory: Optional[Path] = None
    environment_vars: Dict[str, str] = field(default_factory=dict)
    stdout: Optional[str] = None
    stderr: Optional[str] = None
    execution_time: Optional[float] = None
    additional_info: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        """Convert context to dictionary for structured logging."""
        return {
            "command": self.command,
            "exit_code": self.exit_code,
            "working_directory": (
                str(self.working_directory) if self.working_directory else None
            ),
            "environment_vars": self.environment_vars,
            "stdout": self.stdout,
            "stderr": self.stderr,
            "execution_time": self.execution_time,
            "additional_info": self.additional_info,
        }


class BuildSystemError(Exception):
    """
    Base exception class for build system errors with enhanced context tracking.

    This exception provides structured error information including command context,
    execution environment, and detailed debugging information.
    """

    def __init__(
        self,
        message: str,
        *,
        context: Optional[ErrorContext] = None,
        cause: Optional[Exception] = None,
        recoverable: bool = False,
    ) -> None:
        super().__init__(message)
        self.context = context or ErrorContext()
        self.cause = cause
        self.recoverable = recoverable
        self.traceback_str = traceback.format_exc() if cause else None

        # Log error with structured context
        logger.error(
            f"BuildSystemError: {message}",
            extra={
                "error_context": self.context.to_dict(),
                "recoverable": self.recoverable,
                "original_cause": str(cause) if cause else None,
            },
        )

    def __str__(self) -> str:
        """Enhanced string representation with context."""
        base_msg = super().__str__()

        if self.context.command:
            base_msg += f"\nCommand: {self.context.command}"

        if self.context.exit_code is not None:
            base_msg += f"\nExit Code: {self.context.exit_code}"

        if self.context.stderr:
            base_msg += f"\nStderr: {self.context.stderr}"

        if self.cause:
            base_msg += f"\nCaused by: {self.cause}"

        return base_msg

    def with_context(self, **kwargs: Any) -> BuildSystemError:
        """Create a new exception with additional context."""
        new_context = ErrorContext(
            command=kwargs.get("command", self.context.command),
            exit_code=kwargs.get("exit_code", self.context.exit_code),
            working_directory=kwargs.get(
                "working_directory", self.context.working_directory
            ),
            environment_vars={
                **self.context.environment_vars,
                **kwargs.get("environment_vars", {}),
            },
            stdout=kwargs.get("stdout", self.context.stdout),
            stderr=kwargs.get("stderr", self.context.stderr),
            execution_time=kwargs.get("execution_time", self.context.execution_time),
            additional_info={
                **self.context.additional_info,
                **kwargs.get("additional_info", {}),
            },
        )

        return self.__class__(
            str(self),
            context=new_context,
            cause=self.cause,
            recoverable=self.recoverable,
        )


class ConfigurationError(BuildSystemError):
    """Exception raised for errors in the configuration process."""

    def __init__(
        self,
        message: str,
        *,
        config_file: Optional[Union[str, Path]] = None,
        invalid_option: Optional[str] = None,
        **kwargs: Any,
    ) -> None:
        additional_info = kwargs.pop("additional_info", {})
        if config_file:
            additional_info["config_file"] = str(config_file)
        if invalid_option:
            additional_info["invalid_option"] = invalid_option

        context = kwargs.get("context", ErrorContext())
        context.additional_info.update(additional_info)
        kwargs["context"] = context

        super().__init__(message, **kwargs)


class BuildError(BuildSystemError):
    """Exception raised for errors in the build process."""

    def __init__(
        self,
        message: str,
        *,
        target: Optional[str] = None,
        build_system: Optional[str] = None,
        **kwargs: Any,
    ) -> None:
        additional_info = kwargs.pop("additional_info", {})
        if target:
            additional_info["target"] = target
        if build_system:
            additional_info["build_system"] = build_system

        context = kwargs.get("context", ErrorContext())
        context.additional_info.update(additional_info)
        kwargs["context"] = context

        super().__init__(message, **kwargs)


class TestError(BuildSystemError):
    """Exception raised for errors in the testing process."""

    def __init__(
        self,
        message: str,
        *,
        test_suite: Optional[str] = None,
        failed_tests: Optional[int] = None,
        total_tests: Optional[int] = None,
        **kwargs: Any,
    ) -> None:
        additional_info = kwargs.pop("additional_info", {})
        if test_suite:
            additional_info["test_suite"] = test_suite
        if failed_tests is not None:
            additional_info["failed_tests"] = failed_tests
        if total_tests is not None:
            additional_info["total_tests"] = total_tests

        context = kwargs.get("context", ErrorContext())
        context.additional_info.update(additional_info)
        kwargs["context"] = context

        super().__init__(message, **kwargs)


class InstallationError(BuildSystemError):
    """Exception raised for errors in the installation process."""

    def __init__(
        self,
        message: str,
        *,
        install_prefix: Optional[Union[str, Path]] = None,
        permission_error: bool = False,
        **kwargs: Any,
    ) -> None:
        additional_info = kwargs.pop("additional_info", {})
        if install_prefix:
            additional_info["install_prefix"] = str(install_prefix)
        additional_info["permission_error"] = permission_error

        context = kwargs.get("context", ErrorContext())
        context.additional_info.update(additional_info)
        kwargs["context"] = context

        super().__init__(message, **kwargs)


class DependencyError(BuildSystemError):
    """Exception raised for missing or incompatible dependencies."""

    def __init__(
        self,
        message: str,
        *,
        missing_dependency: Optional[str] = None,
        required_version: Optional[str] = None,
        found_version: Optional[str] = None,
        **kwargs: Any,
    ) -> None:
        additional_info = kwargs.pop("additional_info", {})
        if missing_dependency:
            additional_info["missing_dependency"] = missing_dependency
        if required_version:
            additional_info["required_version"] = required_version
        if found_version:
            additional_info["found_version"] = found_version

        context = kwargs.get("context", ErrorContext())
        context.additional_info.update(additional_info)
        kwargs["context"] = context

        super().__init__(message, **kwargs)


def handle_build_error(
    func_name: str,
    error: Exception,
    *,
    context: Optional[ErrorContext] = None,
    recoverable: bool = False,
) -> BuildSystemError:
    """
    Convert generic exceptions to BuildSystemError with context.

    Args:
        func_name: Name of the function where error occurred
        error: The original exception
        context: Error context information
        recoverable: Whether the error is recoverable

    Returns:
        BuildSystemError with enhanced context
    """
    message = f"Error in {func_name}: {str(error)}"

    if isinstance(error, BuildSystemError):
        return error

    # Map common exception types to specific build errors
    if isinstance(error, FileNotFoundError):
        return DependencyError(
            message,
            context=context,
            cause=error,
            recoverable=recoverable,
            missing_dependency=str(error.filename) if error.filename else None,
        )
    elif isinstance(error, PermissionError):
        return InstallationError(
            message,
            context=context,
            cause=error,
            recoverable=recoverable,
            permission_error=True,
        )
    else:
        return BuildSystemError(
            message, context=context, cause=error, recoverable=recoverable
        )
