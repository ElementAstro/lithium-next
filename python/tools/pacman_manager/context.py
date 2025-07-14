#!/usr/bin/env python3
"""
Context managers for the enhanced pacman manager.
Provides resource management and transaction-like operations.
"""

from __future__ import annotations

import asyncio
import contextlib
from typing import TypeVar, Optional, Any, Dict
from collections.abc import Generator, AsyncGenerator
from pathlib import Path

from loguru import logger

from .manager import PacmanManager
from .exceptions import PacmanError


T = TypeVar("T")


class PacmanContext:
    """
    Context manager for pacman operations with automatic resource cleanup.
    Provides transaction-like behavior for package operations.
    """

    def __init__(
        self, config_path: Optional[Path] = None, use_sudo: bool = True, **kwargs
    ):
        """Initialize the context with configuration."""
        self.config_path = config_path
        self.use_sudo = use_sudo
        self.extra_config = kwargs
        self._manager: PacmanManager | None = None
        self._operations: list[str] = []

    def __enter__(self) -> PacmanManager:
        """Enter the context and create manager instance."""
        try:
            self._manager = PacmanManager(
                {"config_path": self.config_path, "use_sudo": self.use_sudo}
            )
            logger.debug("Entered PacmanContext")
            return self._manager
        except Exception as e:
            logger.error(f"Failed to enter PacmanContext: {e}")
            raise

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        """Exit the context with cleanup."""
        if exc_type is not None:
            logger.error(f"Exception in PacmanContext: {exc_type.__name__}: {exc_val}")

        # Cleanup
        if self._manager:
            self._cleanup_manager()

        logger.debug("Exited PacmanContext")
        return False  # Don't suppress exceptions

    def _cleanup_manager(self) -> None:
        """Clean up manager resources."""
        if self._manager and hasattr(self._manager, "_executor"):
            try:
                self._manager._executor.shutdown(wait=True)
            except AttributeError:
                pass  # Executor might not exist
        self._manager = None


class AsyncPacmanContext:
    """
    Async context manager for pacman operations.
    """

    def __init__(
        self, config_path: Optional[Path] = None, use_sudo: bool = True, **kwargs
    ):
        """Initialize the async context with configuration."""
        self.config_path = config_path
        self.use_sudo = use_sudo
        self.extra_config = kwargs
        self._manager = None

    async def __aenter__(self):
        """Enter the async context and create manager instance."""
        try:
            # For now, use regular manager - async manager will be implemented separately
            self._manager = PacmanManager(
                {"config_path": self.config_path, "use_sudo": self.use_sudo}
            )
            logger.debug("Entered AsyncPacmanContext")
            return self._manager
        except Exception as e:
            logger.error(f"Failed to enter AsyncPacmanContext: {e}")
            raise

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> bool:
        """Exit the async context with cleanup."""
        if exc_type is not None:
            logger.error(
                f"Exception in AsyncPacmanContext: {exc_type.__name__}: {exc_val}"
            )

        # Cleanup
        if self._manager:
            await self._cleanup_manager()

        logger.debug("Exited AsyncPacmanContext")
        return False

    async def _cleanup_manager(self) -> None:
        """Clean up async manager resources."""
        if self._manager and hasattr(self._manager, "_executor"):
            try:
                self._manager._executor.shutdown(wait=True)
            except AttributeError:
                pass  # Executor might not exist
        self._manager = None


@contextlib.contextmanager
def temp_config(
    manager: PacmanManager, **config_overrides
) -> Generator[PacmanManager, None, None]:
    """
    Temporarily modify manager configuration within a context.
    """
    original_config = {}

    # Store original values
    for key, value in config_overrides.items():
        if hasattr(manager, key):
            original_config[key] = getattr(manager, key)
            setattr(manager, key, value)

    try:
        yield manager
    finally:
        # Restore original values
        for key, value in original_config.items():
            setattr(manager, key, value)


@contextlib.contextmanager
def suppressed_output(manager: PacmanManager) -> Generator[PacmanManager, None, None]:
    """
    Suppress all output from pacman operations within the context.
    Note: This is a convenience context that doesn't actually suppress output
    but provides a consistent interface for future implementation.
    """
    logger.debug("Entering suppressed output mode")
    try:
        yield manager
    finally:
        logger.debug("Exiting suppressed output mode")


# Convenience functions
def pacman_context(
    config_path: Optional[Path] = None, use_sudo: bool = True, **kwargs
) -> PacmanContext:
    """Create a PacmanContext with optional configuration."""
    return PacmanContext(config_path, use_sudo, **kwargs)


def async_pacman_context(
    config_path: Optional[Path] = None, use_sudo: bool = True, **kwargs
) -> AsyncPacmanContext:
    """Create an AsyncPacmanContext with optional configuration."""
    return AsyncPacmanContext(config_path, use_sudo, **kwargs)


# Export all context managers
__all__ = [
    "PacmanContext",
    "AsyncPacmanContext",
    "temp_config",
    "suppressed_output",
    "pacman_context",
    "async_pacman_context",
]
