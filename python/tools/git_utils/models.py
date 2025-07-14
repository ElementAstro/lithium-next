#!/usr/bin/env python3
"""
Enhanced data models for Git utilities with modern Python features.
Provides type-safe, high-performance data structures for Git operations.
"""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from enum import Enum, IntEnum, auto
from pathlib import Path
from typing import List, Optional, Any, Dict, Union, Literal, TypedDict, NewType
from functools import cached_property
from datetime import datetime


# Type aliases for better type safety
CommitSHA = NewType("CommitSHA", str)
BranchName = NewType("BranchName", str)
TagName = NewType("TagName", str)
RemoteName = NewType("RemoteName", str)
FilePath = NewType("FilePath", str)
CommitMessage = NewType("CommitMessage", str)


class GitStatusCode(Enum):
    """Enumeration of Git file status codes."""

    UNMODIFIED = " "
    MODIFIED = "M"
    ADDED = "A"
    DELETED = "D"
    RENAMED = "R"
    COPIED = "C"
    UNMERGED = "U"
    UNTRACKED = "?"
    IGNORED = "!"
    TYPE_CHANGED = "T"

    @property
    def description(self) -> str:
        """Human-readable description of the status."""
        descriptions = {
            self.UNMODIFIED: "unmodified",
            self.MODIFIED: "modified",
            self.ADDED: "added",
            self.DELETED: "deleted",
            self.RENAMED: "renamed",
            self.COPIED: "copied",
            self.UNMERGED: "unmerged",
            self.UNTRACKED: "untracked",
            self.IGNORED: "ignored",
            self.TYPE_CHANGED: "type changed",
        }
        return descriptions.get(self, "unknown")


class GitOperation(Enum):
    """Enumeration of Git operations."""

    CLONE = auto()
    PULL = auto()
    PUSH = auto()
    FETCH = auto()
    ADD = auto()
    COMMIT = auto()
    RESET = auto()
    BRANCH = auto()
    MERGE = auto()
    REBASE = auto()
    CHERRY_PICK = auto()
    STASH = auto()
    TAG = auto()
    REMOTE = auto()
    CONFIG = auto()
    DIFF = auto()
    LOG = auto()
    STATUS = auto()
    SUBMODULE = auto()


class GitOutputFormat(Enum):
    """Output format options for Git commands."""

    DEFAULT = "default"
    JSON = "json"
    PORCELAIN = "porcelain"
    ONELINE = "oneline"
    SHORT = "short"
    FULL = "full"
    RAW = "raw"


class BranchType(Enum):
    """Type of Git branch."""

    LOCAL = "local"
    REMOTE = "remote"
    TRACKING = "tracking"


class ResetMode(Enum):
    """Git reset modes."""

    SOFT = "soft"
    MIXED = "mixed"
    HARD = "hard"
    MERGE = "merge"
    KEEP = "keep"


class MergeStrategy(Enum):
    """Git merge strategies."""

    RECURSIVE = "recursive"
    RESOLVE = "resolve"
    OCTOPUS = "octopus"
    OURS = "ours"
    SUBTREE = "subtree"


@dataclass(frozen=True)
class CommitInfo:
    """Enhanced information about a Git commit with performance optimizations."""

    sha: CommitSHA
    author: str
    date: str
    message: CommitMessage
    author_email: str = ""
    committer: str = ""
    committer_email: str = ""
    committer_date: str = ""
    parents: List[CommitSHA] = field(default_factory=list)
    files_changed: int = 0
    insertions: int = 0
    deletions: int = 0

    @cached_property
    def short_sha(self) -> str:
        """Returns the short version of the commit SHA."""
        return str(self.sha)[:7]

    @cached_property
    def is_merge_commit(self) -> bool:
        """Checks if this is a merge commit."""
        return len(self.parents) > 1

    @cached_property
    def datetime_obj(self) -> Optional[datetime]:
        """Parse the date string into a datetime object."""
        try:
            # Handle various Git date formats
            for fmt in [
                "%Y-%m-%d %H:%M:%S %z",
                "%Y-%m-%d %H:%M:%S",
                "%Y-%m-%dT%H:%M:%SZ",
            ]:
                try:
                    return datetime.strptime(self.date, fmt)
                except ValueError:
                    continue
        except Exception:
            pass
        return None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "sha": str(self.sha),
            "short_sha": self.short_sha,
            "author": self.author,
            "author_email": self.author_email,
            "date": self.date,
            "message": str(self.message),
            "committer": self.committer,
            "committer_email": self.committer_email,
            "committer_date": self.committer_date,
            "parents": [str(p) for p in self.parents],
            "files_changed": self.files_changed,
            "insertions": self.insertions,
            "deletions": self.deletions,
            "is_merge_commit": self.is_merge_commit,
        }


