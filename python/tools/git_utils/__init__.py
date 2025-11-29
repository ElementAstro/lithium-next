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
    GitException,
    GitCommandError,
    GitRepositoryNotFound,
    GitBranchError,
    GitMergeConflict,
)
from .models import GitResult, GitOutputFormat
from .utils import change_directory, ensure_path, validate_repository
from .git_utils import GitUtils
from .pybind_adapter import GitUtilsPyBindAdapter

__version__ = "2.0.0"
__author__ = "Max Qian"
__license__ = "GPL-3.0-or-later"

__all__ = [
    "GitUtils",
    "GitUtilsPyBindAdapter",
    "GitException",
    "GitCommandError",
    "GitRepositoryNotFound",
    "GitBranchError",
    "GitMergeConflict",
    "GitResult",
    "GitOutputFormat",
    "change_directory",
    "ensure_path",
    "validate_repository",
    "get_tool_info",
]


def get_tool_info() -> dict:
    """
    Return metadata about this tool for discovery by PythonWrapper.

    This function follows the Lithium-Next Python tool discovery convention,
    allowing the C++ PythonWrapper to introspect and catalog this module's
    capabilities.

    Returns:
        Dict containing tool metadata including name, version, description,
        available functions, requirements, and platform compatibility.
    """
    return {
        "name": "git_utils",
        "version": __version__,
        "description": "Comprehensive Git repository management utilities",
        "author": __author__,
        "license": __license__,
        "supported": True,  # Git is cross-platform
        "platform": ["windows", "linux", "macos"],
        "functions": [
            # Repository operations
            "clone_repository",
            "pull_latest_changes",
            "fetch_changes",
            "push_changes",
            # Change management
            "add_changes",
            "commit_changes",
            "reset_changes",
            "stash_changes",
            "stash_pop",
            # Branch operations
            "create_branch",
            "switch_branch",
            "merge_branch",
            "list_branches",
            "delete_branch",
            # Tag operations
            "create_tag",
            "delete_tag",
            "list_tags",
            # Remote operations
            "add_remote",
            "remove_remote",
            "list_remotes",
            # Inspection
            "view_status",
            "view_log",
            "view_diff",
            "get_current_branch",
            # Configuration
            "set_user_info",
        ],
        "requirements": ["loguru"],
        "capabilities": [
            "async_operations",
            "repository_clone",
            "branch_management",
            "merge_operations",
            "stash_management",
            "tag_management",
            "remote_management",
        ],
        "classes": {
            "GitUtils": "Main Git operations interface",
            "GitUtilsPyBindAdapter": "Simplified pybind11 interface",
        },
    }
