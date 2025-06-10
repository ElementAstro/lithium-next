#!/usr/bin/env python3
# -*- coding: utf-8 -*-
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

Usage as module:
    from git_utils import GitUtils
    git = GitUtils(repo_path="/path/to/repo")
    git.clone("https://github.com/user/repo.git")
    
Usage as command:
    python git_utils.py clone https://github.com/user/repo.git ./destination

Author:
    Max Qian <lightapt.com>

License:
    GPL-3.0-or-later

Version:
    2.0.0
"""

import subprocess
import os
import sys
import argparse
import logging
from pathlib import Path
from typing import List, Dict, Optional, Union, Tuple, Any
from dataclasses import dataclass
from enum import Enum
import threading
import asyncio
from functools import wraps
from contextlib import contextmanager


# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger("git_utils")


class GitException(Exception):
    """Base exception for Git-related errors."""
    pass


class GitCommandError(GitException):
    """Raised when a Git command fails."""

    def __init__(self, command: List[str], return_code: int, stderr: str, stdout: str = ""):
        """
        Initialize a GitCommandError.

        Args:
            command: The Git command that failed.
            return_code: The exit code of the command.
            stderr: The error output of the command.
            stdout: The standard output of the command.
        """
        self.command = command
        self.return_code = return_code
        self.stderr = stderr
        self.stdout = stdout
        message = f"Git command {' '.join(command)} failed with exit code {return_code}:\n{stderr}"
        super().__init__(message)


class GitRepositoryNotFound(GitException):
    """Raised when a Git repository is not found."""
    pass


class GitBranchError(GitException):
    """Raised when a branch operation fails."""
    pass


class GitMergeConflict(GitException):
    """Raised when a merge results in conflicts."""
    pass


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


@contextmanager
def change_directory(path: Path):
    """
    Context manager for changing the current working directory.

    Args:
        path: The directory to change to.

    Yields:
        None

    Example:
        >>> with change_directory(Path("/path/to/dir")):
        ...     # Operations in the directory
        ...     pass
    """
    original_dir = Path.cwd()
    try:
        os.chdir(path)
        yield
    finally:
        os.chdir(original_dir)


def ensure_path(path: Union[str, Path]) -> Path:
    """
    Convert a string to a Path object if it isn't already.

    Args:
        path: String path or Path object.

    Returns:
        Path: A Path object representing the input path.
    """
    return path if isinstance(path, Path) else Path(path)


def validate_repository(func):
    """
    Decorator to validate that a repository exists before executing a function.

    Args:
        func: The function to wrap.

    Returns:
        Callable: The wrapped function.

    Raises:
        GitRepositoryNotFound: If the repository directory doesn't exist or isn't a Git repository.
    """
    @wraps(func)
    def wrapper(self, *args, **kwargs):
        # For static methods or functions that take repo_dir as first argument
        if isinstance(self, GitUtils):
            repo_dir = self.repo_dir
        else:
            # For standalone functions
            repo_dir = args[0] if args else kwargs.get('repo_dir')

        if repo_dir is None:
            raise ValueError("Repository directory not specified")

        repo_path = ensure_path(repo_dir)

        if not repo_path.exists():
            raise GitRepositoryNotFound(
                f"Directory {repo_path} does not exist.")

        if not (repo_path / ".git").exists() and func.__name__ != "clone_repository":
            raise GitRepositoryNotFound(
                f"Directory {repo_path} is not a Git repository.")

        return func(self, *args, **kwargs)
    return wrapper


class GitUtils:
    """
    A comprehensive utility class for Git operations.

    This class provides methods to interact with Git repositories both from
    command-line scripts and embedded Python code. It supports all common
    Git operations with enhanced error handling and configuration options.
    """

    def __init__(self, repo_dir: Optional[Union[str, Path]] = None, quiet: bool = False):
        """
        Initialize the GitUtils instance.

        Args:
            repo_dir: Path to the Git repository. Can be set later with set_repo_dir.
            quiet: If True, suppresses non-error output.
        """
        self.repo_dir = ensure_path(repo_dir) if repo_dir else None
        self.quiet = quiet
        self._config_cache = {}

    def set_repo_dir(self, repo_dir: Union[str, Path]) -> None:
        """
        Set the repository directory for subsequent operations.

        Args:
            repo_dir: Path to the Git repository.
        """
        self.repo_dir = ensure_path(repo_dir)

    def run_git_command(self, command: List[str], check_errors: bool = True,
                        capture_output: bool = True, cwd: Optional[Path] = None) -> GitResult:
        """
        Run a Git command and return its result.

        Args:
            command: The Git command and its arguments.
            check_errors: If True, raises exceptions for non-zero return codes.
            capture_output: If True, captures stdout and stderr.
            cwd: Directory to run the command in (overrides self.repo_dir).

        Returns:
            GitResult: Object containing the command's success status and output.

        Raises:
            GitCommandError: If the command fails and check_errors is True.
        """
        working_dir = cwd or self.repo_dir

        # Log the command being executed
        cmd_str = ' '.join(command)
        logger.debug(
            f"Running git command: {cmd_str} in {working_dir or 'current directory'}")

        try:
            # Execute the command
            result = subprocess.run(
                command,
                capture_output=capture_output,
                text=True,
                cwd=working_dir
            )

            success = result.returncode == 0
            stdout = result.stdout.strip() if capture_output else ""
            stderr = result.stderr.strip() if capture_output else ""

            # Handle command failure
            if not success and check_errors:
                raise GitCommandError(
                    command, result.returncode, stderr, stdout)

            # Create result object
            message = stdout if success else stderr
            git_result = GitResult(
                success=success,
                message=message,
                output=stdout,
                error=stderr,
                return_code=result.returncode
            )

            # Log result
            if not self.quiet:
                if success:
                    logger.info(f"Git command successful: {cmd_str}")
                    if stdout and not self.quiet:
                        logger.debug(f"Output: {stdout}")
                else:
                    logger.warning(f"Git command failed: {cmd_str}")
                    logger.warning(f"Error: {stderr}")

            return git_result

        except FileNotFoundError:
            error_msg = "Git executable not found. Is Git installed and in PATH?"
            logger.error(error_msg)
            return GitResult(success=False, message=error_msg, error=error_msg, return_code=127)
        except PermissionError:
            error_msg = f"Permission denied when executing Git command: {' '.join(command)}"
            logger.error(error_msg)
            return GitResult(success=False, message=error_msg, error=error_msg, return_code=126)

    # Repository operations
    def clone_repository(self, repo_url: str, clone_dir: Union[str, Path],
                         options: Optional[List[str]] = None) -> GitResult:
        """
        Clone a Git repository.

        Args:
            repo_url: URL of the repository to clone.
            clone_dir: Directory to clone the repository into.
            options: Additional Git clone options (e.g. ["--depth=1", "--branch=main"]).

        Returns:
            GitResult: Result of the clone operation.

        Example:
            >>> git = GitUtils()
            >>> result = git.clone_repository("https://github.com/user/repo.git", "./my_repo", ["--depth=1"])
            >>> if result:
            ...     print("Clone successful")
        """
        target_dir = ensure_path(clone_dir)

        if target_dir.exists() and any(target_dir.iterdir()):
            return GitResult(
                success=False,
                message=f"Directory {target_dir} already exists and is not empty.",
                error=f"Directory {target_dir} already exists and is not empty."
            )

        # Create parent directories if they don't exist
        target_dir.parent.mkdir(parents=True, exist_ok=True)

        # Build command with optional arguments
        command = ["git", "clone"]
        if options:
            command.extend(options)
        command.extend([repo_url, str(target_dir)])

        result = self.run_git_command(command, cwd=None)

        # Set the repository directory to the newly cloned repo if successful
        if result.success:
            self.set_repo_dir(target_dir)

        return result

    @validate_repository
    def pull_latest_changes(self, remote: str = "origin", branch: Optional[str] = None,
                            options: Optional[List[str]] = None) -> GitResult:
        """
        Pull the latest changes from the remote repository.

        Args:
            remote: Name of the remote repository (default: 'origin').
            branch: Branch to pull from (default: current branch).
            options: Additional Git pull options.

        Returns:
            GitResult: Result of the pull operation.
        """
        command = ["git", "pull"]
        if options:
            command.extend(options)
        command.append(remote)
        if branch:
            command.append(branch)

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def fetch_changes(self, remote: str = "origin", refspec: Optional[str] = None,
                      all_remotes: bool = False, prune: bool = False) -> GitResult:
        """
        Fetch the latest changes from the remote repository without merging.

        Args:
            remote: Name of the remote repository.
            refspec: Optional refspec to fetch.
            all_remotes: If True, fetches from all remotes.
            prune: If True, removes remote-tracking branches that no longer exist.

        Returns:
            GitResult: Result of the fetch operation.
        """
        command = ["git", "fetch"]
        if prune:
            command.append("--prune")
        if all_remotes:
            command.append("--all")
        else:
            command.append(remote)
        if refspec:
            command.append(refspec)

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def push_changes(self, remote: str = "origin", branch: Optional[str] = None,
                     force: bool = False, tags: bool = False) -> GitResult:
        """
        Push the committed changes to the remote repository.

        Args:
            remote: Name of the remote repository.
            branch: Branch to push to. If None, pushes the current branch.
            force: If True, forces the push with --force.
            tags: If True, pushes tags as well.

        Returns:
            GitResult: Result of the push operation.
        """
        command = ["git", "push"]
        if force:
            command.append("--force")
        if tags:
            command.append("--tags")
        command.append(remote)
        if branch:
            command.append(branch)

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    # Change management
    @validate_repository
    def add_changes(self, paths: Optional[Union[str, List[str]]] = None) -> GitResult:
        """
        Add changes to the staging area.

        Args:
            paths: Specific path(s) to add. If None, adds all changes.

        Returns:
            GitResult: Result of the add operation.

        Examples:
            # Add all changes
            >>> git.add_changes()

            # Add specific files
            >>> git.add_changes(["file1.py", "file2.py"])
            >>> git.add_changes("specific_folder/")
        """
        command = ["git", "add"]

        if not paths:
            command.append(".")
        elif isinstance(paths, str):
            command.append(paths)
        else:
            command.extend(paths)

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def commit_changes(self, message: str, all_changes: bool = False,
                       amend: bool = False) -> GitResult:
        """
        Commit the staged changes with a message.

        Args:
            message: Commit message.
            all_changes: If True, automatically stage all tracked files (git commit -a).
            amend: If True, amends the previous commit.

        Returns:
            GitResult: Result of the commit operation.
        """
        command = ["git", "commit"]

        if all_changes:
            command.append("-a")
        if amend:
            command.append("--amend")

        command.extend(["-m", message])

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def reset_changes(self, target: str = "HEAD", mode: str = "mixed",
                      paths: Optional[List[str]] = None) -> GitResult:
        """
        Reset the repository to a specific state.

        Args:
            target: Commit to reset to (default is HEAD).
            mode: Reset mode - 'soft', 'mixed', or 'hard'.
            paths: Specific paths to reset. If provided, the mode is ignored.

        Returns:
            GitResult: Result of the reset operation.
        """
        command = ["git", "reset"]

        if not paths:
            # If no paths, apply the mode
            if mode == "soft":
                command.append("--soft")
            elif mode == "hard":
                command.append("--hard")
            elif mode == "mixed":
                # mixed is default, so no need to add a flag
                pass
            else:
                return GitResult(
                    success=False,
                    message=f"Invalid reset mode: {mode}. Use 'soft', 'mixed', or 'hard'.",
                    error=f"Invalid reset mode: {mode}"
                )

            command.append(target)
        else:
            # If paths provided, add target and paths
            command.append(target)
            command.extend(paths)

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def stash_changes(self, message: Optional[str] = None,
                      include_untracked: bool = False) -> GitResult:
        """
        Stash the current changes.

        Args:
            message: Optional message for the stash.
            include_untracked: If True, includes untracked files.

        Returns:
            GitResult: Result of the stash operation.
        """
        command = ["git", "stash", "push"]

        if include_untracked:
            command.append("-u")
        if message:
            command.extend(["-m", message])

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def apply_stash(self, stash_id: str = "stash@{0}", pop: bool = False,
                    index: bool = False) -> GitResult:
        """
        Apply stashed changes.

        Args:
            stash_id: Identifier of the stash to apply.
            pop: If True, removes the stash from the stack after applying.
            index: If True, tries to reinstate index changes as well.

        Returns:
            GitResult: Result of the stash apply/pop operation.
        """
        command = ["git"]
        command.append("stash")
        command.append("pop" if pop else "apply")

        if index:
            command.append("--index")

        command.append(stash_id)

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def list_stashes(self) -> GitResult:
        """
        List all stashes.

        Returns:
            GitResult: Result containing the list of stashes.
        """
        command = ["git", "stash", "list"]

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    # Branch operations
    @validate_repository
    def create_branch(self, branch_name: str, start_point: Optional[str] = None,
                      checkout: bool = True) -> GitResult:
        """
        Create a new branch.

        Args:
            branch_name: Name of the new branch.
            start_point: Commit or reference to create the branch from.
            checkout: If True, switches to the new branch after creation.

        Returns:
            GitResult: Result of the branch creation.
        """
        if checkout:
            command = ["git", "checkout", "-b", branch_name]
        else:
            command = ["git", "branch", branch_name]

        if start_point:
            command.append(start_point)

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def switch_branch(self, branch_name: str, create: bool = False,
                      force: bool = False) -> GitResult:
        """
        Switch to an existing branch.

        Args:
            branch_name: Name of the branch to switch to.
            create: If True, creates the branch if it doesn't exist.
            force: If True, forces the switch even with uncommitted changes.

        Returns:
            GitResult: Result of the branch switch.
        """
        command = ["git", "checkout"]

        if create:
            command.append("-b")
        if force:
            command.append("-f")

        command.append(branch_name)

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def merge_branch(self, branch_name: str, strategy: Optional[str] = None,
                     commit_message: Optional[str] = None, no_ff: bool = False) -> GitResult:
        """
        Merge a branch into the current branch.

        Args:
            branch_name: Name of the branch to merge.
            strategy: Merge strategy to use.
            commit_message: Custom commit message for the merge.
            no_ff: If True, creates a merge commit even for fast-forward merges.

        Returns:
            GitResult: Result of the merge operation.
        """
        command = ["git", "merge"]

        if strategy:
            command.extend(["--strategy", strategy])
        if commit_message:
            command.extend(["-m", commit_message])
        if no_ff:
            command.append("--no-ff")

        command.append(branch_name)

        with change_directory(self.repo_dir):
            result = self.run_git_command(
                command, check_errors=False, cwd=self.repo_dir)

            # Check for merge conflicts
            if not result.success and "CONFLICT" in result.error:
                raise GitMergeConflict(
                    f"Merge conflicts detected: {result.error}")

            return result

    @validate_repository
    def list_branches(self, show_all: bool = False, verbose: bool = False) -> GitResult:
        """
        List all branches in the repository.

        Args:
            show_all: If True, shows both local and remote branches.
            verbose: If True, shows more details about each branch.

        Returns:
            GitResult: Result containing the list of branches.
        """
        command = ["git", "branch"]

        if show_all:
            command.append("-a")
        if verbose:
            command.append("-v")

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def delete_branch(self, branch_name: str, force: bool = False,
                      remote: Optional[str] = None) -> GitResult:
        """
        Delete a branch.

        Args:
            branch_name: Name of the branch to delete.
            force: If True, force deletion even if branch is not fully merged.
            remote: If provided, deletes the branch from the specified remote.

        Returns:
            GitResult: Result of the branch deletion.
        """
        if remote:
            command = ["git", "push", remote, "--delete", branch_name]
        else:
            command = ["git", "branch", "-D" if force else "-d", branch_name]

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def get_current_branch(self) -> str:
        """
        Get the name of the current branch.

        Returns:
            str: Name of the current branch.

        Raises:
            GitBranchError: If the branch name cannot be determined.
        """
        command = ["git", "rev-parse", "--abbrev-ref", "HEAD"]

        with change_directory(self.repo_dir):
            result = self.run_git_command(command, cwd=self.repo_dir)

        if not result.success:
            raise GitBranchError("Unable to determine current branch name")

        return result.output.strip()

    # Tag operations
    @validate_repository
    def create_tag(self, tag_name: str, commit: str = "HEAD",
                   message: Optional[str] = None, annotated: bool = True) -> GitResult:
        """
        Create a new tag.

        Args:
            tag_name: Name of the tag to create.
            commit: Commit to tag (default: HEAD).
            message: Tag message (for annotated tags).
            annotated: If True, creates an annotated tag with a message.

        Returns:
            GitResult: Result of the tag creation.
        """
        command = ["git", "tag"]

        if annotated and message:
            command.extend(["-a", tag_name, "-m", message, commit])
        else:
            command.append(tag_name)
            command.append(commit)

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def delete_tag(self, tag_name: str, remote: Optional[str] = None) -> GitResult:
        """
        Delete a tag.

        Args:
            tag_name: Name of the tag to delete.
            remote: If provided, deletes the tag from the specified remote.

        Returns:
            GitResult: Result of the tag deletion.
        """
        if remote:
            command = ["git", "push", remote, f":refs/tags/{tag_name}"]
        else:
            command = ["git", "tag", "-d", tag_name]

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    # Remote operations
    @validate_repository
    def add_remote(self, remote_name: str, remote_url: str) -> GitResult:
        """
        Add a remote repository.

        Args:
            remote_name: Name of the remote repository.
            remote_url: URL of the remote repository.

        Returns:
            GitResult: Result of the remote add operation.
        """
        command = ["git", "remote", "add", remote_name, remote_url]

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def remove_remote(self, remote_name: str) -> GitResult:
        """
        Remove a remote repository.

        Args:
            remote_name: Name of the remote repository.

        Returns:
            GitResult: Result of the remote remove operation.
        """
        command = ["git", "remote", "remove", remote_name]

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    # Repository information
    @validate_repository
    def view_status(self, porcelain: bool = False) -> GitResult:
        """
        View the current status of the repository.

        Args:
            porcelain: If True, returns machine-readable output.

        Returns:
            GitResult: Result containing the repository status.
        """
        command = ["git", "status"]

        if porcelain:
            command.append("--porcelain")

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def view_log(self, num_entries: Optional[int] = None, oneline: bool = True,
                 graph: bool = False, all_branches: bool = False) -> GitResult:
        """
        View the commit log.

        Args:
            num_entries: Number of log entries to show.
            oneline: If True, shows one line per commit.
            graph: If True, shows a graphical representation of branches.
            all_branches: If True, shows commits from all branches.

        Returns:
            GitResult: Result containing the commit log.
        """
        command = ["git", "log"]

        if oneline:
            command.append("--oneline")
        if graph:
            command.append("--graph")
        if all_branches:
            command.append("--all")
        if num_entries:
            command.append(f"-n{num_entries}")

        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    # Configuration
    @validate_repository
    def set_user_info(self, name: Optional[str] = None, email: Optional[str] = None,
                      global_config: bool = False) -> GitResult:
        """
        Set the user name and email for the repository.

        Args:
            name: User name to set.
            email: User email to set.
            global_config: If True, sets global Git config instead of repo-specific.

        Returns:
            GitResult: Result of the configuration operation.
        """
        results = []

        config_flag = "--global" if global_config else "--local"

        if name:
            command = ["git", "config", config_flag, "user.name", name]
            with change_directory(self.repo_dir):
                results.append(self.run_git_command(
                    command, cwd=self.repo_dir))

        if email:
            command = ["git", "config", config_flag, "user.email", email]
            with change_directory(self.repo_dir):
                results.append(self.run_git_command(
                    command, cwd=self.repo_dir))

        if not results:
            return GitResult(
                success=False,
                message="No name or email provided to set",
                error="No name or email provided to set"
            )

        # Return success only if all operations succeeded
        all_success = all(result.success for result in results)
        return GitResult(
            success=all_success,
            message="User info set successfully" if all_success else "Failed to set some user info",
            output="\n".join(result.output for result in results),
            error="\n".join(result.error for result in results if result.error)
        )

    # Async versions for concurrent operations
    async def run_git_command_async(self, command: List[str], check_errors: bool = True,
                                    capture_output: bool = True, cwd: Optional[Path] = None) -> GitResult:
        """
        Run a Git command asynchronously.

        Args:
            command: The Git command and its arguments.
            check_errors: If True, raises exceptions for non-zero return codes.
            capture_output: If True, captures stdout and stderr.
            cwd: Directory to run the command in.

        Returns:
            GitResult: Object containing the command's success status and output.
        """
        working_dir = str(
            cwd or self.repo_dir) if cwd or self.repo_dir else None
        cmd_str = ' '.join(command)
        logger.debug(
            f"Running async git command: {cmd_str} in {working_dir or 'current directory'}")

        try:
            # Create subprocess
            process = await asyncio.create_subprocess_exec(
                *command,
                stdout=asyncio.subprocess.PIPE if capture_output else None,
                stderr=asyncio.subprocess.PIPE if capture_output else None,
                cwd=working_dir
            )

            # Wait for completion and get output
            stdout_data, stderr_data = await process.communicate()

            stdout = stdout_data.decode('utf-8').strip() if stdout_data else ""
            stderr = stderr_data.decode('utf-8').strip() if stderr_data else ""

            success = process.returncode == 0

            # Handle command failure
            if not success and check_errors:
                raise GitCommandError(
                    command, process.returncode, stderr, stdout)

            # Create result object
            message = stdout if success else stderr
            git_result = GitResult(
                success=success,
                message=message,
                output=stdout,
                error=stderr,
                return_code=process.returncode
            )

            return git_result

        except FileNotFoundError:
            error_msg = "Git executable not found. Is Git installed and in PATH?"
            logger.error(error_msg)
            return GitResult(success=False, message=error_msg, error=error_msg, return_code=127)


# pybind11 support
class GitUtilsPyBindAdapter:
    """
    Adapter class to expose GitUtils functionality to C++ via pybind11.

    This class simplifies the interface and handles conversions between Python and C++.
    """

    @staticmethod
    def clone_repository(repo_url: str, clone_dir: str) -> bool:
        """Simplified clone operation for C++ binding."""
        git = GitUtils()
        result = git.clone_repository(repo_url, clone_dir)
        return result.success

    @staticmethod
    def pull_latest_changes(repo_dir: str) -> bool:
        """Simplified pull operation for C++ binding."""
        git = GitUtils(repo_dir)
        try:
            result = git.pull_latest_changes()
            return result.success
        except Exception:
            return False

    @staticmethod
    def add_and_commit(repo_dir: str, message: str) -> bool:
        """Combined add and commit operation for C++ binding."""
        git = GitUtils(repo_dir)
        try:
            add_result = git.add_changes()
            if not add_result.success:
                return False
            commit_result = git.commit_changes(message)
            return commit_result.success
        except Exception:
            return False

    @staticmethod
    def push_changes(repo_dir: str) -> bool:
        """Simplified push operation for C++ binding."""
        git = GitUtils(repo_dir)
        try:
            result = git.push_changes()
            return result.success
        except Exception:
            return False

    @staticmethod
    def get_repository_status(repo_dir: str) -> dict:
        """Get repository status for C++ binding."""
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
            return {
                "success": False,
                "is_clean": False,
                "output": str(e)
            }


# CLI implementation
def cli_clone_repository(args):
    git = GitUtils()
    options = []
    if hasattr(args, 'depth') and args.depth:
        options.append(f"--depth={args.depth}")
    if hasattr(args, 'branch') and args.branch:
        options.extend(["--branch", args.branch])
    return git.clone_repository(args.repo_url, args.clone_dir, options)


def cli_pull_latest_changes(args):
    git = GitUtils(args.repo_dir)
    return git.pull_latest_changes(args.remote, args.branch)


def cli_fetch_changes(args):
    git = GitUtils(args.repo_dir)
    return git.fetch_changes(args.remote, args.refspec, args.all, args.prune)


def cli_push_changes(args):
    git = GitUtils(args.repo_dir)
    return git.push_changes(args.remote, args.branch, args.force, args.tags)


def cli_add_changes(args):
    git = GitUtils(args.repo_dir)
    return git.add_changes(args.paths)


def cli_commit_changes(args):
    git = GitUtils(args.repo_dir)
    return git.commit_changes(args.message, args.all, args.amend)


def cli_reset_changes(args):
    git = GitUtils(args.repo_dir)
    return git.reset_changes(args.target, args.mode, args.paths)


def cli_create_branch(args):
    git = GitUtils(args.repo_dir)
    return git.create_branch(args.branch_name, args.start_point)


def cli_switch_branch(args):
    git = GitUtils(args.repo_dir)
    return git.switch_branch(args.branch_name, args.create, args.force)


def cli_merge_branch(args):
    git = GitUtils(args.repo_dir)
    return git.merge_branch(args.branch_name, args.strategy, args.message, args.no_ff)


def cli_list_branches(args):
    git = GitUtils(args.repo_dir)
    return git.list_branches(args.all, args.verbose)


def cli_view_status(args):
    git = GitUtils(args.repo_dir)
    return git.view_status(args.porcelain)


def cli_view_log(args):
    git = GitUtils(args.repo_dir)
    return git.view_log(args.num_entries, args.oneline, args.graph, args.all)


def cli_add_remote(args):
    git = GitUtils(args.repo_dir)
    return git.add_remote(args.remote_name, args.remote_url)


def cli_remove_remote(args):
    git = GitUtils(args.repo_dir)
    return git.remove_remote(args.remote_name)


def cli_create_tag(args):
    git = GitUtils(args.repo_dir)
    return git.create_tag(args.tag_name, args.commit, args.message, args.annotated)


def cli_delete_tag(args):
    git = GitUtils(args.repo_dir)
    return git.delete_tag(args.tag_name, args.remote)


def cli_stash_changes(args):
    git = GitUtils(args.repo_dir)
    return git.stash_changes(args.message, args.include_untracked)


def cli_apply_stash(args):
    git = GitUtils(args.repo_dir)
    return git.apply_stash(args.stash_id, args.pop, args.index)


def cli_set_user_info(args):
    git = GitUtils(args.repo_dir)
    return git.set_user_info(args.name, args.email, args.global_config)


def setup_parser():
    """
    Set up the argument parser for the command line interface.

    Returns:
        argparse.ArgumentParser: Configured argument parser.
    """
    parser = argparse.ArgumentParser(
        description="Enhanced Git Repository Management Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Clone a repository:
  git_utils.py clone https://github.com/user/repo.git ./destination
  
  # Pull latest changes:
  git_utils.py pull --repo-dir ./my_repo
  
  # Create and switch to a new branch:
  git_utils.py create-branch --repo-dir ./my_repo new-feature
  
  # Add and commit changes:
  git_utils.py add --repo-dir ./my_repo
  git_utils.py commit --repo-dir ./my_repo -m "Added new feature"
  
  # Push changes to remote:
  git_utils.py push --repo-dir ./my_repo
        """
    )

    subparsers = parser.add_subparsers(
        dest="command", help="Git command to run"
    )

    # Common argument function for repo directory
    def add_repo_dir(subparser):
        subparser.add_argument(
            "--repo-dir", "-d",
            required=True,
            help="Directory of the repository"
        )

    # Clone command
    parser_clone = subparsers.add_parser("clone", help="Clone a repository")
    parser_clone.add_argument(
        "repo_url", help="URL of the repository to clone")
    parser_clone.add_argument(
        "clone_dir", help="Directory to clone the repository into")
    parser_clone.add_argument(
        "--depth", type=int, help="Create a shallow clone with specified depth")
    parser_clone.add_argument("--branch", "-b", help="Clone a specific branch")
    parser_clone.set_defaults(func=cli_clone_repository)

    # Pull command
    parser_pull = subparsers.add_parser(
        "pull", help="Pull the latest changes from remote")
    add_repo_dir(parser_pull)
    parser_pull.add_argument("--remote", default="origin",
                             help="Remote to pull from (default: origin)")
    parser_pull.add_argument("--branch", help="Branch to pull")
    parser_pull.set_defaults(func=cli_pull_latest_changes)

    # Fetch command
    parser_fetch = subparsers.add_parser(
        "fetch", help="Fetch changes without merging")
    add_repo_dir(parser_fetch)
    parser_fetch.add_argument(
        "--remote", default="origin", help="Remote to fetch from (default: origin)")
    parser_fetch.add_argument("--refspec", help="Refspec to fetch")
    parser_fetch.add_argument(
        "--all", "-a", action="store_true", help="Fetch from all remotes")
    parser_fetch.add_argument("--prune", "-p", action="store_true",
                              help="Remove remote-tracking branches that no longer exist")
    parser_fetch.set_defaults(func=cli_fetch_changes)

    # Add command
    parser_add = subparsers.add_parser(
        "add", help="Add changes to the staging area")
    add_repo_dir(parser_add)
    parser_add.add_argument(
        "paths", nargs="*", help="Paths to add (default: all changes)")
    parser_add.set_defaults(func=cli_add_changes)

    # Commit command
    parser_commit = subparsers.add_parser(
        "commit", help="Commit staged changes")
    add_repo_dir(parser_commit)
    parser_commit.add_argument(
        "-m", "--message", required=True, help="Commit message")
    parser_commit.add_argument(
        "-a", "--all", action="store_true", help="Automatically stage all tracked files")
    parser_commit.add_argument(
        "--amend", action="store_true", help="Amend the previous commit")
    parser_commit.set_defaults(func=cli_commit_changes)

    # Push command
    parser_push = subparsers.add_parser("push", help="Push changes to remote")
    add_repo_dir(parser_push)
    parser_push.add_argument("--remote", default="origin",
                             help="Remote to push to (default: origin)")
    parser_push.add_argument("--branch", help="Branch to push")
    parser_push.add_argument(
        "-f", "--force", action="store_true", help="Force push")
    parser_push.add_argument(
        "--tags", action="store_true", help="Push tags as well")
    parser_push.set_defaults(func=cli_push_changes)

    # Branch commands
    parser_create_branch = subparsers.add_parser(
        "create-branch", help="Create a new branch")
    add_repo_dir(parser_create_branch)
    parser_create_branch.add_argument(
        "branch_name", help="Name of the new branch")
    parser_create_branch.add_argument(
        "--start-point", help="Commit to start the branch from")
    parser_create_branch.set_defaults(func=cli_create_branch)

    parser_switch_branch = subparsers.add_parser(
        "switch-branch", help="Switch to an existing branch")
    add_repo_dir(parser_switch_branch)
    parser_switch_branch.add_argument(
        "branch_name", help="Name of the branch to switch to")
    parser_switch_branch.add_argument("-c", "--create", action="store_true",
                                      help="Create the branch if it doesn't exist")
    parser_switch_branch.add_argument("-f", "--force", action="store_true",
                                      help="Force switch even with uncommitted changes")
    parser_switch_branch.set_defaults(func=cli_switch_branch)

    parser_merge_branch = subparsers.add_parser(
        "merge-branch", help="Merge a branch into the current branch")
    add_repo_dir(parser_merge_branch)
    parser_merge_branch.add_argument(
        "branch_name", help="Name of the branch to merge")
    parser_merge_branch.add_argument("--strategy",
                                     choices=["recursive", "resolve",
                                              "octopus", "ours", "subtree"],
                                     help="Merge strategy to use")
    parser_merge_branch.add_argument(
        "-m", "--message", help="Custom commit message for the merge")
    parser_merge_branch.add_argument("--no-ff", action="store_true",
                                     help="Create a merge commit even for fast-forward merges")
    parser_merge_branch.set_defaults(func=cli_merge_branch)

    parser_list_branches = subparsers.add_parser(
        "list-branches", help="List all branches")
    add_repo_dir(parser_list_branches)
    parser_list_branches.add_argument("-a", "--all", action="store_true",
                                      help="Show both local and remote branches")
    parser_list_branches.add_argument("-v", "--verbose", action="store_true",
                                      help="Show more details about each branch")
    parser_list_branches.set_defaults(func=cli_list_branches)

    # Reset command
    parser_reset = subparsers.add_parser(
        "reset", help="Reset the repository to a specific state")
    add_repo_dir(parser_reset)
    parser_reset.add_argument(
        "--target", default="HEAD", help="Commit to reset to (default: HEAD)")
    parser_reset.add_argument("--mode", choices=["soft", "mixed", "hard"], default="mixed",
                              help="Reset mode (default: mixed)")
    parser_reset.add_argument(
        "paths", nargs="*", help="Paths to reset (if specified, mode is ignored)")
    parser_reset.set_defaults(func=cli_reset_changes)

    # Stash commands
    parser_stash = subparsers.add_parser("stash", help="Stash changes")
    add_repo_dir(parser_stash)
    parser_stash.add_argument("-m", "--message", help="Stash message")
    parser_stash.add_argument("-u", "--include-untracked", action="store_true",
                              help="Include untracked files")
    parser_stash.set_defaults(func=cli_stash_changes)

    parser_apply_stash = subparsers.add_parser(
        "apply-stash", help="Apply stashed changes")
    add_repo_dir(parser_apply_stash)
    parser_apply_stash.add_argument("--stash-id", default="stash@{0}",
                                    help="Stash to apply (default: stash@{0})")
    parser_apply_stash.add_argument("-p", "--pop", action="store_true",
                                    help="Remove the stash after applying")
    parser_apply_stash.add_argument("--index", action="store_true",
                                    help="Reinstate index changes as well")
    parser_apply_stash.set_defaults(func=cli_apply_stash)

    # Status command
    parser_status = subparsers.add_parser(
        "status", help="View the current status")
    add_repo_dir(parser_status)
    parser_status.add_argument("-p", "--porcelain", action="store_true",
                               help="Machine-readable output")
    parser_status.set_defaults(func=cli_view_status)

    # Log command
    parser_log = subparsers.add_parser("log", help="View commit history")
    add_repo_dir(parser_log)
    parser_log.add_argument("-n", "--num-entries",
                            type=int, help="Number of commits to display")
    parser_log.add_argument("--oneline", action="store_true", default=True,
                            help="One line per commit")
    parser_log.add_argument(
        "--graph", action="store_true", help="Show branch graph")
    parser_log.add_argument(
        "-a", "--all", action="store_true", help="Show commits from all branches")
    parser_log.set_defaults(func=cli_view_log)

    # Remote commands
    parser_add_remote = subparsers.add_parser(
        "add-remote", help="Add a remote repository")
    add_repo_dir(parser_add_remote)
    parser_add_remote.add_argument("remote_name", help="Name of the remote")
    parser_add_remote.add_argument("remote_url", help="URL of the remote")
    parser_add_remote.set_defaults(func=cli_add_remote)

    parser_remove_remote = subparsers.add_parser(
        "remove-remote", help="Remove a remote repository")
    add_repo_dir(parser_remove_remote)
    parser_remove_remote.add_argument(
        "remote_name", help="Name of the remote to remove")
    parser_remove_remote.set_defaults(func=cli_remove_remote)

    # Tag commands
    parser_create_tag = subparsers.add_parser(
        "create-tag", help="Create a tag")
    add_repo_dir(parser_create_tag)
    parser_create_tag.add_argument("tag_name", help="Name of the tag")
    parser_create_tag.add_argument(
        "--commit", default="HEAD", help="Commit to tag (default: HEAD)")
    parser_create_tag.add_argument("-m", "--message", help="Tag message")
    parser_create_tag.add_argument("-a", "--annotated", action="store_true", default=True,
                                   help="Create an annotated tag")
    parser_create_tag.set_defaults(func=cli_create_tag)

    parser_delete_tag = subparsers.add_parser(
        "delete-tag", help="Delete a tag")
    add_repo_dir(parser_delete_tag)
    parser_delete_tag.add_argument(
        "tag_name", help="Name of the tag to delete")
    parser_delete_tag.add_argument(
        "--remote", help="Delete from the specified remote")
    parser_delete_tag.set_defaults(func=cli_delete_tag)

    # Config command
    parser_config = subparsers.add_parser(
        "set-user-info", help="Set user name and email")
    add_repo_dir(parser_config)
    parser_config.add_argument("--name", help="User name")
    parser_config.add_argument("--email", help="User email")
    parser_config.add_argument("--global", dest="global_config", action="store_true",
                               help="Set global Git config")
    parser_config.set_defaults(func=cli_set_user_info)

    return parser


def main():
    """
    Main function for command-line execution.

    This function parses command-line arguments and executes the corresponding
    Git operation, printing the result to the console.
    """
    # Set up argument parser
    parser = setup_parser()

    # Parse arguments
    args = parser.parse_args()

    try:
        # Execute the selected function
        if hasattr(args, 'func'):
            result = args.func(args)

            # Print result if it's a GitResult object
            if isinstance(result, GitResult):
                if result.success:
                    if result.output:
                        print(result.output)
                    else:
                        print(result.message)
                    sys.exit(0)
                else:
                    print(
                        f"Error: {result.error if result.error else result.message}", file=sys.stderr)
                    sys.exit(1)
        else:
            parser.print_help()

    except GitCommandError as e:
        print(f"Git command error: {e}", file=sys.stderr)
        sys.exit(1)
    except GitRepositoryNotFound as e:
        print(f"Git repository error: {e}", file=sys.stderr)
        sys.exit(1)
    except GitBranchError as e:
        print(f"Git branch error: {e}", file=sys.stderr)
        sys.exit(1)
    except GitMergeConflict as e:
        print(f"Git merge conflict: {e}", file=sys.stderr)
        sys.exit(1)
    except GitException as e:
        print(f"Git error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