@dataclass(frozen=True)
class FileStatus:
    """Enhanced representation of a file's Git status."""

    path: FilePath
    x_status: GitStatusCode
    y_status: GitStatusCode
    original_path: Optional[FilePath] = None  # For renames/copies
    similarity: Optional[int] = None  # Rename/copy similarity percentage

    @cached_property
    def index_status(self) -> GitStatusCode:
        """Status in the index (staging area)."""
        return self.x_status

    @cached_property
    def worktree_status(self) -> GitStatusCode:
        """Status in the working tree."""
        return self.y_status

    @cached_property
    def is_tracked(self) -> bool:
        """Whether the file is tracked by Git."""
        return self.x_status != GitStatusCode.UNTRACKED

    @cached_property
    def is_staged(self) -> bool:
        """Whether the file has staged changes."""
        return self.x_status != GitStatusCode.UNMODIFIED

    @cached_property
    def is_modified(self) -> bool:
        """Whether the file has unstaged changes."""
        return self.y_status != GitStatusCode.UNMODIFIED

    @cached_property
    def is_conflicted(self) -> bool:
        """Whether the file has merge conflicts."""
        return (
            self.x_status == GitStatusCode.UNMERGED
            or self.y_status == GitStatusCode.UNMERGED
        )

    @cached_property
    def is_renamed(self) -> bool:
        """Whether the file was renamed."""
        return (
            self.x_status == GitStatusCode.RENAMED
            or self.y_status == GitStatusCode.RENAMED
        )

    @cached_property
    def description(self) -> str:
        """Human-readable description of the file status."""
        if self.is_conflicted:
            return "conflicted"

        if self.is_renamed and self.original_path:
            return f"renamed from {self.original_path}"

        if self.x_status == self.y_status and self.x_status != GitStatusCode.UNMODIFIED:
            return self.x_status.description

        parts = []
        if self.is_staged:
            parts.append(f"index: {self.x_status.description}")
        if self.is_modified:
            parts.append(f"worktree: {self.y_status.description}")

        return ", ".join(parts) if parts else "unmodified"

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "path": str(self.path),
            "index_status": self.x_status.value,
            "worktree_status": self.y_status.value,
            "original_path": str(self.original_path) if self.original_path else None,
            "similarity": self.similarity,
            "is_tracked": self.is_tracked,
            "is_staged": self.is_staged,
            "is_modified": self.is_modified,
            "is_conflicted": self.is_conflicted,
            "is_renamed": self.is_renamed,
            "description": self.description,
        }


@dataclass(frozen=True)
class AheadBehindInfo:
    """Enhanced ahead/behind status information."""

    ahead: int
    behind: int

    @cached_property
    def is_ahead(self) -> bool:
        """Whether the branch is ahead of the remote."""
        return self.ahead > 0

    @cached_property
    def is_behind(self) -> bool:
        """Whether the branch is behind the remote."""
        return self.behind > 0

    @cached_property
    def is_diverged(self) -> bool:
        """Whether the branch has diverged from the remote."""
        return self.is_ahead and self.is_behind

    @cached_property
    def is_up_to_date(self) -> bool:
        """Whether the branch is up to date with the remote."""
        return self.ahead == 0 and self.behind == 0

    @cached_property
    def status_description(self) -> str:
        """Human-readable status description."""
        if self.is_up_to_date:
            return "up to date"
        elif self.is_diverged:
            return f"diverged (ahead {self.ahead}, behind {self.behind})"
        elif self.is_ahead:
            return f"ahead by {self.ahead} commit{'s' if self.ahead != 1 else ''}"
        elif self.is_behind:
            return f"behind by {self.behind} commit{'s' if self.behind != 1 else ''}"
        else:
            return "unknown"

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "ahead": self.ahead,
            "behind": self.behind,
            "is_ahead": self.is_ahead,
            "is_behind": self.is_behind,
            "is_diverged": self.is_diverged,
            "is_up_to_date": self.is_up_to_date,
            "status_description": self.status_description,
        }


@dataclass(frozen=True)
class BranchInfo:
    """Enhanced information about a Git branch."""

    name: BranchName
    branch_type: BranchType
    is_current: bool = False
    upstream: Optional[str] = None
    ahead_behind: Optional[AheadBehindInfo] = None
    last_commit: Optional[CommitSHA] = None
    commit_date: Optional[str] = None

    @cached_property
    def display_name(self) -> str:
        """Display name with current branch indicator."""
        prefix = "* " if self.is_current else "  "
        return f"{prefix}{self.name}"

    @cached_property
    def tracking_status(self) -> str:
        """Tracking status description."""
        if not self.upstream:
            return "no upstream"
        if not self.ahead_behind:
            return f"tracking {self.upstream}"
        return f"tracking {self.upstream} ({self.ahead_behind.status_description})"

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "name": str(self.name),
            "type": self.branch_type.value,
            "is_current": self.is_current,
            "upstream": self.upstream,
            "ahead_behind": self.ahead_behind.to_dict() if self.ahead_behind else None,
            "last_commit": str(self.last_commit) if self.last_commit else None,
            "commit_date": self.commit_date,
            "display_name": self.display_name,
            "tracking_status": self.tracking_status,
        }


