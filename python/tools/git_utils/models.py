"""Data models for git utilities."""

from dataclasses import dataclass
from enum import Enum


@dataclass
class GitResult:
    """Class to represent the result of a Git operation."""

    success: bool
    message: str
    output: str = ""
    error: str = ""
    return_code: int = 0

    def __bool__(self) -> bool:
        """Return whether the operation was successful."""
        return self.success


class GitOutputFormat(Enum):
    """Output format options for Git commands."""

    DEFAULT = "default"
    JSON = "json"
    PORCELAIN = "porcelain"
