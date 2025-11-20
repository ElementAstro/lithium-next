"""
Enhanced Git Utility Functions

This module provides a comprehensive set of utility functions to interact with Git repositories.
It supports both command-line usage and embedding via pybind11 for C++ applications.

Features:
- Repository operations: clone, pull, fetch, push
- Branch management: create, switch, merge, list, delete
- Change management: add, commit, reset, stash
- Tag operations: create, delete, list
- Remote repository management: add, remove, list, set-url
- Repository information: status, log, diff
- Configuration: user info, settings

Author:
    Max Qian <lightapt.com>

License:
    GPL-3.0-or-later

Version:
    2.0.0
"""

from .exceptions import (
    GitException, GitCommandError, GitRepositoryNotFound,
    GitBranchError, GitMergeConflict
)
from .models import GitResult, GitOutputFormat
from .utils import change_directory, ensure_path, validate_repository
from .git_utils import GitUtils
from .pybind_adapter import GitUtilsPyBindAdapter

__version__ = "2.0.0"
__all__ = [
    'GitUtils',
    'GitUtilsPyBindAdapter',
    'GitException',
    'GitCommandError',
    'GitRepositoryNotFound',
    'GitBranchError',
    'GitMergeConflict',
    'GitResult',
    'GitOutputFormat',
    'change_directory',
    'ensure_path',
    'validate_repository'
]
