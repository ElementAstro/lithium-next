"""
Core Git utility implementation.

This module provides the main GitUtils class for interacting with Git repositories.
"""

import asyncio
import subprocess
from pathlib import Path
from typing import List, Dict, Optional, Union, Tuple, Any

from loguru import logger

from .exceptions (
    GitCommandError, GitBranchError, GitMergeConflict,
    GitRebaseConflictError, GitCherryPickError
)
from .models (
    GitResult, CommitInfo, StatusInfo, FileStatus, AheadBehindInfo
)
from .utils import change_directory, ensure_path, validate_repository


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

        logger.debug(f"Initialized GitUtils with repo_dir: {self.repo_dir}")

    def set_repo_dir(self, repo_dir: Union[str, Path]) -> None:
        """
        Set the repository directory for subsequent operations.

        Args:
            repo_dir: Path to the Git repository.
        """
        self.repo_dir = ensure_path(repo_dir)
        logger.debug(f"Repository directory set to: {self.repo_dir}")

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

        cmd_str = ' '.join(command)
        logger.debug(
            f"Running git command: {cmd_str} in {working_dir or 'current directory'}")

        try:
            result = subprocess.run(
                command,
                capture_output=capture_output,
                text=True,
                cwd=working_dir
            )

            success = result.returncode == 0
            stdout = result.stdout.strip() if capture_output else ""
            stderr = result.stderr.strip() if capture_output else ""

            if not success and check_errors:
                raise GitCommandError(
                    command, result.returncode, stderr, stdout)

            message = stdout if success else stderr
            git_result = GitResult(
                success=success,
                message=message,
                output=stdout,
                error=stderr,
                return_code=result.returncode
            )

            if not self.quiet:
                if success:
                    logger.info(f"Git command successful: {cmd_str}")
                    if stdout:
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

    # ... (rest of the methods from the original file)

    # New and enhanced methods

    @validate_repository
    def diff(self, cached: bool = False, other: Optional[str] = None) -> GitResult:
        """
        Show changes between commits, commit and working tree, etc.

        Args:
            cached: If True, shows staged changes.
            other: Commit or branch to compare against.

        Returns:
            GitResult: Result containing the diff output.
        """
        command = ["git", "diff"]
        if cached:
            command.append("--cached")
        if other:
            command.append(other)

        logger.info("Getting diff")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def rebase(self, branch: str, interactive: bool = False) -> GitResult:
        """
        Rebase the current branch onto another branch.

        Args:
            branch: The branch to rebase onto.
            interactive: If True, starts an interactive rebase.

        Returns:
            GitResult: Result of the rebase operation.
        """
        command = ["git", "rebase"]
        if interactive:
            command.append("-i")
        command.append(branch)

        logger.info(f"Rebasing current branch onto {branch}")
        with change_directory(self.repo_dir):
            result = self.run_git_command(command, check_errors=False, cwd=self.repo_dir)
            if not result.success and "CONFLICT" in result.error:
                logger.warning(f"Rebase conflicts detected")
                raise GitRebaseConflictError(f"Rebase conflicts detected: {result.error}")
            return result

    @validate_repository
    def cherry_pick(self, commit: str) -> GitResult:
        """
        Apply the changes introduced by an existing commit.

        Args:
            commit: The commit to cherry-pick.

        Returns:
            GitResult: Result of the cherry-pick operation.
        """
        command = ["git", "cherry-pick", commit]

        logger.info(f"Cherry-picking commit {commit}")
        with change_directory(self.repo_dir):
            result = self.run_git_command(command, check_errors=False, cwd=self.repo_dir)
            if not result.success:
                logger.warning(f"Cherry-pick failed")
                raise GitCherryPickError(f"Cherry-pick failed: {result.error}")
            return result

    @validate_repository
    def submodule_update(self, init: bool = True, recursive: bool = True) -> GitResult:
        """
        Update the registered submodules.

        Args:
            init: If True, initializes submodules.
            recursive: If True, updates submodules recursively.

        Returns:
            GitResult: Result of the submodule update operation.
        """
        command = ["git", "submodule", "update"]
        if init:
            command.append("--init")
        if recursive:
            command.append("--recursive")

        logger.info("Updating submodules")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def get_config(self, key: str, global_config: bool = False) -> GitResult:
        """
        Get a configuration value.

        Args:
            key: The configuration key.
            global_config: If True, gets the global config value.

        Returns:
            GitResult: Result containing the config value.
        """
        command = ["git", "config"]
        if global_config:
            command.append("--global")
        command.append(key)

        logger.info(f"Getting config value for {key}")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def set_config(self, key: str, value: str, global_config: bool = False) -> GitResult:
        """
        Set a configuration value.

        Args:
            key: The configuration key.
            value: The configuration value.
            global_config: If True, sets the global config value.

        Returns:
            GitResult: Result of the config set operation.
        """
        command = ["git", "config"]
        if global_config:
            command.append("--global")
        command.extend([key, value])

        logger.info(f"Setting config value for {key}")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def is_dirty(self) -> bool:
        """
        Check if the repository has uncommitted changes.

        Returns:
            bool: True if the repository is dirty, False otherwise.
        """
        result = self.view_status(porcelain=True)
        return bool(result.output)

    @validate_repository
    def get_ahead_behind_info(self, branch: Optional[str] = None) -> Optional[AheadBehindInfo]:
        """
        Get the number of commits ahead and behind the remote branch.

        Args:
            branch: The branch to check. Defaults to the current branch.

        Returns:
            AheadBehindInfo: Object with ahead and behind counts, or None if not applicable.
        """
        branch = branch or self.get_current_branch()
        command = ["git", "rev-list", "--left-right", "--count", f"origin/{branch}...{branch}"]
        result = self.run_git_command(command, check_errors=False)
        if result.success:
            try:
                ahead, behind = map(int, result.output.split())
                return AheadBehindInfo(ahead=ahead, behind=behind)
            except (ValueError, IndexError):
                return None
        return None

    def parse_status(self, output: str) -> List[FileStatus]:
        """
        Parse the output of 'git status --porcelain' into a list of FileStatus objects.

        Args:
            output: The porcelain status output.

        Returns:
            List[FileStatus]: A list of file statuses.
        """
        files = []
        for line in output.strip().split('\n'):
            if not line:
                continue
            x_status = line[0]
            y_status = line[1]
            path = line[3:]
            files.append(FileStatus(path=path, x_status=x_status, y_status=y_status))
        return files

    def parse_log(self, output: str) -> List[CommitInfo]:
        """
        Parse the output of 'git log' into a list of CommitInfo objects.

        Args:
            output: The log output.

        Returns:
            List[CommitInfo]: A list of commit information.
        """
        commits = []
        # This is a simple parser, a more robust one would be needed for complex logs
        # Assuming --oneline format for simplicity here, but can be extended
        for line in output.strip().split('\n'):
            if not line:
                continue
            parts = line.split(' ', 1)
            if len(parts) == 2:
                sha, message = parts
                # Author and date are not available in oneline format, would need different log format
                commits.append(CommitInfo(sha=sha, author="", date="", message=message))
        return commits

    # Async versions

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
            process = await asyncio.create_subprocess_exec(
                *command,
                stdout=asyncio.subprocess.PIPE if capture_output else None,
                stderr=asyncio.subprocess.PIPE if capture_output else None,
                cwd=working_dir
            )

            stdout_data, stderr_data = await process.communicate()

            stdout = stdout_data.decode('utf-8').strip() if stdout_data else ""
            stderr = stderr_data.decode('utf-8').strip() if stderr_data else ""

            success = process.returncode == 0

            if not success and check_errors:
                raise GitCommandError(
                    command, process.returncode, stderr, stdout)

            message = stdout if success else stderr
            git_result = GitResult(
                success=success,
                message=message,
                output=stdout,
                error=stderr,
                return_code=process.returncode if process.returncode is not None else 1
            )

            if success:
                logger.info(f"Async git command successful: {cmd_str}")
                if stdout and not self.quiet:
                    logger.debug(f"Output: {stdout}")
            else:
                logger.warning(f"Async git command failed: {cmd_str}")
                logger.warning(f"Error: {stderr}")

            return git_result

        except FileNotFoundError:
            error_msg = "Git executable not found. Is Git installed and in PATH?"
            logger.error(error_msg)
            return GitResult(success=False, message=error_msg, error=error_msg, return_code=127)

    async def clone_repository_async(self, repo_url: str, clone_dir: Union[str, Path],
                                     options: Optional[List[str]] = None) -> GitResult:
        target_dir = ensure_path(clone_dir)
        if target_dir.exists() and any(target_dir.iterdir()):
            return GitResult(success=False, message=f"Directory {target_dir} already exists and is not empty.")
        target_dir.parent.mkdir(parents=True, exist_ok=True)
        command = ["git", "clone"]
        if options:
            command.extend(options)
        command.extend([repo_url, str(target_dir)])
        result = await self.run_git_command_async(command, cwd=None)
        if result.success:
            self.set_repo_dir(target_dir)
        return result

    @validate_repository
    async def pull_latest_changes_async(self, remote: str = "origin", branch: Optional[str] = None,
                                        options: Optional[List[str]] = None) -> GitResult:
        command = ["git", "pull"]
        if options:
            command.extend(options)
        command.append(remote)
        if branch:
            command.append(branch)
        with change_directory(self.repo_dir):
            return await self.run_git_command_async(command, cwd=self.repo_dir)

    @validate_repository
    async def fetch_changes_async(self, remote: str = "origin", refspec: Optional[str] = None,
                                  all_remotes: bool = False, prune: bool = False) -> GitResult:
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
            return await self.run_git_command_async(command, cwd=self.repo_dir)

    @validate_repository
    async def push_changes_async(self, remote: str = "origin", branch: Optional[str] = None,
                                 force: bool = False, tags: bool = False) -> GitResult:
        command = ["git", "push"]
        if force:
            command.append("--force")
        if tags:
            command.append("--tags")
        command.append(remote)
        if branch:
            command.append(branch)
        with change_directory(self.repo_dir):
            return await self.run_git_command_async(command, cwd=self.repo_dir)

    # ... (original methods from git_utils.py)
    # This is a placeholder for brevity. The full file would include all original methods.
    # For this example, I will only include the new and async methods.

    # Original methods (abbreviated for this example)
    @validate_repository
    def add_changes(self, paths: Optional[Union[str, List[str]]] = None) -> GitResult:
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
        command = ["git", "commit"]
        if all_changes:
            command.append("-a")
        if amend:
            command.append("--amend")
        command.extend(["-m", message])
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def view_status(self, porcelain: bool = False) -> GitResult:
        command = ["git", "status"]
        if porcelain:
            command.append("--porcelain")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def get_current_branch(self) -> str:
        command = ["git", "rev-parse", "--abbrev-ref", "HEAD"]
        result = self.run_git_command(command)
        if not result.success:
            raise GitBranchError("Unable to determine current branch name")
        return result.output.strip()