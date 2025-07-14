#!/usr/bin/env python3
"""
Enhanced core Git utility implementation with modern Python features.
Provides high-performance, type-safe Git operations with robust error handling.
"""

from __future__ import annotations

import asyncio
import subprocess
import re
import time
import shutil
from pathlib import Path
from typing import (
    List,
    Dict,
    Optional,
    Union,
    Tuple,
    Any,
    AsyncIterator,
    Literal,
    overload,
    TypeGuard,
    Self,
)
from functools import lru_cache, cached_property
from contextlib import asynccontextmanager
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field

from loguru import logger

from .exceptions import (
    GitCommandError,
    GitBranchError,
    GitMergeConflict,
    GitRebaseConflictError,
    GitCherryPickError,
    GitRemoteError,
    GitTagError,
    GitStashError,
    GitConfigError,
    GitRepositoryNotFound,
    create_git_error_context,
)
from .models import (
    GitResult,
    CommitInfo,
    StatusInfo,
    FileStatus,
    AheadBehindInfo,
    BranchInfo,
    RemoteInfo,
    TagInfo,
    GitOperation,
    GitStatusCode,
    BranchType,
    ResetMode,
    MergeStrategy,
    GitOutputFormat,
    CommitSHA,
    BranchName,
    TagName,
    RemoteName,
    FilePath,
    CommitMessage,
)
from .utils import (
    change_directory,
    ensure_path,
    validate_repository,
    performance_monitor,
    async_performance_monitor,
    retry_on_failure,
    async_retry_on_failure,
    validate_git_reference,
    sanitize_commit_message,
)


@dataclass
class GitConfig:
    """Enhanced configuration for Git operations."""

    timeout: int = 300
    retry_attempts: int = 3
    retry_delay: float = 1.0
    parallel_operations: int = 4
    cache_enabled: bool = True
    cache_ttl: int = 300
    default_remote: str = "origin"
    auto_stash: bool = False
    sign_commits: bool = False


