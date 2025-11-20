"""
PyBind11 adapter for Git utilities.

This module provides simplified interfaces for C++ bindings via pybind11.
"""

from loguru import logger
from .git_utils import GitUtils


class GitUtilsPyBindAdapter:
    """
    Adapter class to expose GitUtils functionality to C++ via pybind11.

    This class simplifies the interface and handles conversions between Python and C++.
    """

    @staticmethod
    def clone_repository(repo_url: str, clone_dir: str) -> bool:
        """Simplified clone operation for C++ binding."""
        logger.info(
            f"C++ binding: Cloning repository {repo_url} to {clone_dir}")
        git = GitUtils()
        result = git.clone_repository(repo_url, clone_dir)
        return result.success

    @staticmethod
    def pull_latest_changes(repo_dir: str) -> bool:
        """Simplified pull operation for C++ binding."""
        logger.info(f"C++ binding: Pulling latest changes in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.pull_latest_changes()
            return result.success
        except Exception as e:
            logger.exception(f"Error in pull_latest_changes: {e}")
            return False

    @staticmethod
    def add_and_commit(repo_dir: str, message: str) -> bool:
        """Combined add and commit operation for C++ binding."""
        logger.info(
            f"C++ binding: Adding and committing changes in {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            add_result = git.add_changes()
            if not add_result.success:
                logger.warning("Failed to add changes")
                return False
            commit_result = git.commit_changes(message)
            return commit_result.success
        except Exception as e:
            logger.exception(f"Error in add_and_commit: {e}")
            return False

    @staticmethod
    def push_changes(repo_dir: str) -> bool:
        """Simplified push operation for C++ binding."""
        logger.info(f"C++ binding: Pushing changes from {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.push_changes()
            return result.success
        except Exception as e:
            logger.exception(f"Error in push_changes: {e}")
            return False

    @staticmethod
    def get_repository_status(repo_dir: str) -> dict:
        """Get repository status for C++ binding."""
        logger.info(f"C++ binding: Getting status of repository {repo_dir}")
        git = GitUtils(repo_dir)
        try:
            result = git.view_status(porcelain=True)
            status = {
                "success": result.success,
                "is_clean": result.success and not result.output.strip(),
                "output": result.output
            }
            return status
        except Exception as e:
            logger.exception(f"Error in get_repository_status: {e}")
            return {
                "success": False,
                "is_clean": False,
                "output": str(e)
            }
