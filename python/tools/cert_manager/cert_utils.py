#!/usr/bin/env python3
"""
Certificate utility functions.

This module provides utility functions for certificate operations,
such as directory handling and logging decorators.
"""

from functools import wraps
from pathlib import Path
from typing import Callable, Any

from loguru import logger


def ensure_directory_exists(path: Path) -> None:
    """
    Ensure the specified directory exists, creating it if necessary.

    Args:
        path: The directory path to check/create

    Raises:
        OSError: If directory creation fails
    """
    try:
        path.mkdir(exist_ok=True, parents=True)
    except OSError as e:
        logger.error(f"Failed to create directory {path}: {e}")
        raise


def log_operation(func: Callable) -> Callable:
    """
    Decorator to log function calls and their results.

    Args:
        func: The function to decorate

    Returns:
        The decorated function
    """
    @wraps(func)
    def wrapper(*args: Any, **kwargs: Any) -> Any:
        logger.debug(f"Calling {func.__name__}")
        try:
            result = func(*args, **kwargs)
            logger.debug(f"{func.__name__} completed successfully")
            return result
        except Exception as e:
            logger.error(f"Error in {func.__name__}: {str(e)}")
            raise
    return wrapper
