#!/usr/bin/env python3
"""
Enhanced Git Utility Functions with Modern Python Features

This module provides a comprehensive set of utility functions to interact with Git repositories
using modern Python patterns, robust error handling, and performance optimizations.
It supports both command-line usage and embedding via pybind11 for C++ applications.

Features:
- Repository operations: clone, pull, fetch, push, rebase, cherry-pick, diff
- Branch management: create, switch, merge, list, delete
- Change management: add, commit, reset, stash
- Tag operations: create, delete, list
- Remote repository management: add, remove, list, set-url
- Submodule management
- Repository information: status, log, diff, ahead/behind status
- Configuration: user info, settings
- Async support for all operations
- Performance monitoring and caching
- Comprehensive error handling with context
- Type safety with modern Python features

Author:
    Enhanced by Claude Code Assistant
    Original by Max Qian <lightapt.com>

License:
    GPL-3.0-or-later

Version:
    4.0.0
"""

from __future__ import annotations
from pathlib import Path

# Core exceptions with enhanced context
from .exceptions import (
    GitException,
    GitCommandError,
    GitRepositoryNotFound,
    GitBranchError,
    GitMergeConflict,
    GitRebaseConflictError,
    GitCherryPickError,
    GitRemoteError,
    GitTagError,
    GitStashError,
    GitConfigError,
    GitErrorContext,
    create_git_error_context,
)

# Enhanced data models with modern Python features
from .models import (
    GitResult,
    GitOutputFormat,
    CommitInfo,
    StatusInfo,
    FileStatus,
    AheadBehindInfo,
    BranchInfo,
    RemoteInfo,
    TagInfo,
    GitStatusCode,
    GitOperation,
    BranchType,
    ResetMode,
    MergeStrategy,
    CommitSHA,
    BranchName,
    TagName,
    RemoteName,
    FilePath,
    CommitMessage,
    GitCommandResult,
)

# Enhanced utilities with performance optimizations
from .utils import (
    change_directory,
    async_change_directory,
    ensure_path,
    validate_repository,
    is_git_repository,
    performance_monitor,
    async_performance_monitor,
    retry_on_failure,
    async_retry_on_failure,
    validate_git_reference,
    sanitize_commit_message,
    get_git_version,
    GitRepositoryProtocol,
)

# Enhanced main Git utilities class
from .git_utils import GitUtils, GitConfig

# PyBind adapter for C++ integration
try:
    from .pybind_adapter import GitUtilsPyBindAdapter

    PYBIND_AVAILABLE = True
except ImportError:
    PYBIND_AVAILABLE = False
    GitUtilsPyBindAdapter = None

__version__ = "4.0.0"
__author__ = "Enhanced by Claude Code Assistant, Original by Max Qian"
__license__ = "GPL-3.0-or-later"

__all__ = [
    # Core classes
    "GitUtils",
    "GitConfig",
    # PyBind adapter (if available)
    "GitUtilsPyBindAdapter",
    "PYBIND_AVAILABLE",
    # Enhanced exceptions
    "GitException",
    "GitCommandError",
    "GitRepositoryNotFound",
    "GitBranchError",
    "GitMergeConflict",
    "GitRebaseConflictError",
    "GitCherryPickError",
    "GitRemoteError",
    "GitTagError",
    "GitStashError",
    "GitConfigError",
    "GitErrorContext",
    "create_git_error_context",
    # Enhanced data models
    "GitResult",
    "GitOutputFormat",
    "CommitInfo",
    "StatusInfo",
    "FileStatus",
    "AheadBehindInfo",
    "BranchInfo",
    "RemoteInfo",
    "TagInfo",
    "GitStatusCode",
    "GitOperation",
    "BranchType",
    "ResetMode",
    "MergeStrategy",
    "GitCommandResult",
    # Type aliases
    "CommitSHA",
    "BranchName",
    "TagName",
    "RemoteName",
    "FilePath",
    "CommitMessage",
    # Enhanced utilities
    "change_directory",
    "async_change_directory",
    "ensure_path",
    "validate_repository",
    "is_git_repository",
    "performance_monitor",
    "async_performance_monitor",
    "retry_on_failure",
    "async_retry_on_failure",
    "validate_git_reference",
    "sanitize_commit_message",
    "get_git_version",
    "GitRepositoryProtocol",
    # Metadata
    "__version__",
    "__author__",
    "__license__",
]


# Convenience functions for quick operations
def quick_status(repo_dir: str = ".") -> StatusInfo:
    """
    Quick status check with enhanced information.

    Args:
        repo_dir: Repository directory path.

    Returns:
        StatusInfo: Enhanced status information.
    """
    with GitUtils(repo_dir) as git:
        result = git.view_status(porcelain=True)
        return (
            result.data
            if result.data
            else StatusInfo(branch=BranchName("unknown"), is_clean=True)
        )


def quick_clone(repo_url: str, target_dir: str, **options) -> bool:
    """
    Quick repository cloning with sensible defaults.

    Args:
        repo_url: Repository URL to clone.
        target_dir: Target directory for cloning.
        **options: Additional clone options.

    Returns:
        bool: True if clone was successful.
    """
    try:
        with GitUtils() as git:
            result = git.clone_repository(repo_url, target_dir)
            return result.success
    except Exception:
        return False


async def async_quick_clone(repo_url: str, target_dir: str, **options) -> bool:
    """
    Quick asynchronous repository cloning.

    Args:
        repo_url: Repository URL to clone.
        target_dir: Target directory for cloning.
        **options: Additional clone options.

    Returns:
        bool: True if clone was successful.
    """
    try:
        async with GitUtils() as git:
            result = await git.clone_repository_async(repo_url, target_dir)
            return result.success
    except Exception:
        return False


def is_git_repo(path: str = ".") -> bool:
    """
    Quick check if a directory is a Git repository.

    Args:
        path: Directory path to check.

    Returns:
        bool: True if the directory is a Git repository.
    """
    return is_git_repository(ensure_path(path) or Path("."))
