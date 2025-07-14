#!/usr/bin/env python3
"""
Enhanced exception types for Git operations.
Provides structured error handling with context and debugging information.
"""

from __future__ import annotations

import time
from pathlib import Path
from typing import Any, Optional, List, Dict
from dataclasses import dataclass, field


@dataclass(frozen=True, slots=True)
class GitErrorContext:
    """Context information for debugging Git errors."""

    timestamp: float = field(default_factory=time.time)
    working_directory: Optional[Path] = None
    repository_path: Optional[Path] = None
    command: List[str] = field(default_factory=list)
    environment_vars: Dict[str, str] = field(default_factory=dict)
    git_version: Optional[str] = None
    additional_data: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "timestamp": self.timestamp,
            "working_directory": (
                str(self.working_directory) if self.working_directory else None
            ),
            "repository_path": (
                str(self.repository_path) if self.repository_path else None
            ),
            "command": self.command,
            "environment_vars": self.environment_vars,
            "git_version": self.git_version,
            "additional_data": self.additional_data,
        }


class GitException(Exception):
    """
    Base exception for all Git-related errors with enhanced context.

    Provides structured error information for better debugging and handling.
    """

    def __init__(
        self,
        message: str,
        *,
        error_code: Optional[str] = None,
        context: Optional[GitErrorContext] = None,
        original_error: Optional[Exception] = None,
        **extra_context: Any,
    ):
        super().__init__(message)
        self.error_code = error_code or self.__class__.__name__.upper()
        self.context = context or GitErrorContext()
        self.original_error = original_error
        self.extra_context = extra_context

        # Add extra context to the error context
        if extra_context:
            self.context.additional_data.update(extra_context)

    def to_dict(self) -> Dict[str, Any]:
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


class GitCommandError(GitException):
    """Exception raised when a Git command execution fails."""

    def __init__(
        self,
        command: List[str],
        return_code: int,
        stderr: str,
        stdout: Optional[str] = None,
        *,
        duration: Optional[float] = None,
        **kwargs: Any,
    ):
        self.command = command
        self.return_code = return_code
        self.stderr = stderr
        self.stdout = stdout
        self.duration = duration

        command_str = " ".join(command)
        enhanced_message = (
            f"Git command failed: {command_str} (Return code: {return_code})"
        )
        if stderr:
            enhanced_message += f": {stderr}"

        super().__init__(
            enhanced_message,
            error_code="GIT_COMMAND_FAILED",
            command=command,
            return_code=return_code,
            stderr=stderr,
            stdout=stdout,
            duration=duration,
            **kwargs,
        )


class GitRepositoryNotFound(GitException):
    """Exception raised when a Git repository is not found."""

    def __init__(
        self, message: str, repository_path: Optional[Path] = None, **kwargs: Any
    ):
        self.repository_path = repository_path

        super().__init__(
            message,
            error_code="GIT_REPOSITORY_NOT_FOUND",
            repository_path=str(repository_path) if repository_path else None,
            **kwargs,
        )


class GitBranchError(GitException):
    """Exception raised when branch operations fail."""

    def __init__(
        self,
        message: str,
        branch_name: Optional[str] = None,
        current_branch: Optional[str] = None,
        available_branches: Optional[List[str]] = None,
        **kwargs: Any,
    ):
        self.branch_name = branch_name
        self.current_branch = current_branch
        self.available_branches = available_branches or []

        super().__init__(
            message,
            error_code="GIT_BRANCH_ERROR",
            branch_name=branch_name,
            current_branch=current_branch,
            available_branches=available_branches,
            **kwargs,
        )


class GitMergeConflict(GitException):
    """Exception raised when merge operations result in conflicts."""

    def __init__(
        self,
        message: str,
        conflicted_files: Optional[List[str]] = None,
        merge_branch: Optional[str] = None,
        target_branch: Optional[str] = None,
        **kwargs: Any,
    ):
        self.conflicted_files = conflicted_files or []
        self.merge_branch = merge_branch
        self.target_branch = target_branch

        super().__init__(
            message,
            error_code="GIT_MERGE_CONFLICT",
            conflicted_files=conflicted_files,
            merge_branch=merge_branch,
            target_branch=target_branch,
            **kwargs,
        )


