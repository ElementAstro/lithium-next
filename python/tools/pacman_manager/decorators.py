#!/usr/bin/env python3
"""
Advanced decorators for the enhanced pacman manager.
Provides functionality for validation, caching, retry logic, and performance monitoring.
"""

from __future__ import annotations

import time
import functools
import asyncio
import os
from typing import TypeVar, ParamSpec, Callable, Any
from collections.abc import Awaitable

from loguru import logger

from .exceptions import CommandError, PackageNotFoundError
from .pacman_types import PackageName, OperationResult

T = TypeVar("T")
P = ParamSpec("P")

# Cache storage for memoization
_cache: dict[str, tuple[Any, float]] = {}
_cache_ttl = 300  # 5 minutes default TTL


def require_sudo(func: Callable[P, T]) -> Callable[P, T]:
    """
    Decorator that ensures sudo privileges are available for operations that require them.
    Uses modern Python pattern matching for improved error handling.
    """

    @functools.wraps(func)
    def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
        # Check if we're on Windows (no sudo needed)
        if os.name == "nt":
            return func(*args, **kwargs)

        # Check if running as root
        if os.geteuid() == 0:
            return func(*args, **kwargs)

        # Check if the manager has use_sudo enabled
        instance = args[0] if args else None
        use_sudo = getattr(instance, "use_sudo", True)

        if not use_sudo:
            logger.warning(
                f"Function {func.__name__} requires sudo but use_sudo is disabled"
            )
            raise PermissionError(f"Function {func.__name__} requires sudo privileges")

        return func(*args, **kwargs)

    return wrapper


def validate_package(func: Callable[P, T]) -> Callable[P, T]:
    """
    Decorator that validates package names before processing.
    Uses pattern matching for comprehensive validation.
    """

    @functools.wraps(func)
    def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
        # Extract package name from arguments
        package_name = None

        # Try to find package name in positional args
        if len(args) > 1 and isinstance(args[1], str):
            package_name = args[1]

        # Try to find in keyword arguments
        for key in ["package", "package_name", "name"]:
            if key in kwargs:
                package_name = kwargs[key]
                break

        if package_name:
            # Validate package name using pattern matching
            match package_name:
                case str() if not package_name.strip():
                    raise ValueError("Package name cannot be empty")
                case str() if any(
                    char in package_name for char in ["/", "\\", "<", ">", "|"]
                ):
                    raise ValueError(
                        f"Invalid characters in package name: {package_name}"
                    )
                case PackageName():
                    pass  # Already validated
                case _:
                    logger.warning(
                        f"Unexpected package name type: {type(package_name)}"
                    )

        return func(*args, **kwargs)

    return wrapper


def cache_result(
    ttl: int = 300, key_func: Callable[..., str] | None = None
) -> Callable[[Callable[P, T]], Callable[P, T]]:
    """
    Decorator for caching function results with TTL support.
    Uses advanced type hints and modern Python features.
    """

    def decorator(func: Callable[P, T]) -> Callable[P, T]:
        @functools.wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
            # Generate cache key
            if key_func:
                cache_key = key_func(*args, **kwargs)
            else:
                cache_key = f"{func.__module__}.{func.__name__}:{hash((args, tuple(sorted(kwargs.items()))))}"

            # Check cache
            if cache_key in _cache:
                result, timestamp = _cache[cache_key]
                if time.time() - timestamp < ttl:
                    logger.debug(f"Cache hit for {func.__name__}")
                    return result
                else:
                    # Remove expired entry
                    del _cache[cache_key]

            # Execute function and cache result
            result = func(*args, **kwargs)
            _cache[cache_key] = (result, time.time())
            logger.debug(f"Cached result for {func.__name__}")

            return result

        # Add cache management methods via setattr to avoid type checker issues
        setattr(wrapper, "cache_clear", lambda: _cache.clear())
        setattr(
            wrapper,
            "cache_info",
            lambda: {
                "size": len(_cache),
                "hits": sum(
                    1 for _, (_, ts) in _cache.items() if time.time() - ts < ttl
                ),
            },
        )

        return wrapper

    return decorator


def retry_on_failure(
    max_attempts: int = 3,
    backoff_factor: float = 1.0,
    retry_on: tuple[type[Exception], ...] = (CommandError,),
    give_up_on: tuple[type[Exception], ...] = (PackageNotFoundError,),
) -> Callable[[Callable[P, T]], Callable[P, T]]:
    """
    Decorator for automatic retry with exponential backoff.
    Uses modern exception handling and type annotations.
    """

    def decorator(func: Callable[P, T]) -> Callable[P, T]:
        @functools.wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
            last_exception = None

            for attempt in range(max_attempts):
                try:
                    return func(*args, **kwargs)
                except Exception as e:
                    last_exception = e

                    # Check if we should give up immediately
                    if isinstance(e, give_up_on):
                        logger.error(f"Giving up on {func.__name__} due to: {e}")
                        raise

                    # Check if we should retry
                    if not isinstance(e, retry_on):
                        logger.error(f"Not retrying {func.__name__} due to: {e}")
                        raise

                    # Don't sleep on the last attempt
                    if attempt < max_attempts - 1:
                        sleep_time = backoff_factor * (2**attempt)
                        logger.warning(
                            f"Attempt {attempt + 1} failed, retrying in {sleep_time}s: {e}"
                        )
                        time.sleep(sleep_time)
                    else:
                        logger.error(
                            f"All {max_attempts} attempts failed for {func.__name__}"
                        )

            # If we get here, all attempts failed
            raise last_exception or RuntimeError("All retry attempts failed")

        return wrapper

    return decorator


