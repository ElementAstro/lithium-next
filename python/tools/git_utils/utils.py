#!/usr/bin/env python3
"""
Enhanced utility functions for Git operations with modern Python features.
Provides high-performance, type-safe utilities for Git repository management.
"""

from __future__ import annotations

import os
import re
import asyncio
import contextlib
import time
from contextlib import contextmanager, asynccontextmanager
from functools import wraps, lru_cache
from pathlib import Path
from typing import (
    Union,
    Callable,
    TypeVar,
    ParamSpec,
    Awaitable,
    Optional,
    Any,
    List,
    AsyncGenerator,
    Generator,
    Protocol,
    runtime_checkable,
)

from loguru import logger

from .exceptions import GitRepositoryNotFound, GitException, create_git_error_context
from .models import GitOperation


# Type variables for generic functions
T = TypeVar("T")
P = ParamSpec("P")
F = TypeVar("F", bound=Callable[..., Any])
AsyncF = TypeVar("AsyncF", bound=Callable[..., Awaitable[Any]])


@runtime_checkable
class GitRepositoryProtocol(Protocol):
    """Protocol for objects that have a repository directory."""

    repo_dir: Optional[Path]


@contextmanager
def change_directory(path: Optional[Union[str, Path]]) -> Generator[Path, None, None]:
    """
    Enhanced context manager for changing the current working directory.

    Args:
        path: The directory to change to. If None, stays in current directory.

    Yields:
        Path: The directory we changed to (or current if path was None)

    Example:
        >>> with change_directory(Path("/path/to/dir")) as current_dir:
        ...     # Operations in the directory
        ...     print(f"Working in {current_dir}")
    """
    if path is None:
        yield Path.cwd()
        return

    original_dir = Path.cwd()
    target_dir = ensure_path(path)

    if target_dir is None:  # Added check for None after ensure_path
        logger.warning(
            f"Invalid target directory path: {path}, staying in {original_dir}"
        )
        yield original_dir
        return

    try:
        if not target_dir.exists():
            logger.warning(
                f"Directory {target_dir} does not exist, staying in {original_dir}"
            )
            yield original_dir
            return

        logger.debug(f"Changing directory from {original_dir} to {target_dir}")
        os.chdir(target_dir)
        yield target_dir
    except OSError as e:
        logger.error(f"Failed to change directory to {target_dir}: {e}")
        raise GitException(
            f"Failed to change directory to {target_dir}: {e}",
            original_error=e,
            target_directory=str(target_dir),
            original_directory=str(original_dir),
        )
    finally:
        try:
            os.chdir(original_dir)
            logger.debug(f"Restored directory to {original_dir}")
        except OSError as e:
            logger.error(f"Failed to restore directory to {original_dir}: {e}")


@asynccontextmanager
async def async_change_directory(path: Optional[Path]) -> AsyncGenerator[Path, None]:
    """
    Async context manager for changing the current working directory.

    Args:
        path: The directory to change to. If None, stays in current directory.

    Yields:
        Path: The directory we changed to (or current if path was None)
    """
    # Since os.chdir is synchronous, we wrap it in a context manager
    # In a real async application, you might want to use a different approach
    with change_directory(path) as current_dir:
        yield current_dir


@lru_cache(maxsize=128)
def ensure_path(path: Union[str, Path, None]) -> Optional[Path]:
    """
    Convert a string to a Path object with caching for performance.

    Args:
        path: String path, Path object, or None.

    Returns:
        Path: A Path object representing the input path, or None if input was None.
    """
    if path is None:
        return None
    return path if isinstance(path, Path) else Path(path).resolve()


@lru_cache(maxsize=32)
def is_git_repository(repo_path: Path) -> bool:
    """
    Check if a directory is a Git repository with caching.

    Args:
        repo_path: Path to check.

    Returns:
        bool: True if the path is a Git repository.
    """
    if not repo_path.exists():
        return False

    # Check for .git directory or file (for worktrees)
    git_path = repo_path / ".git"
    if git_path.is_dir():
        return True

    # Check if .git is a file (Git worktree)
    if git_path.is_file():
        try:
            content = git_path.read_text().strip()
            return content.startswith("gitdir:")
        except (OSError, UnicodeDecodeError):
            return False

    return False


