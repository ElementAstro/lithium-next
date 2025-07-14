from __future__ import annotations

#!/usr/bin/env python3
"""
Enhanced exception types for the Pacman Package Manager.
Provides structured error handling with context and debugging information.
"""

from __future__ import annotations

import time
from pathlib import Path
from typing import Any, Optional
from dataclasses import dataclass, field

from .pacman_types import CommandOutput


@dataclass(frozen=True, slots=True)
class ErrorContext:
    """Context information for debugging errors."""

    timestamp: float = field(default_factory=time.time)
    working_directory: Optional[Path] = None
    environment_vars: dict[str, str] = field(default_factory=dict)
    system_info: dict[str, Any] = field(default_factory=dict)
    additional_data: dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "timestamp": self.timestamp,
            "working_directory": (
                str(self.working_directory) if self.working_directory else None
            ),
            "environment_vars": self.environment_vars,
            "system_info": self.system_info,
            "additional_data": self.additional_data,
        }


class PacmanError(Exception):
    """
    Base exception for all pacman-related errors with enhanced context.

    Provides structured error information for better debugging and handling.
    """

    def __init__(
        self,
        message: str,
        *,
        error_code: Optional[str] = None,
        context: Optional[ErrorContext] = None,
        original_error: Optional[Exception] = None,
        **extra_context: Any,
    ):
        super().__init__(message)
        self.error_code = error_code or self.__class__.__name__.upper()
        self.context = context or ErrorContext()
        self.original_error = original_error
        self.extra_context = extra_context

        # Add extra context to the error context
        if extra_context:
            self.context.additional_data.update(extra_context)

    def to_dict(self) -> dict[str, Any]:
        """Convert exception to structured dictionary."""
        return {
            "error_type": self.__class__.__name__,
            "message": str(self),
            "error_code": self.error_code,
            "context": self.context.to_dict(),
            "original_error": str(self.original_error) if self.original_error else None,
            "extra_context": self.extra_context,
        }

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(message={str(self)!r}, error_code={self.error_code!r})"


class OperationError(PacmanError):
    """Raised for general operational failures in pacman manager."""

    pass


class CommandError(PacmanError):
    """Exception raised when a command execution fails."""

    def __init__(
        self,
        message: str,
        return_code: int,
        stderr: CommandOutput,
        *,
        command: Optional[list[str]] = None,
        stdout: Optional[CommandOutput] = None,
        duration: Optional[float] = None,
        **kwargs: Any,
    ):
        self.return_code = return_code
        self.stderr = stderr
        self.command = command or []
        self.stdout = stdout
        self.duration = duration

        enhanced_message = f"{message} (Return code: {return_code})"
        if stderr:
            enhanced_message += f": {stderr}"

        super().__init__(
            enhanced_message,
            error_code="COMMAND_FAILED",
            command=command,
            return_code=return_code,
            stderr=str(stderr),
            stdout=str(stdout) if stdout else None,
            duration=duration,
            **kwargs,
        )


class PackageNotFoundError(PacmanError):
    """Exception raised when a package is not found."""

    def __init__(
        self,
        package_name: str,
        repository: Optional[str] = None,
        searched_repositories: Optional[list[str]] = None,
        **kwargs: Any,
    ):
        self.package_name = package_name
        self.repository = repository
        self.searched_repositories = searched_repositories or []

        message = f"Package '{package_name}' not found"
        if repository:
            message += f" in repository '{repository}'"
        elif searched_repositories:
            message += f" in repositories: {', '.join(searched_repositories)}"

        super().__init__(
            message,
            error_code="PACKAGE_NOT_FOUND",
            package_name=package_name,
            repository=repository,
            searched_repositories=searched_repositories,
            **kwargs,
        )


class ConfigError(PacmanError):
    """Exception raised when there's a configuration error."""

    def __init__(
        self,
        message: str,
        config_path: Optional[Path] = None,
        config_section: Optional[str] = None,
        invalid_option: Optional[str] = None,
        **kwargs: Any,
    ):
        self.config_path = config_path
        self.config_section = config_section
        self.invalid_option = invalid_option

        super().__init__(
            message,
            error_code="CONFIG_ERROR",
            config_path=str(config_path) if config_path else None,
            config_section=config_section,
            invalid_option=invalid_option,
            **kwargs,
        )