def benchmark(log_level: str = "INFO") -> Callable[[Callable[P, T]], Callable[P, T]]:
    """
    Decorator for benchmarking function execution time.
    Provides detailed performance metrics.
    """

    def decorator(func: Callable[P, T]) -> Callable[P, T]:
        @functools.wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
            start_time = time.perf_counter()
            start_process_time = time.process_time()
            success = False

            try:
                result = func(*args, **kwargs)
                success = True
                return result
            except Exception as e:
                success = False
                raise
            finally:
                end_time = time.perf_counter()
                end_process_time = time.process_time()

                wall_time = end_time - start_time
                cpu_time = end_process_time - start_process_time

                status = "✓" if success else "✗"
                message = (
                    f"{status} {func.__name__} | "
                    f"Wall: {wall_time:.3f}s | "
                    f"CPU: {cpu_time:.3f}s | "
                    f"Efficiency: {(cpu_time/wall_time)*100:.1f}%"
                )

                logger.log(log_level, message)

        return wrapper

    return decorator


# Async versions of decorators
def async_cache_result(
    ttl: int = 300, key_func: Callable[..., str] | None = None
) -> Callable[[Callable[P, Awaitable[T]]], Callable[P, Awaitable[T]]]:
    """Async version of cache_result decorator."""

    def decorator(func: Callable[P, Awaitable[T]]) -> Callable[P, Awaitable[T]]:
        @functools.wraps(func)
        async def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
            # Generate cache key
            if key_func:
                cache_key = key_func(*args, **kwargs)
            else:
                cache_key = f"{func.__module__}.{func.__name__}:{hash((args, tuple(sorted(kwargs.items()))))}"

            # Check cache
            if cache_key in _cache:
                result, timestamp = _cache[cache_key]
                if time.time() - timestamp < ttl:
                    logger.debug(f"Cache hit for async {func.__name__}")
                    return result
                else:
                    del _cache[cache_key]

            # Execute function and cache result
            result = await func(*args, **kwargs)
            _cache[cache_key] = (result, time.time())
            logger.debug(f"Cached result for async {func.__name__}")

            return result

        return wrapper

    return decorator


def async_retry_on_failure(
    max_attempts: int = 3,
    backoff_factor: float = 1.0,
    retry_on: tuple[type[Exception], ...] = (CommandError,),
    give_up_on: tuple[type[Exception], ...] = (PackageNotFoundError,),
) -> Callable[[Callable[P, Awaitable[T]]], Callable[P, Awaitable[T]]]:
    """Async version of retry_on_failure decorator."""

    def decorator(func: Callable[P, Awaitable[T]]) -> Callable[P, Awaitable[T]]:
        @functools.wraps(func)
        async def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
            last_exception = None

            for attempt in range(max_attempts):
                try:
                    return await func(*args, **kwargs)
                except Exception as e:
                    last_exception = e

                    if isinstance(e, give_up_on):
                        logger.error(f"Giving up on async {func.__name__} due to: {e}")
                        raise

                    if not isinstance(e, retry_on):
                        logger.error(f"Not retrying async {func.__name__} due to: {e}")
                        raise

                    if attempt < max_attempts - 1:
                        sleep_time = backoff_factor * (2**attempt)
                        logger.warning(
                            f"Async attempt {attempt + 1} failed, retrying in {sleep_time}s: {e}"
                        )
                        await asyncio.sleep(sleep_time)
                    else:
                        logger.error(
                            f"All {max_attempts} async attempts failed for {func.__name__}"
                        )

            raise last_exception or RuntimeError("All async retry attempts failed")

        return wrapper

    return decorator


def async_benchmark(
    log_level: str = "INFO",
) -> Callable[[Callable[P, Awaitable[T]]], Callable[P, Awaitable[T]]]:
    """Async version of benchmark decorator."""

    def decorator(func: Callable[P, Awaitable[T]]) -> Callable[P, Awaitable[T]]:
        @functools.wraps(func)
        async def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
            start_time = time.perf_counter()
            success = False

            try:
                result = await func(*args, **kwargs)
                success = True
                return result
            except Exception as e:
                success = False
                raise
            finally:
                end_time = time.perf_counter()
                wall_time = end_time - start_time

                status = "✓" if success else "✗"
                message = f"{status} async {func.__name__} | Wall: {wall_time:.3f}s"
                logger.log(log_level, message)

        return wrapper

    return decorator


# Utility decorator for operation results
def wrap_operation_result(func: Callable[P, T]) -> Callable[P, OperationResult[T]]:
    """
    Decorator that wraps function results in OperationResult for consistent error handling.
    """

    @functools.wraps(func)
    def wrapper(*args: P.args, **kwargs: P.kwargs) -> OperationResult[T]:
        start_time = time.perf_counter()

        try:
            result = func(*args, **kwargs)
            duration = time.perf_counter() - start_time
            return OperationResult(success=True, data=result, duration=duration)
        except Exception as e:
            duration = time.perf_counter() - start_time
            return OperationResult(success=False, error=e, duration=duration)

    return wrapper


# Export all decorators
__all__ = [
    "require_sudo",
    "validate_package",
    "cache_result",
    "retry_on_failure",
    "benchmark",
    "async_cache_result",
    "async_retry_on_failure",
    "async_benchmark",
    "wrap_operation_result",
]
