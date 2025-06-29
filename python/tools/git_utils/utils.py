"""Utility functions for git operations."""

import os
from contextlib import contextmanager
from functools import wraps
from pathlib import Path
from typing import Union, Callable

from loguru import logger

from .exceptions import GitRepositoryNotFound


@contextmanager
def change_directory(path: Path):
    """
    Context manager for changing the current working directory.

    Args:
        path: The directory to change to.

    Yields:
        None

    Example:
        >>> with change_directory(Path("/path/to/dir")):
        ...     # Operations in the directory
        ...     pass
    """
    original_dir = Path.cwd()
    try:
        os.chdir(path)
        yield
    finally:
        os.chdir(original_dir)


def ensure_path(path: Union[str, Path]) -> Path:
    """
    Convert a string to a Path object if it isn't already.

    Args:
        path: String path or Path object.

    Returns:
        Path: A Path object representing the input path.
    """
    return path if isinstance(path, Path) else Path(path)


def validate_repository(func: Callable) -> Callable:
    """
    Decorator to validate that a repository exists before executing a function.

    Args:
        func: The function to wrap.

    Returns:
        Callable: The wrapped function.

    Raises:
        GitRepositoryNotFound: If the repository directory doesn't exist or isn't a Git repository.
    """

    @wraps(func)
    def wrapper(self, *args, **kwargs):
        # For static methods or functions that take repo_dir as first argument
        if hasattr(self, "repo_dir"):
            repo_dir = self.repo_dir
        else:
            # For standalone functions
            repo_dir = args[0] if args else kwargs.get("repo_dir")

        if repo_dir is None:
            raise ValueError("Repository directory not specified")

        repo_path = ensure_path(repo_dir)

        if not repo_path.exists():
            raise GitRepositoryNotFound(f"Directory {repo_path} does not exist.")

        if not (repo_path / ".git").exists() and func.__name__ != "clone_repository":
            raise GitRepositoryNotFound(
                f"Directory {repo_path} is not a Git repository."
            )

        return func(self, *args, **kwargs)

    return wrapper
