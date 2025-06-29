"""
Core Git utility implementation.

This module provides the main GitUtils class for interacting with Git repositories.
"""

import asyncio
import subprocess
from pathlib import Path
from typing import List, Dict, Optional, Union, Tuple, Any

from loguru import logger

from .exceptions import GitCommandError, GitBranchError, GitMergeConflict
from .models import GitResult
from .utils import change_directory, ensure_path, validate_repository


class GitUtils:
    """
    A comprehensive utility class for Git operations.

    This class provides methods to interact with Git repositories both from
    command-line scripts and embedded Python code. It supports all common
    Git operations with enhanced error handling and configuration options.
    """

    def __init__(
        self, repo_dir: Optional[Union[str, Path]] = None, quiet: bool = False
    ):
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

    def run_git_command(
        self,
        command: List[str],
        check_errors: bool = True,
        capture_output: bool = True,
        cwd: Optional[Path] = None,
    ) -> GitResult:
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
        cmd_str = " ".join(command)
        logger.debug(
            f"Running git command: {cmd_str} in {working_dir or 'current directory'}"
        )

        try:
            # Execute the command
            result = subprocess.run(
                command, capture_output=capture_output, text=True, cwd=working_dir
            )

            success = result.returncode == 0
            stdout = result.stdout.strip() if capture_output else ""
            stderr = result.stderr.strip() if capture_output else ""

            # Handle command failure
            if not success and check_errors:
                raise GitCommandError(command, result.returncode, stderr, stdout)

            # Create result object
            message = stdout if success else stderr
            git_result = GitResult(
                success=success,
                message=message,
                output=stdout,
                error=stderr,
                return_code=result.returncode,
            )

            # Log result
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
            return GitResult(
                success=False, message=error_msg, error=error_msg, return_code=127
            )
        except PermissionError:
            error_msg = (
                f"Permission denied when executing Git command: {' '.join(command)}"
            )
            logger.error(error_msg)
            return GitResult(
                success=False, message=error_msg, error=error_msg, return_code=126
            )

    # Repository operations
    def clone_repository(
        self,
        repo_url: str,
        clone_dir: Union[str, Path],
        options: Optional[List[str]] = None,
    ) -> GitResult:
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
            logger.warning(
                f"Cannot clone: Directory {target_dir} already exists and is not empty"
            )
            return GitResult(
                success=False,
                message=f"Directory {target_dir} already exists and is not empty.",
                error=f"Directory {target_dir} already exists and is not empty.",
            )

        # Create parent directories if they don't exist
        target_dir.parent.mkdir(parents=True, exist_ok=True)

        # Build command with optional arguments
        command = ["git", "clone"]
        if options:
            command.extend(options)
        command.extend([repo_url, str(target_dir)])

        logger.info(f"Cloning repository {repo_url} to {target_dir}")
        result = self.run_git_command(command, cwd=None)

        # Set the repository directory to the newly cloned repo if successful
        if result.success:
            self.set_repo_dir(target_dir)
            logger.success(f"Repository cloned successfully to {target_dir}")

        return result

    @validate_repository
    def pull_latest_changes(
        self,
        remote: str = "origin",
        branch: Optional[str] = None,
        options: Optional[List[str]] = None,
    ) -> GitResult:
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

        logger.info(
            f"Pulling latest changes from {remote}" + (f"/{branch}" if branch else "")
        )

        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def fetch_changes(
        self,
        remote: str = "origin",
        refspec: Optional[str] = None,
        all_remotes: bool = False,
        prune: bool = False,
    ) -> GitResult:
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

        fetch_from = "all remotes" if all_remotes else remote
        logger.info(
            f"Fetching changes from {fetch_from}" + (f" ({refspec})" if refspec else "")
        )

        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def push_changes(
        self,
        remote: str = "origin",
        branch: Optional[str] = None,
        force: bool = False,
        tags: bool = False,
    ) -> GitResult:
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

        push_info = []
        if force:
            push_info.append("force")
        if tags:
            push_info.append("with tags")
        push_info_str = f" ({', '.join(push_info)})" if push_info else ""

        logger.info(
            f"Pushing changes to {remote}"
            + (f"/{branch}" if branch else "")
            + push_info_str
        )
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
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
            logger.info(f"Adding all changes to staging area")
        elif isinstance(paths, str):
            command.append(paths)
            logger.info(f"Adding changes from {paths} to staging area")
        else:
            command.extend(paths)
            logger.info(f"Adding changes from {len(paths)} paths to staging area")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def commit_changes(
        self, message: str, all_changes: bool = False, amend: bool = False
    ) -> GitResult:
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

        commit_type = "Amending commit" if amend else "Committing changes"
        commit_type += " with auto-staging" if all_changes else ""

        logger.info(
            f"{commit_type}: {message[:50]}{'...' if len(message) > 50 else ''}"
        )
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def reset_changes(
        self,
        target: str = "HEAD",
        mode: str = "mixed",
        paths: Optional[List[str]] = None,
    ) -> GitResult:
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
                logger.error(f"Invalid reset mode: {mode}")
                return GitResult(
                    success=False,
                    message=f"Invalid reset mode: {mode}. Use 'soft', 'mixed', or 'hard'.",
                    error=f"Invalid reset mode: {mode}",
                )

            command.append(target)
            logger.info(f"Resetting repository to {target} with {mode} mode")
        else:
            # If paths provided, add target and paths
            command.append(target)
            command.extend(paths)
            logger.info(f"Resetting {len(paths)} paths to {target}")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def stash_changes(
        self, message: Optional[str] = None, include_untracked: bool = False
    ) -> GitResult:
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

        log_msg = f"Stashing changes"
        if include_untracked:
            log_msg += " (including untracked files)"
        if message:
            log_msg += f": {message}"
        logger.info(log_msg)
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def apply_stash(
        self, stash_id: str = "stash@{0}", pop: bool = False, index: bool = False
    ) -> GitResult:
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

        action = "Popping" if pop else "Applying"
        logger.info(f"{action} stash {stash_id}" + (" with index" if index else ""))
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
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

        logger.info("Listing stashes")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    # Branch operations
    @validate_repository
    def create_branch(
        self, branch_name: str, start_point: Optional[str] = None, checkout: bool = True
    ) -> GitResult:
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

        action = "Creating and checking out" if checkout else "Creating"
        logger.info(
            f"{action} branch '{branch_name}'"
            + (f" from '{start_point}'" if start_point else "")
        )
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def switch_branch(
        self, branch_name: str, create: bool = False, force: bool = False
    ) -> GitResult:
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

        action = []
        if create:
            action.append("creating")
        if force:
            action.append("force")

        action_str = " (" + ", ".join(action) + ")" if action else ""
        logger.info(f"Switching to branch '{branch_name}'{action_str}")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def merge_branch(
        self,
        branch_name: str,
        strategy: Optional[str] = None,
        commit_message: Optional[str] = None,
        no_ff: bool = False,
    ) -> GitResult:
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

        merge_options = []
        if strategy:
            merge_options.append(f"strategy={strategy}")
        if no_ff:
            merge_options.append("no-ff")

        options_str = " (" + ", ".join(merge_options) + ")" if merge_options else ""
        logger.info(f"Merging branch '{branch_name}' into current branch{options_str}")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            result = self.run_git_command(
                command, check_errors=False, cwd=self.repo_dir
            )

            # Check for merge conflicts
            if not result.success and "CONFLICT" in result.error:
                logger.warning(
                    f"Merge conflicts detected while merging '{branch_name}'"
                )
                raise GitMergeConflict(f"Merge conflicts detected: {result.error}")

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

        list_type = "all" if show_all else "local"
        verbose_str = " (verbose)" if verbose else ""
        logger.info(f"Listing {list_type} branches{verbose_str}")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def delete_branch(
        self, branch_name: str, force: bool = False, remote: Optional[str] = None
    ) -> GitResult:
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
            logger.info(f"Deleting remote branch '{branch_name}' from '{remote}'")
        else:
            command = ["git", "branch", "-D" if force else "-d", branch_name]
            logger.info(
                f"Deleting local branch '{branch_name}'" + (" (force)" if force else "")
            )
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
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

        logger.debug("Getting current branch name")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            result = self.run_git_command(command, cwd=self.repo_dir)

        if not result.success:
            logger.error("Failed to determine current branch name")
            raise GitBranchError("Unable to determine current branch name")

        logger.debug(f"Current branch is '{result.output.strip()}'")
        return result.output.strip()

    # Tag operations
    @validate_repository
    def create_tag(
        self,
        tag_name: str,
        commit: str = "HEAD",
        message: Optional[str] = None,
        annotated: bool = True,
    ) -> GitResult:
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
            logger.info(
                f"Creating annotated tag '{tag_name}' at '{commit}' with message"
            )
        else:
            command.append(tag_name)
            command.append(commit)
            logger.info(f"Creating tag '{tag_name}' at '{commit}'")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
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
            logger.info(f"Deleting remote tag '{tag_name}' from '{remote}'")
        else:
            command = ["git", "tag", "-d", tag_name]
            logger.info(f"Deleting local tag '{tag_name}'")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
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

        logger.info(f"Adding remote '{remote_name}' with URL '{remote_url}'")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
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

        logger.info(f"Removing remote '{remote_name}'")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
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

        format_type = "machine-readable" if porcelain else "human-readable"
        logger.info(f"Getting repository status ({format_type})")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    @validate_repository
    def view_log(
        self,
        num_entries: Optional[int] = None,
        oneline: bool = True,
        graph: bool = False,
        all_branches: bool = False,
    ) -> GitResult:
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

        log_options = []
        if oneline:
            log_options.append("oneline")
        if graph:
            log_options.append("graph")
        if all_branches:
            log_options.append("all branches")
        if num_entries:
            log_options.append(f"limit {num_entries}")

        options_str = " (" + ", ".join(log_options) + ")" if log_options else ""
        logger.info(f"Viewing commit log{options_str}")
        if self.repo_dir is None:
            raise ValueError("Repository directory is not set.")
        with change_directory(self.repo_dir):
            return self.run_git_command(command, cwd=self.repo_dir)

    # Configuration
    @validate_repository
    def set_user_info(
        self,
        name: Optional[str] = None,
        email: Optional[str] = None,
        global_config: bool = False,
    ) -> GitResult:
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
        scope = "global" if global_config else "repository"

        if name:
            command = ["git", "config", config_flag, "user.name", name]
            logger.info(f"Setting {scope} user name to '{name}'")
            if self.repo_dir is None:
                raise ValueError("Repository directory is not set.")
            with change_directory(self.repo_dir):
                results.append(self.run_git_command(command, cwd=self.repo_dir))

        if email:
            command = ["git", "config", config_flag, "user.email", email]
            logger.info(f"Setting {scope} user email to '{email}'")
            if self.repo_dir is None:
                raise ValueError("Repository directory is not set.")
            with change_directory(self.repo_dir):
                results.append(self.run_git_command(command, cwd=self.repo_dir))

        if not results:
            logger.warning("No name or email provided to set")
            return GitResult(
                success=False,
                message="No name or email provided to set",
                error="No name or email provided to set",
            )

        # Return success only if all operations succeeded
        all_success = all(result.success for result in results)
        if all_success:
            logger.success(f"User info set successfully")
        else:
            logger.warning("Failed to set some user info")

        return GitResult(
            success=all_success,
            message=(
                "User info set successfully"
                if all_success
                else "Failed to set some user info"
            ),
            output="\n".join(result.output for result in results),
            error="\n".join(result.error for result in results if result.error),
        )

    # Async versions for concurrent operations
    async def run_git_command_async(
        self,
        command: List[str],
        check_errors: bool = True,
        capture_output: bool = True,
        cwd: Optional[Path] = None,
    ) -> GitResult:
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
        working_dir = str(cwd or self.repo_dir) if cwd or self.repo_dir else None
        cmd_str = " ".join(command)
        logger.debug(
            f"Running async git command: {cmd_str} in {working_dir or 'current directory'}"
        )

        try:
            # Create subprocess
            process = await asyncio.create_subprocess_exec(
                *command,
                stdout=asyncio.subprocess.PIPE if capture_output else None,
                stderr=asyncio.subprocess.PIPE if capture_output else None,
                cwd=working_dir,
            )

            # Wait for completion and get output
            stdout_data, stderr_data = await process.communicate()

            stdout = stdout_data.decode("utf-8").strip() if stdout_data else ""
            stderr = stderr_data.decode("utf-8").strip() if stderr_data else ""

            success = process.returncode == 0

            # Handle command failure
            if not success and check_errors:
                raise GitCommandError(command, process.returncode, stderr, stdout)

            # Create result object
            message = stdout if success else stderr
            git_result = GitResult(
                success=success,
                message=message,
                output=stdout,
                error=stderr,
                return_code=process.returncode if process.returncode is not None else 1,
            )

            # Log result
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
            return GitResult(
                success=False, message=error_msg, error=error_msg, return_code=127
            )