class GitUtils:
    """
    Enhanced comprehensive utility class for Git operations.

    Features modern Python patterns, robust error handling, performance optimizations,
    and comprehensive Git functionality with both sync and async support.
    """

    def __init__(
        self,
        repo_dir: Optional[Union[str, Path]] = None,
        quiet: bool = False,
        config: Optional[GitConfig] = None,
    ):
        """
        Initialize the GitUtils instance with enhanced configuration.

        Args:
            repo_dir: Path to the Git repository. Can be set later with set_repo_dir.
            quiet: If True, suppresses non-error output.
            config: Configuration object for Git operations.
        """
        self.repo_dir = ensure_path(repo_dir) if repo_dir else None
        self.quiet = quiet
        self.config = config or GitConfig()

        # Performance optimizations
        self._config_cache: Dict[str, str] = {}
        self._branch_cache: Dict[str, List[BranchInfo]] = {}
        self._cache_timestamp: float = 0

        # Async support
        self._executor = ThreadPoolExecutor(max_workers=self.config.parallel_operations)

        logger.debug(
            "Initialized enhanced GitUtils",
            extra={
                "repo_dir": str(self.repo_dir) if self.repo_dir else None,
                "quiet": self.quiet,
                "config": {
                    "timeout": self.config.timeout,
                    "retry_attempts": self.config.retry_attempts,
                    "parallel_operations": self.config.parallel_operations,
                },
            },
        )

    def __enter__(self) -> Self:
        """Context manager entry."""
        return self

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """Context manager exit with cleanup."""
        self.cleanup()

    async def __aenter__(self) -> Self:
        """Async context manager entry."""
        return self

    async def __aexit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """Async context manager exit with cleanup."""
        self.cleanup()

    def cleanup(self) -> None:
        """Cleanup resources when the instance is destroyed."""
        if hasattr(self, "_executor"):
            self._executor.shutdown(wait=False)
            logger.debug("Thread pool executor shut down")

    def set_repo_dir(self, repo_dir: Union[str, Path]) -> None:
        """
        Set the repository directory with validation.

        Args:
            repo_dir: Path to the Git repository.

        Raises:
            GitRepositoryNotFound: If the directory doesn't exist.
        """
        new_repo_dir = ensure_path(repo_dir)
        if new_repo_dir and not new_repo_dir.exists():
            raise GitRepositoryNotFound(
                f"Repository directory {new_repo_dir} does not exist",
                repository_path=new_repo_dir,
            )

        self.repo_dir = new_repo_dir
        self._clear_caches()
        logger.debug(f"Repository directory set to: {self.repo_dir}")

    def _clear_caches(self) -> None:
        """Clear all internal caches."""
        self._config_cache.clear()
        self._branch_cache.clear()
        self._cache_timestamp = 0
        logger.debug("Caches cleared")

    @cached_property
    def git_version(self) -> Optional[str]:
        """Get the Git version string with caching."""
        try:
            result = subprocess.run(
                ["git", "--version"], capture_output=True, text=True, timeout=10
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except (subprocess.TimeoutExpired, FileNotFoundError, Exception):
            pass
        return None

    @performance_monitor(GitOperation.STATUS)
    def run_git_command(
        self,
        command: List[str],
        check_errors: bool = True,
        capture_output: bool = True,
        cwd: Optional[Path] = None,
        timeout: Optional[int] = None,
    ) -> GitResult:
        """
        Enhanced Git command execution with comprehensive error handling.

        Args:
            command: The Git command and its arguments.
            check_errors: If True, raises exceptions for non-zero return codes.
            capture_output: If True, captures stdout and stderr.
            cwd: Directory to run the command in (overrides self.repo_dir).
            timeout: Command timeout in seconds.

        Returns:
            GitResult: Object containing the command's success status and output.

        Raises:
            GitCommandError: If the command fails and check_errors is True.
        """
        working_dir = cwd or self.repo_dir
        timeout = timeout or self.config.timeout

        cmd_str = " ".join(command)

        with change_directory(working_dir) as current_dir:
            logger.debug(
                f"Executing Git command: {cmd_str}",
                extra={
                    "command": command,
                    "working_directory": str(current_dir),
                    "capture_output": capture_output,
                    "timeout": timeout,
                },
            )

            try:
                start_time = time.time()

                result = subprocess.run(
                    command,
                    capture_output=capture_output,
                    text=True,
                    cwd=current_dir,
                    timeout=timeout,
                    env=self._get_enhanced_environment(),
                )

                duration = time.time() - start_time
                success = result.returncode == 0
                stdout = (
                    result.stdout.strip() if capture_output and result.stdout else ""
                )
                stderr = (
                    result.stderr.strip() if capture_output and result.stderr else ""
                )

                git_result = GitResult(
                    success=success,
                    message=stdout if success else stderr,
                    output=stdout,
                    error=stderr,
                    return_code=result.returncode,
                    duration=duration,
                    operation=self._infer_operation(command),
                )

                if not success and check_errors:
                    context = create_git_error_context(
                        working_dir=current_dir,
                        repo_path=self.repo_dir,
                        command=command,
                    )

                    raise GitCommandError(
                        command=command,
                        return_code=result.returncode,
                        stderr=stderr,
                        stdout=stdout,
                        duration=duration,
                        context=context,
                    )

                if not self.quiet:
                    log_level = "info" if success else "warning"
                    getattr(logger, log_level)(
                        f"Git command {'completed' if success else 'failed'}: {cmd_str}",
                        extra={
                            "success": success,
                            "duration": duration,
                            "return_code": result.returncode,
                        },
                    )

                return git_result

            except subprocess.TimeoutExpired as e:
                context = create_git_error_context(
                    working_dir=current_dir, repo_path=self.repo_dir, command=command
                )

                raise GitCommandError(
                    command=command,
                    return_code=-1,
                    stderr=f"Command timed out after {timeout} seconds",
                    duration=timeout,
                    context=context,
                ) from e

            except FileNotFoundError as e:
                error_msg = "Git executable not found. Is Git installed and in PATH?"
                logger.error(error_msg)
                return GitResult(
                    success=False, message=error_msg, error=error_msg, return_code=127
                )

            except PermissionError as e:
                error_msg = f"Permission denied when executing Git command: {cmd_str}"
                logger.error(error_msg)
                return GitResult(
                    success=False, message=error_msg, error=error_msg, return_code=126
                )

    def _get_enhanced_environment(self) -> Dict[str, str]:
        """Get enhanced environment variables for Git commands."""
        import os

        env = os.environ.copy()

        # Set consistent locale for parsing
        env["LC_ALL"] = "C"
        env["LANG"] = "C"

        # Configure Git behavior
        if self.config.sign_commits:
            env["GIT_COMMITTER_GPG_KEY"] = env.get("GIT_COMMITTER_GPG_KEY", "")

        return env

    def _infer_operation(self, command: List[str]) -> Optional[GitOperation]:
        """Infer the Git operation from the command."""
        if not command or command[0] != "git":
            return None

        if len(command) < 2:
            return None

        operation_map = {
            "clone": GitOperation.CLONE,
            "pull": GitOperation.PULL,
            "push": GitOperation.PUSH,
            "fetch": GitOperation.FETCH,
            "add": GitOperation.ADD,
            "commit": GitOperation.COMMIT,
            "reset": GitOperation.RESET,
            "branch": GitOperation.BRANCH,
            "checkout": GitOperation.BRANCH,
            "switch": GitOperation.BRANCH,
            "merge": GitOperation.MERGE,
            "rebase": GitOperation.REBASE,
            "cherry-pick": GitOperation.CHERRY_PICK,
            "stash": GitOperation.STASH,
            "tag": GitOperation.TAG,
            "remote": GitOperation.REMOTE,
            "config": GitOperation.CONFIG,
            "diff": GitOperation.DIFF,
            "log": GitOperation.LOG,
            "status": GitOperation.STATUS,
            "submodule": GitOperation.SUBMODULE,
        }

        return operation_map.get(command[1])

    @async_performance_monitor(GitOperation.STATUS)
    async def run_git_command_async(
        self,
        command: List[str],
        check_errors: bool = True,
        capture_output: bool = True,
        cwd: Optional[Path] = None,
        timeout: Optional[int] = None,
    ) -> GitResult:
        """
        Execute a Git command asynchronously with enhanced error handling.

        Args:
            command: The Git command and its arguments.
            check_errors: If True, raises exceptions for non-zero return codes.
            capture_output: If True, captures stdout and stderr.
            cwd: Directory to run the command in.
            timeout: Command timeout in seconds.

        Returns:
            GitResult: Object containing the command's success status and output.
        """
        working_dir = cwd or self.repo_dir
        timeout = timeout or self.config.timeout

        cmd_str = " ".join(command)
        logger.debug(
            f"Executing async Git command: {cmd_str}",
            extra={
                "command": command,
                "working_directory": str(working_dir) if working_dir else None,
                "async": True,
            },
        )

        try:
            start_time = time.time()

            process = await asyncio.create_subprocess_exec(
                *command,
                stdout=asyncio.subprocess.PIPE if capture_output else None,
                stderr=asyncio.subprocess.PIPE if capture_output else None,
                cwd=working_dir,
                env=self._get_enhanced_environment(),
            )

            try:
                stdout_data, stderr_data = await asyncio.wait_for(
                    process.communicate(), timeout=timeout
                )
            except asyncio.TimeoutError:
                process.kill()
                await process.wait()
                raise

            duration = time.time() - start_time
            stdout = stdout_data.decode("utf-8").strip() if stdout_data else ""
            stderr = stderr_data.decode("utf-8").strip() if stderr_data else ""
            success = process.returncode == 0

            git_result = GitResult(
                success=success,
                message=stdout if success else stderr,
                output=stdout,
                error=stderr,
                return_code=process.returncode or 0,
                duration=duration,
                operation=self._infer_operation(command),
            )

            if not success and check_errors:
                context = create_git_error_context(
                    working_dir=working_dir, repo_path=self.repo_dir, command=command
                )

                raise GitCommandError(
                    command=command,
                    return_code=process.returncode or -1,
                    stderr=stderr,
                    stdout=stdout,
                    duration=duration,
                    context=context,
                )

            if not self.quiet:
                log_level = "info" if success else "warning"
                getattr(logger, log_level)(
                    f"Async Git command {'completed' if success else 'failed'}: {cmd_str}",
                    extra={"success": success, "duration": duration, "async": True},
                )

            return git_result

        except asyncio.TimeoutError as e:
            context = create_git_error_context(
                working_dir=working_dir, repo_path=self.repo_dir, command=command
            )

            raise GitCommandError(
                command=command,
                return_code=-1,
                stderr=f"Async command timed out after {timeout} seconds",
                duration=timeout,
                context=context,
            ) from e

        except FileNotFoundError:
            error_msg = "Git executable not found. Is Git installed and in PATH?"
            logger.error(error_msg)
            return GitResult(
                success=False, message=error_msg, error=error_msg, return_code=127
            )

    # Repository Operations

    @performance_monitor(GitOperation.CLONE)
    @retry_on_failure(max_attempts=3)
    def clone_repository(
        self,
        repo_url: str,
        clone_dir: Union[str, Path],
        options: Optional[List[str]] = None,
    ) -> GitResult:
        """
        Enhanced repository cloning with validation and error handling.

        Args:
            repo_url: URL of the repository to clone.
            clone_dir: Directory to clone the repository into.
            options: Additional Git clone options.

        Returns:
            GitResult: Result of the clone operation.
        """
        target_dir = ensure_path(clone_dir)
        if not target_dir:
            raise ValueError("Clone directory cannot be None")

        if target_dir.exists() and any(target_dir.iterdir()):
            return GitResult(
                success=False,
                message=f"Directory {target_dir} already exists and is not empty",
                error="Directory not empty",
            )

        # Create parent directories
        target_dir.parent.mkdir(parents=True, exist_ok=True)

        command = ["git", "clone"]
        if options:
            command.extend(options)
        command.extend([repo_url, str(target_dir)])

        logger.info(f"Cloning repository {repo_url} to {target_dir}")

        result = self.run_git_command(command, cwd=None)
        if result.success:
            self.set_repo_dir(target_dir)
            logger.info(f"Successfully cloned repository to {target_dir}")

        return result

    @async_performance_monitor(GitOperation.CLONE)
    @async_retry_on_failure(max_attempts=3)
    async def clone_repository_async(
        self,
        repo_url: str,
        clone_dir: Union[str, Path],
        options: Optional[List[str]] = None,
    ) -> GitResult:
        """
        Asynchronously clone a repository.

        Args:
            repo_url: URL of the repository to clone.
            clone_dir: Directory to clone the repository into.
            options: Additional Git clone options.

        Returns:
            GitResult: Result of the clone operation.
        """
        target_dir = ensure_path(clone_dir)
        if not target_dir:
            raise ValueError("Clone directory cannot be None")

        if target_dir.exists() and any(target_dir.iterdir()):
            return GitResult(
                success=False,
                message=f"Directory {target_dir} already exists and is not empty",
                error="Directory not empty",
            )

        # Create parent directories
        target_dir.parent.mkdir(parents=True, exist_ok=True)

        command = ["git", "clone"]
        if options:
            command.extend(options)
        command.extend([repo_url, str(target_dir)])

        logger.info(f"Async cloning repository {repo_url} to {target_dir}")

        result = await self.run_git_command_async(command, cwd=None)
        if result.success:
            self.set_repo_dir(target_dir)
            logger.info(f"Successfully cloned repository to {target_dir}")

        return result

    # Change Management Operations

    @performance_monitor(GitOperation.ADD)
    @validate_repository
    def add_changes(
        self, paths: Optional[Union[str, List[str], Path, List[Path]]] = None
    ) -> GitResult:
        """
        Enhanced add operation with path validation.

        Args:
            paths: Files/directories to add. If None, adds all changes.

        Returns:
            GitResult: Result of the add operation.
        """
        command = ["git", "add"]

        if not paths:
            command.append(".")
        elif isinstance(paths, (str, Path)):
            command.append(str(paths))
        else:
            command.extend(str(p) for p in paths)

        with change_directory(self.repo_dir):
            result = self.run_git_command(command)
            if result.success:
                logger.info(f"Successfully added changes: {paths or 'all files'}")
            return result

    @performance_monitor(GitOperation.COMMIT)
    @validate_repository
    def commit_changes(
        self,
        message: str,
        all_changes: bool = False,
        amend: bool = False,
        sign: bool = False,
    ) -> GitResult:
        """
        Enhanced commit operation with message sanitization.

        Args:
            message: Commit message.
            all_changes: Stage all modified files before committing.
            amend: Amend the previous commit.
            sign: Sign the commit with GPG.

        Returns:
            GitResult: Result of the commit operation.
        """
        sanitized_message = sanitize_commit_message(message)

        command = ["git", "commit"]
        if all_changes:
            command.append("-a")
        if amend:
            command.append("--amend")
        if sign or self.config.sign_commits:
            command.append("-S")
        command.extend(["-m", sanitized_message])

        with change_directory(self.repo_dir):
            result = self.run_git_command(command)
            if result.success:
                logger.info(
                    f"Successfully committed changes: {sanitized_message[:50]}..."
                )
            return result

    @performance_monitor(GitOperation.STATUS)
    @validate_repository
    def view_status(self, porcelain: bool = False) -> GitResult:
        """
        Enhanced status operation with structured output.

        Args:
            porcelain: Use porcelain output format.

        Returns:
            GitResult: Result containing status information.
        """
        command = ["git", "status"]
        if porcelain:
            command.append("--porcelain")

        with change_directory(self.repo_dir):
            result = self.run_git_command(command)

            if result.success and porcelain:
                # Parse porcelain output into structured data
                files = self.parse_status(result.output)
                branch = self.get_current_branch()
                ahead_behind = self.get_ahead_behind_info(branch)

                status_info = StatusInfo(
                    branch=BranchName(branch),
                    is_clean=not bool(result.output.strip()),
                    ahead_behind=ahead_behind,
                    files=files,
                )
                result.data = status_info

            return result

    @validate_repository
    def get_current_branch(self) -> str:
        """
        Get the current branch name with enhanced error handling.

        Returns:
            str: Current branch name.

        Raises:
            GitBranchError: If unable to determine current branch.
        """
        command = ["git", "rev-parse", "--abbrev-ref", "HEAD"]
        result = self.run_git_command(command)

        if not result.success:
            raise GitBranchError(
                "Unable to determine current branch name",
                context=create_git_error_context(
                    working_dir=Path.cwd(), repo_path=self.repo_dir
                ),
            )

        branch_name = result.output.strip()
        if branch_name == "HEAD":
            # Detached HEAD state
            raise GitBranchError(
                "Repository is in detached HEAD state", is_detached=True
            )

        return branch_name

    def parse_status(self, output: str) -> List[FileStatus]:
        """
        Enhanced parsing of Git status output.

        Args:
            output: Porcelain status output.

        Returns:
            List[FileStatus]: Parsed file statuses.
        """
        files = []

        for line in output.strip().split("\n"):  # Changed from '\\n' to '\n'
            if not line:
                continue

            x_status = (
                GitStatusCode(line[0])
                if line[0] in [s.value for s in GitStatusCode]
                else GitStatusCode.UNMODIFIED
            )
            y_status = (
                GitStatusCode(line[1])
                if line[1] in [s.value for s in GitStatusCode]
                else GitStatusCode.UNMODIFIED
            )
            path = line[3:]

            # Handle renames (R) and copies (C)
            original_path = None
            similarity = None

            if (
                x_status in [GitStatusCode.RENAMED, GitStatusCode.COPIED]
                and "->" in path
            ):
                parts = path.split(" -> ")
                if len(parts) == 2:
                    original_path = FilePath(parts[0])
                    path = parts[1]

            files.append(
                FileStatus(
                    path=FilePath(path),
                    x_status=x_status,
                    y_status=y_status,
                    original_path=original_path,
                    similarity=similarity,
                )
            )

        return files

    @validate_repository
    def get_ahead_behind_info(
        self, branch: Optional[str] = None
    ) -> Optional[AheadBehindInfo]:
        """
        Enhanced ahead/behind information with better error handling.

        Args:
            branch: Branch to check. Defaults to current branch.

        Returns:
            Optional[AheadBehindInfo]: Ahead/behind information or None.
        """
        try:
            branch = branch or self.get_current_branch()
            command = [
                "git",
                "rev-list",
                "--left-right",
                "--count",
                f"origin/{branch}...{branch}",
            ]
            result = self.run_git_command(command, check_errors=False)

            if result.success and result.output.strip():
                try:
                    behind, ahead = map(int, result.output.split())
                    return AheadBehindInfo(ahead=ahead, behind=behind)
                except (ValueError, IndexError) as e:
                    logger.debug(
                        f"Failed to parse ahead/behind output: {result.output}"
                    )
        except GitBranchError:
            logger.debug("Cannot get ahead/behind info: not on a branch")
        except Exception as e:
            logger.debug(f"Error getting ahead/behind info: {e}")

        return None

    @validate_repository
    def is_dirty(self) -> bool:
        """
        Enhanced dirty state check.

        Returns:
            bool: True if repository has uncommitted changes.
        """
        result = self.view_status(porcelain=True)
        return bool(result.output.strip())


# Export enhanced GitUtils
__all__ = [
    "GitUtils",
    "GitConfig",
]