class GitRebaseConflictError(GitException):
    """Exception raised when a rebase results in conflicts."""

    def __init__(
        self,
        message: str,
        conflicted_files: Optional[List[str]] = None,
        rebase_branch: Optional[str] = None,
        current_commit: Optional[str] = None,
        **kwargs: Any,
    ):
        self.conflicted_files = conflicted_files or []
        self.rebase_branch = rebase_branch
        self.current_commit = current_commit

        super().__init__(
            message,
            error_code="GIT_REBASE_CONFLICT",
            conflicted_files=conflicted_files,
            rebase_branch=rebase_branch,
            current_commit=current_commit,
            **kwargs,
        )


class GitCherryPickError(GitException):
    """Exception raised when cherry-pick operations fail."""

    def __init__(
        self,
        message: str,
        commit_sha: Optional[str] = None,
        conflicted_files: Optional[List[str]] = None,
        **kwargs: Any,
    ):
        self.commit_sha = commit_sha
        self.conflicted_files = conflicted_files or []

        super().__init__(
            message,
            error_code="GIT_CHERRY_PICK_ERROR",
            commit_sha=commit_sha,
            conflicted_files=conflicted_files,
            **kwargs,
        )


class GitRemoteError(GitException):
    """Exception raised when remote operations fail."""

    def __init__(
        self,
        message: str,
        remote_name: Optional[str] = None,
        remote_url: Optional[str] = None,
        **kwargs: Any,
    ):
        self.remote_name = remote_name
        self.remote_url = remote_url

        super().__init__(
            message,
            error_code="GIT_REMOTE_ERROR",
            remote_name=remote_name,
            remote_url=remote_url,
            **kwargs,
        )


class GitTagError(GitException):
    """Exception raised when tag operations fail."""

    def __init__(self, message: str, tag_name: Optional[str] = None, **kwargs: Any):
        self.tag_name = tag_name

        super().__init__(
            message, error_code="GIT_TAG_ERROR", tag_name=tag_name, **kwargs
        )


class GitStashError(GitException):
    """Exception raised when stash operations fail."""

    def __init__(self, message: str, stash_id: Optional[str] = None, **kwargs: Any):
        self.stash_id = stash_id

        super().__init__(
            message, error_code="GIT_STASH_ERROR", stash_id=stash_id, **kwargs
        )


class GitConfigError(GitException):
    """Exception raised when configuration operations fail."""

    def __init__(
        self,
        message: str,
        config_key: Optional[str] = None,
        config_value: Optional[str] = None,
        **kwargs: Any,
    ):
        self.config_key = config_key
        self.config_value = config_value

        super().__init__(
            message,
            error_code="GIT_CONFIG_ERROR",
            config_key=config_key,
            config_value=config_value,
            **kwargs,
        )


def create_git_error_context(
    working_dir: Optional[Path] = None,
    repo_path: Optional[Path] = None,
    command: Optional[List[str]] = None,
    **extra: Any,
) -> GitErrorContext:
    """Create a Git error context with current system information."""
    import os
    import subprocess

    # Try to get Git version
    git_version = None
    try:
        result = subprocess.run(
            ["git", "--version"], capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            git_version = result.stdout.strip()
    except (subprocess.TimeoutExpired, FileNotFoundError, Exception):
        pass

    return GitErrorContext(
        working_directory=working_dir or Path.cwd(),
        repository_path=repo_path,
        command=command or [],
        environment_vars=dict(os.environ),
        git_version=git_version,
        additional_data=extra,
    )


# Export all exceptions
__all__ = [
    # Base exception and context
    "GitException",
    "GitErrorContext",
    "create_git_error_context",
    # Core exceptions
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
]
