#!/usr/bin/env python3
"""
Type definitions for the enhanced pacman manager.
Uses modern Python typing features including type aliases and NewType.
"""

from __future__ import annotations

from typing import NewType, TypedDict, Literal, Union, Any
from pathlib import Path
from collections.abc import Callable, Awaitable
from dataclasses import dataclass

# Strong type aliases using NewType for better type safety
PackageName = NewType("PackageName", str)
PackageVersion = NewType("PackageVersion", str)
RepositoryName = NewType("RepositoryName", str)
CacheKey = NewType("CacheKey", str)

# Type aliases for complex types
type PackageDict = dict[PackageName, PackageVersion]
type RepositoryDict = dict[RepositoryName, list[PackageName]]
type SearchResults = list[tuple[PackageName, PackageVersion, str]]

# Literal types for constrained values
type PackageAction = Literal["install",
                             "remove", "upgrade", "downgrade", "search"]
type SortOrder = Literal["name", "version", "size", "date", "repository"]
type LogLevel = Literal["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]
type CommandStatus = Literal["pending",
                             "running", "completed", "failed", "cancelled"]

# Union types for flexibility
type PathLike = Union[str, Path]
type PackageIdentifier = Union[PackageName, str]
type CommandOutput = Union[str, bytes]

# TypedDict for structured data


class CommandOptions(TypedDict, total=False):
    """Options for package management commands."""
    force: bool
    no_deps: bool
    as_deps: bool
    needed: bool
    ignore_deps: bool
    recursive: bool
    cascade: bool
    nosave: bool
    verbose: bool
    quiet: bool
    parallel_downloads: int
    timeout: int


class SearchFilter(TypedDict, total=False):
    """Filters for package search operations."""
    repository: RepositoryName | None
    installed_only: bool
    outdated_only: bool
    sort_by: SortOrder
    limit: int
    category: str | None
    min_size: int | None
    max_size: int | None


class CacheConfig(TypedDict, total=False):
    """Configuration for package cache."""
    max_size: int
    ttl_seconds: int
    use_disk_cache: bool
    cache_directory: PathLike
    compression_enabled: bool


class RetryConfig(TypedDict, total=False):
    """Configuration for retry mechanisms."""
    max_attempts: int
    backoff_factor: float
    retry_on_errors: list[type[Exception]]
    timeout_seconds: int


# Callback types
type ProgressCallback = Callable[[int, int, str], None]
type AsyncProgressCallback = Callable[[int, int, str], Awaitable[None]]
type ErrorHandler = Callable[[Exception], bool]
type AsyncErrorHandler = Callable[[Exception], Awaitable[bool]]

# Advanced generic types for plugin system
type PluginHook[T] = Callable[..., T]
type AsyncPluginHook[T] = Callable[..., Awaitable[T]]

# Pattern matching support with dataclass


@dataclass(frozen=True, slots=True)
class CommandPattern:
    """Pattern for matching commands in pattern matching."""
    action: PackageAction
    target: PackageIdentifier | None = None
    options: CommandOptions | None = None

    def matches(self, other: CommandPattern) -> bool:
        """Check if this pattern matches another."""
        return (
            self.action == other.action and
            (self.target is None or self.target == other.target)
        )

# Result types for operations


@dataclass(frozen=True, slots=True)
class OperationResult[T]:
    """Generic result type for operations."""
    success: bool
    data: T | None = None
    error: Exception | None = None
    duration: float = 0.0
    metadata: dict[str, Any] | None = None

    @property
    def is_success(self) -> bool:
        return self.success and self.error is None

    @property
    def is_failure(self) -> bool:
        return not self.success or self.error is not None


# Async result type
type AsyncResult[T] = Awaitable[OperationResult[T]]

# Event types for notifications


@dataclass(frozen=True, slots=True)
class PackageEvent:
    """Event data for package operations."""
    event_type: str
    package_name: PackageName
    timestamp: float
    data: dict[str, Any] | None = None


type EventHandler = Callable[[PackageEvent], None]
type AsyncEventHandler = Callable[[PackageEvent], Awaitable[None]]

# Configuration types


class ManagerConfig(TypedDict, total=False):
    """Configuration for PacmanManager."""
    config_path: PathLike | None
    use_sudo: bool
    parallel_downloads: int
    cache_config: CacheConfig
    retry_config: RetryConfig
    log_level: LogLevel
    enable_plugins: bool
    plugin_directories: list[PathLike]


# Export all type definitions
__all__ = [
    # NewTypes
    "PackageName",
    "PackageVersion",
    "RepositoryName",
    "CacheKey",

    # Type aliases
    "PackageDict",
    "RepositoryDict",
    "SearchResults",
    "PathLike",
    "PackageIdentifier",
    "CommandOutput",

    # Literal types
    "PackageAction",
    "SortOrder",
    "LogLevel",
    "CommandStatus",

    # TypedDict classes
    "CommandOptions",
    "SearchFilter",
    "CacheConfig",
    "RetryConfig",
    "ManagerConfig",

    # Callback types
    "ProgressCallback",
    "AsyncProgressCallback",
    "ErrorHandler",
    "AsyncErrorHandler",

    # Plugin types
    "PluginHook",
    "AsyncPluginHook",

    # Data classes
    "CommandPattern",
    "OperationResult",
    "PackageEvent",

    # Event types
    "EventHandler",
    "AsyncEventHandler",

    # Async types
    "AsyncResult",
]