class DependencyError(PacmanError):
    """Exception raised when package dependencies cannot be resolved."""

    def __init__(
        self,
        message: str,
        package_name: Optional[str] = None,
        missing_dependencies: Optional[list[str]] = None,
        conflicting_packages: Optional[list[str]] = None,
        **kwargs: Any,
    ):
        self.package_name = package_name
        self.missing_dependencies = missing_dependencies or []
        self.conflicting_packages = conflicting_packages or []

        super().__init__(
            message,
            error_code="DEPENDENCY_ERROR",
            package_name=package_name,
            missing_dependencies=missing_dependencies,
            conflicting_packages=conflicting_packages,
            **kwargs,
        )


class PermissionError(PacmanError):
    """Exception raised when insufficient permissions for an operation."""

    def __init__(
        self,
        message: str,
        required_permission: Optional[str] = None,
        operation: Optional[str] = None,
        **kwargs: Any,
    ):
        self.required_permission = required_permission
        self.operation = operation

        super().__init__(
            message,
            error_code="PERMISSION_DENIED",
            required_permission=required_permission,
            operation=operation,
            **kwargs,
        )


class NetworkError(PacmanError):
    """Exception raised when network operations fail."""

    def __init__(
        self,
        message: str,
        url: Optional[str] = None,
        status_code: Optional[int] = None,
        timeout: Optional[float] = None,
        **kwargs: Any,
    ):
        self.url = url
        self.status_code = status_code
        self.timeout = timeout

        super().__init__(
            message,
            error_code="NETWORK_ERROR",
            url=url,
            status_code=status_code,
            timeout=timeout,
            **kwargs,
        )


class CacheError(PacmanError):
    """Exception raised when cache operations fail."""

    def __init__(
        self,
        message: str,
        cache_path: Optional[Path] = None,
        operation: Optional[str] = None,
        **kwargs: Any,
    ):
        self.cache_path = cache_path
        self.operation = operation

        super().__init__(
            message,
            error_code="CACHE_ERROR",
            cache_path=str(cache_path) if cache_path else None,
            operation=operation,
            **kwargs,
        )


class ValidationError(PacmanError):
    """Exception raised when input validation fails."""

    def __init__(
        self,
        message: str,
        field_name: Optional[str] = None,
        invalid_value: Any = None,
        expected_type: Optional[type] = None,
        **kwargs: Any,
    ):
        self.field_name = field_name
        self.invalid_value = invalid_value
        self.expected_type = expected_type

        super().__init__(
            message,
            error_code="VALIDATION_ERROR",
            field_name=field_name,
            invalid_value=str(invalid_value) if invalid_value is not None else None,
            expected_type=expected_type.__name__ if expected_type else None,
            **kwargs,
        )


class PluginError(PacmanError):
    """Exception raised when plugin operations fail."""

    def __init__(
        self,
        message: str,
        plugin_name: Optional[str] = None,
        plugin_version: Optional[str] = None,
        **kwargs: Any,
    ):
        self.plugin_name = plugin_name
        self.plugin_version = plugin_version

        super().__init__(
            message,
            error_code="PLUGIN_ERROR",
            plugin_name=plugin_name,
            plugin_version=plugin_version,
            **kwargs,
        )


# Exception hierarchy for easier catching
DatabaseError = type("DatabaseError", (PacmanError,), {})
RepositoryError = type("RepositoryError", (PacmanError,), {})
SignatureError = type("SignatureError", (PacmanError,), {})
LockError = type("LockError", (PacmanError,), {})


def create_error_context(
    working_dir: Optional[Path] = None,
    env_vars: Optional[dict[str, str]] = None,
    **extra: Any,
) -> ErrorContext:
    """Create an error context with current system information."""
    import os
    import platform

    return ErrorContext(
        working_directory=working_dir or Path.cwd(),
        environment_vars=env_vars or dict(os.environ),
        system_info={
            "platform": platform.platform(),
            "python_version": platform.python_version(),
            "architecture": platform.architecture(),
            "processor": platform.processor(),
        },
        additional_data=extra,
    )


__all__ = [
    "OperationError",
    # Base exception
    "PacmanError",
    # Core exceptions
    "CommandError",
    "PackageNotFoundError",
    "ConfigError",
    "DependencyError",
    "PermissionError",
    "NetworkError",
    "CacheError",
    "ValidationError",
    "PluginError",
    # Specialized exceptions
    "OperationError",
    "DatabaseError",
    "RepositoryError",
    "SignatureError",
    "LockError",
    # Utilities
    "ErrorContext",
    "create_error_context",
]
