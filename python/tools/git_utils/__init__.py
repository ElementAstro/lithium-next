"""
Enhanced Git Utility Functions

This module provides a comprehensive set of utility functions to interact with Git repositories.
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

Author:
    Max Qian <lightapt.com>

License:
    GPL-3.0-or-later

Version:
    3.0.0
"""

from .exceptions import (
    GitException, GitCommandError, GitRepositoryNotFound,
    GitBranchError, GitMergeConflict, GitRebaseConflictError, GitCherryPickError
)
from .models import (
    GitResult, GitOutputFormat, CommitInfo, StatusInfo, FileStatus, AheadBehindInfo
)
from .utils import change_directory, ensure_path, validate_repository
from .git_utils import GitUtils
from .pybind_adapter import GitUtilsPyBindAdapter

__version__ = "3.0.0"
__all__ = [
    'GitUtils',
    'GitUtilsPyBindAdapter',
    'GitException',
    'GitCommandError',
    'GitRepositoryNotFound',
    'GitBranchError',
    'GitMergeConflict',
    'GitRebaseConflictError',
    'GitCherryPickError',
    'GitResult',
    'GitOutputFormat',
    'CommitInfo',
    'StatusInfo',
    'FileStatus',
    'AheadBehindInfo',
    'change_directory',
    'ensure_path',
    'validate_repository'
]