#!/usr/bin/env python3
"""
Enhanced Pacman Manager Package
Modern Python package manager interface with advanced features and type safety.
"""

from __future__ import annotations

__version__ = "2.0.0"
__author__ = "Lithium Development Team"
__description__ = "Advanced Pacman Package Manager with Modern Python Features"

# Core exports
from .manager import PacmanManager
from .config import PacmanConfig
from .models import PackageInfo, PackageStatus, CommandResult
# Exceptions
from .exceptions import (
    PacmanError, CommandError, PackageNotFoundError, ConfigError,
    DependencyError, PermissionError, NetworkError, CacheError,
    ValidationError, PluginError, DatabaseError, RepositoryError,
    SignatureError, LockError, ErrorContext, create_error_context
)
from .async_manager import AsyncPacmanManager
from .api import PacmanAPI
from .cli import CLI
from .cache import PackageCache
from .analytics import PackageAnalytics
from .plugins import (
    PluginManager,
    PluginBase,
    LoggingPlugin,
    BackupPlugin,
    NotificationPlugin,
    SecurityPlugin,
)

# Type definitions
from .pacman_types import (
    PackageName,
    PackageVersion,
    RepositoryName,
    CacheKey,
    CommandOptions,
    SearchFilter,
)

# Context managers
from .context import PacmanContext, async_pacman_context

# Decorators
from .decorators import (
    require_sudo,
    validate_package,
    cache_result,
    retry_on_failure,
    benchmark,
)

__all__ = [
    # Core classes
    "PacmanManager",
    "AsyncPacmanManager",
    "PacmanConfig",
    "PacmanAPI",
    "CLI",

    # Data models
    "PackageInfo",
    "PackageStatus",
    "CommandResult",

    # Exceptions
    "PacmanError",
    "CommandError",
    "PackageNotFoundError",
    "ConfigError",
    "DependencyError",
    "PermissionError",
    "NetworkError",
    "CacheError",
    "ValidationError",
    "PluginError",
    "DatabaseError",
    "RepositoryError",
    "SignatureError",
    "LockError",
    "ErrorContext",
    "create_error_context",

    # Advanced features
    "PackageCache",
    "PackageAnalytics",
    "PluginManager",
    "PluginBase",
    "LoggingPlugin",
    "BackupPlugin",
    "NotificationPlugin",
    "SecurityPlugin",

    # Type definitions
    "PackageName",
    "PackageVersion",
    "RepositoryName",
    "CacheKey",
    "CommandOptions",
    "SearchFilter",

    # Context managers
    "PacmanContext",
    "async_pacman_context",

    # Decorators
    "require_sudo",
    "validate_package",
    "cache_result",
    "retry_on_failure",
    "benchmark",

    # Metadata
    "__version__",
    "__author__",
    "__description__",
]

# Module-level convenience functions


def quick_install(package: str, **kwargs) -> bool:
    """Quick package installation with sensible defaults."""
    try:
        manager = PacmanManager()
        result = manager.install_package(package, **kwargs)
        # Handle different return types
        if hasattr(result, '__getitem__') and 'success' in result:
            return result['success']
        return bool(result)
    except Exception:
        return False


def quick_search(query: str, limit: int = 10) -> list[PackageInfo]:
    """Quick package search with limited results."""
    try:
        manager = PacmanManager()
        results = manager.search_package(query)
        return results[:limit] if limit else results
    except Exception:
        return []


async def async_quick_install(package: str, **kwargs) -> bool:
    """Async quick package installation."""
    try:
        from .async_manager import AsyncPacmanManager
        manager = AsyncPacmanManager()
        result = await manager.install_package(package, **kwargs)
        # Handle different return types
        if hasattr(result, '__getitem__') and 'success' in result:
            return result['success']
        return bool(result)
    except Exception:
        return False


async def async_quick_search(query: str, limit: int = 10) -> list[PackageInfo]:
    """Async quick package search."""
    try:
        from .async_manager import AsyncPacmanManager
        manager = AsyncPacmanManager()
        results = await manager.search_packages(query, limit=limit)
        return results
    except Exception:
        return []
