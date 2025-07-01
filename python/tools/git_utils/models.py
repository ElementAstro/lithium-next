"""Data models for git utilities."""

from dataclasses import dataclass, field
from enum import Enum
from typing import List, Optional, Any

@dataclass
class CommitInfo:
    """Represents information about a single Git commit."""
    sha: str
    author: str
    date: str
    message: str

@dataclass
class FileStatus:
    """Represents the status of a single file in the repository."""
    path: str
    x_status: str
    y_status: str

    @property
    def description(self) -> str:
        """Provides a human-readable description of the file status."""
        status_map = {
            ' ': 'unmodified',
            'M': 'modified',
            'A': 'added',
            'D': 'deleted',
            'R': 'renamed',
            'C': 'copied',
            'U': 'unmerged',
            '?': 'untracked',
            '!': 'ignored'
        }
        x_desc = status_map.get(self.x_status, 'unknown')
        y_desc = status_map.get(self.y_status, 'unknown')
        if self.x_status == self.y_status and self.x_status != ' ':
            return f"{x_desc}"
        return f"index: {x_desc}, worktree: {y_desc}"

@dataclass
class AheadBehindInfo:
    """Represents the ahead/behind status of a branch."""
    ahead: int
    behind: int

@dataclass
class StatusInfo:
    """Represents the overall status of the repository."""
    branch: str
    is_clean: bool
    ahead_behind: Optional[AheadBehindInfo]
    files: List[FileStatus] = field(default_factory=list)

@dataclass
class GitResult:
    """Class to represent the result of a Git operation."""
    success: bool
    message: str
    output: str = ""
    error: str = ""
    return_code: int = 0
    data: Optional[Any] = None  # For structured data

    def __bool__(self) -> bool:
        """Return whether the operation was successful."""
        return self.success

class GitOutputFormat(Enum):
    """Output format options for Git commands."""
    DEFAULT = "default"
    JSON = "json"
    PORCELAIN = "porcelain"