def performance_monitor(operation: GitOperation):
    """
    Decorator to monitor performance of Git operations.

    Args:
        operation: The Git operation being performed.
    """

    def decorator(func: F) -> F:
        @wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> Any:
            start_time = time.time()
            operation_name = f"{operation.name.lower()}_{func.__name__}"

            logger.debug(f"Starting {operation_name}")

            try:
                result = func(*args, **kwargs)
                duration = time.time() - start_time

                logger.info(
                    f"Completed {operation_name}",
                    extra={
                        "operation": operation.name,
                        "duration": duration,
                        "function": func.__name__,
                        "success": getattr(result, "success", True),
                    },
                )

                # Add performance info to result if it's a GitResult
                if hasattr(result, "duration") and result.duration is None:
                    result.duration = duration
                if hasattr(result, "operation") and result.operation is None:
                    result.operation = operation

                return result

            except Exception as e:
                duration = time.time() - start_time
                logger.error(
                    f"Failed {operation_name}",
                    extra={
                        "operation": operation.name,
                        "duration": duration,
                        "function": func.__name__,
                        "error": str(e),
                    },
                )
                raise

        return wrapper  # type: ignore

    return decorator


def async_performance_monitor(operation: GitOperation):
    """
    Decorator to monitor performance of async Git operations.

    Args:
        operation: The Git operation being performed.
    """

    def decorator(func: AsyncF) -> AsyncF:
        @wraps(func)
        async def wrapper(*args: P.args, **kwargs: P.kwargs) -> Any:
            start_time = time.time()
            operation_name = f"{operation.name.lower()}_{func.__name__}"

            logger.debug(f"Starting async {operation_name}")

            try:
                result = await func(*args, **kwargs)
                duration = time.time() - start_time

                logger.info(
                    f"Completed async {operation_name}",
                    extra={
                        "operation": operation.name,
                        "duration": duration,
                        "function": func.__name__,
                        "success": getattr(result, "success", True),
                        "async": True,
                    },
                )

                # Add performance info to result if it's a GitResult
                if hasattr(result, "duration") and result.duration is None:
                    result.duration = duration
                if hasattr(result, "operation") and result.operation is None:
                    result.operation = operation

                return result

            except Exception as e:
                duration = time.time() - start_time
                logger.error(
                    f"Failed async {operation_name}",
                    extra={
                        "operation": operation.name,
                        "duration": duration,
                        "function": func.__name__,
                        "error": str(e),
                        "async": True,
                    },
                )
                raise

        return wrapper  # type: ignore

    return decorator


def validate_repository(func: F) -> F:
    """
    Enhanced decorator to validate that a repository exists before executing a function.

    Args:
        func: The function to wrap.

    Returns:
        Callable: The wrapped function.

    Raises:
        GitRepositoryNotFound: If the repository directory doesn't exist or isn't a Git repository.
        ValueError: If no repository directory is specified.
    """

    @wraps(func)
    def wrapper(*args, **kwargs) -> Any:
        # Extract repository directory from various sources
        repo_dir = None

        # Check if first argument has repo_dir attribute (self parameter)
        if args and hasattr(args[0], "repo_dir"):
            repo_dir = args[0].repo_dir
        # Check if first argument is a path (for standalone functions)
        elif args and isinstance(args[0], (str, Path)):
            repo_dir = args[0]
        # Check kwargs
        elif "repo_dir" in kwargs:
            repo_dir = kwargs["repo_dir"]
        elif "repository_path" in kwargs:
            repo_dir = kwargs["repository_path"]

        if repo_dir is None:
            raise ValueError(
                f"Repository directory not specified for function '{func.__name__}'. "
                "Provide repo_dir parameter or use on an object with repo_dir attribute."
            )

        repo_path = ensure_path(repo_dir)
        if repo_path is None:
            raise ValueError("Repository path cannot be None")

        # Validate repository exists
        if not repo_path.exists():
            context = create_git_error_context(
                working_dir=Path.cwd(), repo_path=repo_path, function_name=func.__name__
            )
            raise GitRepositoryNotFound(
                f"Directory {repo_path} does not exist",
                repository_path=repo_path,
                context=context,
            )

        # Special case: clone operations don't need existing .git
        if func.__name__ not in [
            "clone_repository",
            "clone_repository_async",
            "init_repository",
        ]:
            if not is_git_repository(repo_path):
                context = create_git_error_context(
                    working_dir=Path.cwd(),
                    repo_path=repo_path,
                    function_name=func.__name__,
                )
                raise GitRepositoryNotFound(
                    f"Directory {repo_path} is not a Git repository",
                    repository_path=repo_path,
                    context=context,
                )

        logger.debug(f"Repository validation passed for {repo_path}")
        return func(*args, **kwargs)

    return wrapper  # type: ignore