@dataclass(frozen=True)
class RemoteInfo:
    """Enhanced information about a Git remote."""

    name: RemoteName
    fetch_url: str
    push_url: str

    @cached_property
    def is_same_url(self) -> bool:
        """Whether fetch and push URLs are the same."""
        return self.fetch_url == self.push_url

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "name": str(self.name),
            "fetch_url": self.fetch_url,
            "push_url": self.push_url,
            "is_same_url": self.is_same_url,
        }


@dataclass(frozen=True)
class TagInfo:
    """Enhanced information about a Git tag."""

    name: TagName
    commit: CommitSHA
    is_annotated: bool = False
    message: Optional[str] = None
    tagger: Optional[str] = None
    date: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "name": str(self.name),
            "commit": str(self.commit),
            "is_annotated": self.is_annotated,
            "message": self.message,
            "tagger": self.tagger,
            "date": self.date,
        }


@dataclass
class StatusInfo:
    """Enhanced repository status information with performance optimizations."""

    branch: BranchName
    is_clean: bool
    ahead_behind: Optional[AheadBehindInfo] = None
    files: List[FileStatus] = field(default_factory=list)
    is_bare: bool = False
    is_detached: bool = False
    upstream_branch: Optional[str] = None

    @cached_property
    def staged_files(self) -> List[FileStatus]:
        """Files with staged changes."""
        return [f for f in self.files if f.is_staged]

    @cached_property
    def modified_files(self) -> List[FileStatus]:
        """Files with unstaged changes."""
        return [f for f in self.files if f.is_modified]

    @cached_property
    def untracked_files(self) -> List[FileStatus]:
        """Untracked files."""
        return [f for f in self.files if f.x_status == GitStatusCode.UNTRACKED]

    @cached_property
    def conflicted_files(self) -> List[FileStatus]:
        """Files with merge conflicts."""
        return [f for f in self.files if f.is_conflicted]

    @cached_property
    def summary(self) -> str:
        """Summary of repository status."""
        if self.is_clean:
            return "Working tree clean"

        parts = []
        if self.staged_files:
            parts.append(f"{len(self.staged_files)} staged")
        if self.modified_files:
            parts.append(f"{len(self.modified_files)} modified")
        if self.untracked_files:
            parts.append(f"{len(self.untracked_files)} untracked")
        if self.conflicted_files:
            parts.append(f"{len(self.conflicted_files)} conflicted")

        return ", ".join(parts)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "branch": str(self.branch),
            "is_clean": self.is_clean,
            "is_bare": self.is_bare,
            "is_detached": self.is_detached,
            "upstream_branch": self.upstream_branch,
            "ahead_behind": self.ahead_behind.to_dict() if self.ahead_behind else None,
            "files": [f.to_dict() for f in self.files],
            "staged_count": len(self.staged_files),
            "modified_count": len(self.modified_files),
            "untracked_count": len(self.untracked_files),
            "conflicted_count": len(self.conflicted_files),
            "summary": self.summary,
        }


# TypedDict for structured command results
class GitCommandResult(TypedDict, total=False):
    """Type-safe dictionary for Git command results."""

    success: bool
    stdout: str
    stderr: str
    return_code: int
    duration: float
    command: List[str]
    working_directory: str
    environment: Dict[str, str]


@dataclass
class GitResult:
    """Enhanced result object for Git operations with modern features."""

    success: bool
    message: str
    output: str = ""
    error: str = ""
    return_code: int = 0
    data: Optional[Any] = None
    operation: Optional[GitOperation] = None
    duration: Optional[float] = None
    timestamp: float = field(default_factory=time.time)

    def __bool__(self) -> bool:
        """Return whether the operation was successful."""
        return self.success

    @cached_property
    def is_failure(self) -> bool:
        """Whether the operation failed."""
        return not self.success

    @cached_property
    def has_output(self) -> bool:
        """Whether the operation produced output."""
        return bool(self.output.strip())

    @cached_property
    def has_error(self) -> bool:
        """Whether the operation produced error output."""
        return bool(self.error.strip())

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "success": self.success,
            "message": self.message,
            "output": self.output,
            "error": self.error,
            "return_code": self.return_code,
            "operation": self.operation.name if self.operation else None,
            "duration": self.duration,
            "timestamp": self.timestamp,
            "has_output": self.has_output,
            "has_error": self.has_error,
            "data": self.data,
        }

    def raise_for_status(self) -> None:
        """Raise an exception if the operation failed."""
        if not self.success:
            from .exceptions import GitCommandError

            raise GitCommandError(
                command=[],
                return_code=self.return_code,
                stderr=self.error,
                stdout=self.output,
            )


# Export all models and types
__all__ = [
    # Type aliases
    "CommitSHA",
    "BranchName",
    "TagName",
    "RemoteName",
    "FilePath",
    "CommitMessage",
    # Enums
    "GitStatusCode",
    "GitOperation",
    "GitOutputFormat",
    "BranchType",
    "ResetMode",
    "MergeStrategy",
    # Data classes
    "CommitInfo",
    "FileStatus",
    "AheadBehindInfo",
    "BranchInfo",
    "RemoteInfo",
    "TagInfo",
    "StatusInfo",
    "GitResult",
    # TypedDict
    "GitCommandResult",
]
