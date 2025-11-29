"""
PyBind11 adapter for Git utilities.

This module provides simplified interfaces for C++ bindings via pybind11.
Inherits from BasePybindAdapter for standardized tool registration.
"""

from typing import Any, Dict, List

from loguru import logger

from ..common.base_adapter import (
    BasePybindAdapter,
    ToolInfo,
    FunctionInfo,
    ParameterInfo,
    ParameterType,
    pybind_method,
)
from .git_utils import GitUtils


class GitUtilsPyBindAdapter(BasePybindAdapter):
    """
    Adapter class to expose GitUtils functionality to C++ via pybind11.

    This class simplifies the interface and handles conversions between Python and C++.
    Inherits from BasePybindAdapter for automatic discovery and registration.
    """

    SUPPORTED_PLATFORMS = ["linux", "windows", "darwin"]

    @classmethod
    def get_tool_info(cls) -> ToolInfo:
        """Get comprehensive metadata about the git_utils tool."""
        return ToolInfo(
            name="git_utils",
            version="2.0.0",
            description="Comprehensive Git repository management utilities",
            author="Max Qian",
            license="GPL-3.0-or-later",
            supported=True,
            platforms=["linux", "windows", "darwin"],
            requirements=["loguru"],
            capabilities=[
                "async_operations",
                "repository_clone",
                "branch_management",
                "merge_operations",
                "stash_management",
                "tag_management",
                "remote_management",
            ],
            categories=["vcs", "git", "development"],
        )

    @staticmethod
    @pybind_method(
        description="Clone a git repository to a local directory",
        category="repository",
        tags=["clone", "network"],
    )
    def clone_repository(repo_url: str, clone_dir: str) -> Dict[str, Any]:
        """Simplified clone operation for C++ binding."""
        logger.info(f"C++ binding: Cloning repository {repo_url} to {clone_dir}")
        git = GitUtils()
        result = git.clone_repository(repo_url, clone_dir)
        return {
            "success": result.success,
            "output": result.output,
            "return_code": result.return_code,
        }

    @staticmethod
    @pybind_method(
        description="Pull latest changes from remote",
        category="sync",
        tags=["pull", "network"],
    )
    def pull_latest_changes(repo_dir: str) -> Dict[str, Any]:
        """Simplified pull operation for C++ binding."""
        logger.info(f"C++ binding: Pulling latest changes in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.pull_latest_changes()
            return {
                "success": result.success,
                "output": result.output,
                "return_code": result.return_code,
            }
        except Exception as e:
            logger.exception(f"Error in pull_latest_changes: {e}")
            return {"success": False, "error": str(e)}

    @staticmethod
    @pybind_method(
        description="Add and commit changes to the repository",
        category="changes",
        tags=["add", "commit"],
    )
    def add_and_commit(repo_dir: str, message: str) -> Dict[str, Any]:
        """Combined add and commit operation for C++ binding."""
        logger.info(f"C++ binding: Adding and committing changes in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            add_result = git.add_changes()
            if not add_result.success:
                logger.warning("Failed to add changes")
                return {"success": False, "error": "Failed to add changes"}
            commit_result = git.commit_changes(message)
            return {
                "success": commit_result.success,
                "output": commit_result.output,
                "return_code": commit_result.return_code,
            }
        except Exception as e:
            logger.exception(f"Error in add_and_commit: {e}")
            return {"success": False, "error": str(e)}

    @staticmethod
    @pybind_method(
        description="Push changes to remote repository",
        category="sync",
        tags=["push", "network"],
    )
    def push_changes(repo_dir: str) -> Dict[str, Any]:
        """Simplified push operation for C++ binding."""
        logger.info(f"C++ binding: Pushing changes from {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.push_changes()
            return {
                "success": result.success,
                "output": result.output,
                "return_code": result.return_code,
            }
        except Exception as e:
            logger.exception(f"Error in push_changes: {e}")
            return {"success": False, "error": str(e)}

    @staticmethod
    @pybind_method(
        description="Get repository status",
        category="info",
        tags=["status", "inspection"],
    )
    def get_repository_status(repo_dir: str) -> Dict[str, Any]:
        """Get repository status for C++ binding."""
        logger.info(f"C++ binding: Getting status of repository {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.view_status(porcelain=True)
            status = {
                "success": result.success,
                "is_clean": result.success and not result.output.strip(),
                "output": result.output,
            }
            return status
        except Exception as e:
            logger.exception(f"Error in get_repository_status: {e}")
            return {"success": False, "is_clean": False, "output": str(e)}

    @staticmethod
    @pybind_method(
        description="Create a new branch",
        category="branch",
        tags=["branch", "create"],
    )
    def create_branch(repo_dir: str, branch_name: str) -> Dict[str, Any]:
        """Create a new branch for C++ binding."""
        logger.info(f"C++ binding: Creating branch {branch_name} in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.create_branch(branch_name)
            return {
                "success": result.success,
                "output": result.output,
                "return_code": result.return_code,
            }
        except Exception as e:
            logger.exception(f"Error in create_branch: {e}")
            return {"success": False, "error": str(e)}

    @staticmethod
    @pybind_method(
        description="Switch to a branch",
        category="branch",
        tags=["branch", "checkout"],
    )
    def switch_branch(repo_dir: str, branch_name: str) -> Dict[str, Any]:
        """Switch to a branch for C++ binding."""
        logger.info(f"C++ binding: Switching to branch {branch_name} in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.switch_branch(branch_name)
            return {
                "success": result.success,
                "output": result.output,
                "return_code": result.return_code,
            }
        except Exception as e:
            logger.exception(f"Error in switch_branch: {e}")
            return {"success": False, "error": str(e)}

    @staticmethod
    @pybind_method(
        description="List all branches",
        category="branch",
        tags=["branch", "list"],
    )
    def list_branches(repo_dir: str, remote: bool = False) -> Dict[str, Any]:
        """List branches for C++ binding."""
        logger.info(f"C++ binding: Listing branches in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.list_branches(remote=remote)
            branches = []
            if result.success:
                branches = [
                    b.strip().lstrip("* ")
                    for b in result.output.strip().split("\n")
                    if b.strip()
                ]
            return {
                "success": result.success,
                "branches": branches,
                "count": len(branches),
            }
        except Exception as e:
            logger.exception(f"Error in list_branches: {e}")
            return {"success": False, "error": str(e), "branches": []}

    @staticmethod
    @pybind_method(
        description="Get current branch name",
        category="branch",
        tags=["branch", "current"],
    )
    def get_current_branch(repo_dir: str) -> Dict[str, Any]:
        """Get current branch name for C++ binding."""
        logger.info(f"C++ binding: Getting current branch in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.get_current_branch()
            return {
                "success": result.success,
                "branch": result.output.strip() if result.success else "",
            }
        except Exception as e:
            logger.exception(f"Error in get_current_branch: {e}")
            return {"success": False, "error": str(e), "branch": ""}

    @staticmethod
    @pybind_method(
        description="View commit log",
        category="info",
        tags=["log", "history"],
    )
    def view_log(
        repo_dir: str, max_count: int = 10, oneline: bool = True
    ) -> Dict[str, Any]:
        """View commit log for C++ binding."""
        logger.info(f"C++ binding: Viewing log in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.view_log(max_count=max_count, oneline=oneline)
            commits = []
            if result.success and result.output:
                commits = [
                    c.strip() for c in result.output.strip().split("\n") if c.strip()
                ]
            return {
                "success": result.success,
                "commits": commits,
                "count": len(commits),
            }
        except Exception as e:
            logger.exception(f"Error in view_log: {e}")
            return {"success": False, "error": str(e), "commits": []}

    @staticmethod
    @pybind_method(
        description="Fetch changes from remote without merging",
        category="sync",
        tags=["fetch", "network"],
    )
    def fetch_changes(repo_dir: str, remote: str = "origin") -> Dict[str, Any]:
        """Fetch changes from remote for C++ binding."""
        logger.info(f"C++ binding: Fetching changes from {remote} in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.fetch_changes(remote=remote)
            return {
                "success": result.success,
                "output": result.output,
                "return_code": result.return_code,
            }
        except Exception as e:
            logger.exception(f"Error in fetch_changes: {e}")
            return {"success": False, "error": str(e)}

    @staticmethod
    @pybind_method(
        description="Reset changes in the repository",
        category="changes",
        tags=["reset", "undo"],
    )
    def reset_changes(repo_dir: str, hard: bool = False) -> Dict[str, Any]:
        """Reset changes for C++ binding."""
        logger.info(f"C++ binding: Resetting changes in {repo_dir} (hard={hard})")
        git = GitUtils(repo_dir)
        try:
            result = git.reset_changes(hard=hard)
            return {
                "success": result.success,
                "output": result.output,
                "return_code": result.return_code,
            }
        except Exception as e:
            logger.exception(f"Error in reset_changes: {e}")
            return {"success": False, "error": str(e)}

    @staticmethod
    @pybind_method(
        description="Stash changes",
        category="stash",
        tags=["stash", "save"],
    )
    def stash_changes(repo_dir: str, message: str = "") -> Dict[str, Any]:
        """Stash changes for C++ binding."""
        logger.info(f"C++ binding: Stashing changes in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.stash_changes(message=message)
            return {
                "success": result.success,
                "output": result.output,
                "return_code": result.return_code,
            }
        except Exception as e:
            logger.exception(f"Error in stash_changes: {e}")
            return {"success": False, "error": str(e)}

    @staticmethod
    @pybind_method(
        description="Pop stashed changes",
        category="stash",
        tags=["stash", "restore"],
    )
    def stash_pop(repo_dir: str) -> Dict[str, Any]:
        """Pop stashed changes for C++ binding."""
        logger.info(f"C++ binding: Popping stash in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.stash_pop()
            return {
                "success": result.success,
                "output": result.output,
                "return_code": result.return_code,
            }
        except Exception as e:
            logger.exception(f"Error in stash_pop: {e}")
            return {"success": False, "error": str(e)}