def retry_on_failure(
    max_attempts: int = 3,
    delay: float = 1.0,
    backoff_factor: float = 2.0,
    exceptions: tuple[type[Exception], ...] = (GitException,),
):
    """
    Decorator to retry Git operations on failure with exponential backoff.

    Args:
        max_attempts: Maximum number of retry attempts.
        delay: Initial delay between attempts in seconds.
        backoff_factor: Factor to multiply delay by after each failure.
        exceptions: Tuple of exception types to retry on.
    """

    def decorator(func: F) -> F:
        @wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> Any:
            last_exception = None
            current_delay = delay

            for attempt in range(max_attempts):
                try:
                    return func(*args, **kwargs)
                except exceptions as e:
                    last_exception = e

                    if attempt == max_attempts - 1:
                        logger.error(
                            f"Function {func.__name__} failed after {max_attempts} attempts",
                            extra={"final_error": str(e), "attempts": max_attempts},
                        )
                        raise

                    logger.warning(
                        f"Function {func.__name__} failed (attempt {attempt + 1}/{max_attempts}), "
                        f"retrying in {current_delay:.1f}s: {e}"
                    )

                    time.sleep(current_delay)
                    current_delay *= backoff_factor

            # This shouldn't be reached, but just in case
            if last_exception:
                raise last_exception

        return wrapper  # type: ignore

    return decorator


def async_retry_on_failure(
    max_attempts: int = 3,
    delay: float = 1.0,
    backoff_factor: float = 2.0,
    exceptions: tuple[type[Exception], ...] = (GitException,),
):
    """
    Async decorator to retry Git operations on failure with exponential backoff.

    Args:
        max_attempts: Maximum number of retry attempts.
        delay: Initial delay between attempts in seconds.
        backoff_factor: Factor to multiply delay by after each failure.
        exceptions: Tuple of exception types to retry on.
    """

    def decorator(func: AsyncF) -> AsyncF:
        @wraps(func)
        async def wrapper(*args: P.args, **kwargs: P.kwargs) -> Any:
            last_exception = None
            current_delay = delay

            for attempt in range(max_attempts):
                try:
                    return await func(*args, **kwargs)
                except exceptions as e:
                    last_exception = e

                    if attempt == max_attempts - 1:
                        logger.error(
                            f"Async function {func.__name__} failed after {max_attempts} attempts",
                            extra={"final_error": str(e), "attempts": max_attempts},
                        )
                        raise

                    logger.warning(
                        f"Async function {func.__name__} failed (attempt {attempt + 1}/{max_attempts}), "
                        f"retrying in {current_delay:.1f}s: {e}"
                    )

                    await asyncio.sleep(current_delay)
                    current_delay *= backoff_factor

            # This shouldn't be reached, but just in case
            if last_exception:
                raise last_exception

        return wrapper  # type: ignore

    return decorator


@lru_cache(maxsize=64)
def validate_git_reference(ref: str) -> bool:
    """
    Validate a Git reference (branch, tag, commit) name with caching.

    Args:
        ref: The reference name to validate.

    Returns:
        bool: True if the reference name is valid.
    """
    if not ref or not isinstance(ref, str):
        return False

    # Git reference name rules (simplified)
    invalid_patterns = [
        r"\.\.",
        r"@{",
        r"^\.",  # No .. or @{ or starting with .
        r"\.$",
        r"/$",
        r"\.lock$",  # No ending with . or / or .lock
        r"[\x00-\x1f\x7f~^:?*\[]",  # No control chars, ~, ^, :, ?, *, [
        r"\s",  # No whitespace
    ]

    return not any(re.search(pattern, ref) for pattern in invalid_patterns)


def sanitize_commit_message(message: str, max_length: int = 72) -> str:
    """
    Sanitize and format a commit message according to Git best practices.

    Args:
        message: The raw commit message.
        max_length: Maximum length for the first line.

    Returns:
        str: Sanitized commit message.
    """
    if not message or not message.strip():
        return "Empty commit message"

    lines = message.strip().split("\n")

    # Sanitize first line (subject)
    subject = lines[0].strip()
    if len(subject) > max_length:
        subject = subject[: max_length - 3] + "..."

    # Remove leading/trailing whitespace from other lines
    body_lines = [line.rstrip() for line in lines[1:] if line.strip()]

    # Reconstruct message
    if body_lines:
        return subject + "\n\n" + "\n".join(body_lines)
    else:
        return subject


def get_git_version() -> Optional[str]:
    """
    Get the Git version string with caching.

    Returns:
        Optional[str]: Git version string or None if Git is not available.
    """
    import subprocess

    try:
        result = subprocess.run(
            ["git", "--version"], capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except (subprocess.TimeoutExpired, FileNotFoundError, Exception):
        pass

    return None


# Cache the Git version
_git_version_cached = lru_cache(maxsize=1)(get_git_version)


# Export all utilities
__all__ = [
    # Context managers
    "change_directory",
    "async_change_directory",
    # Path utilities
    "ensure_path",
    "is_git_repository",
    # Decorators
    "validate_repository",
    "performance_monitor",
    "async_performance_monitor",
    "retry_on_failure",
    "async_retry_on_failure",
    # Validation utilities
    "validate_git_reference",
    "sanitize_commit_message",
    # System utilities
    "get_git_version",
    # Protocols
    "GitRepositoryProtocol",
]
