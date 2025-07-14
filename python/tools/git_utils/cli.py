"""
Command-line interface for Git utilities.

This module provides functions to interact with Git repositories via command line.
"""

import argparse
import json
from typing import List, Optional

from loguru import logger

from .git_utils import GitUtils
from .models import GitResult, StatusInfo, CommitInfo, AheadBehindInfo


def cli_clone_repository(args) -> GitResult:
    """Clone a repository from the command line."""
    git = GitUtils()
    options = []
    if hasattr(args, "depth") and args.depth:
        options.append(f"--depth={args.depth}")
    if hasattr(args, "branch") and args.branch:
        options.extend(["--branch", args.branch])
    return git.clone_repository(args.repo_url, args.clone_dir, options)


def cli_pull_latest_changes(args) -> GitResult:
    """Pull latest changes from the command line."""
    git = GitUtils(args.repo_dir)
    return git.pull_latest_changes(args.remote, args.branch)


def cli_fetch_changes(args) -> GitResult:
    """Fetch changes from the command line."""
    git = GitUtils(args.repo_dir)
    return git.fetch_changes(args.remote, args.refspec, args.all, args.prune)


def cli_push_changes(args) -> GitResult:
    """Push changes from the command line."""
    git = GitUtils(args.repo_dir)
    return git.push_changes(args.remote, args.branch, args.force, args.tags)


def cli_add_changes(args) -> GitResult:
    """Add changes from the command line."""
    git = GitUtils(args.repo_dir)
    return git.add_changes(args.paths)


def cli_commit_changes(args) -> GitResult:
    """Commit changes from the command line."""
    git = GitUtils(args.repo_dir)
    return git.commit_changes(args.message, args.all, args.amend)


def cli_reset_changes(args) -> GitResult:
    """Reset changes from the command line."""
    git = GitUtils(args.repo_dir)
    return git.reset_changes(args.target, args.mode, args.paths)


def cli_create_branch(args) -> GitResult:
    """Create a branch from the command line."""
    git = GitUtils(args.repo_dir)
    return git.create_branch(args.branch_name, args.start_point)


def cli_switch_branch(args) -> GitResult:
    """Switch branches from the command line."""
    git = GitUtils(args.repo_dir)
    return git.switch_branch(args.branch_name, args.create, args.force)


def cli_merge_branch(args) -> GitResult:
    """Merge branches from the command line."""
    git = GitUtils(args.repo_dir)
    return git.merge_branch(args.branch_name, args.strategy, args.message, args.no_ff)


def cli_list_branches(args) -> GitResult:
    """List branches from the command line."""
    git = GitUtils(args.repo_dir)
    return git.list_branches(args.all, args.verbose)


def cli_view_status(args) -> GitResult:
    """View status from the command line."""
    git = GitUtils(args.repo_dir)
    result = git.view_status(porcelain=True)
    if result.success:
        files = git.parse_status(result.output)
        branch = git.get_current_branch()
        ahead_behind = git.get_ahead_behind_info(branch)
        status_info = StatusInfo(
            branch=branch,
            is_clean=not bool(result.output),
            ahead_behind=ahead_behind,
            files=files,
        )
        result.data = status_info
    return result


def cli_view_log(args) -> GitResult:
    """View log from the command line."""
    git = GitUtils(args.repo_dir)
    result = git.view_log(args.num_entries, args.oneline, args.graph, args.all)
    if result.success and args.json:
        result.data = git.parse_log(result.output)
    return result


def cli_add_remote(args) -> GitResult:
    """Add a remote from the command line."""
    git = GitUtils(args.repo_dir)
    return git.add_remote(args.remote_name, args.remote_url)


def cli_remove_remote(args) -> GitResult:
    """Remove a remote from the command line."""
    git = GitUtils(args.repo_dir)
    return git.remove_remote(args.remote_name)


def cli_create_tag(args) -> GitResult:
    """Create a tag from the command line."""
    git = GitUtils(args.repo_dir)
    return git.create_tag(args.tag_name, args.commit, args.message, args.annotated)


def cli_delete_tag(args) -> GitResult:
    """Delete a tag from the command line."""
    git = GitUtils(args.repo_dir)
    return git.delete_tag(args.tag_name, args.remote)


def cli_stash_changes(args) -> GitResult:
    """Stash changes from the command line."""
    git = GitUtils(args.repo_dir)
    return git.stash_changes(args.message, args.include_untracked)


def cli_apply_stash(args) -> GitResult:
    """Apply stash from the command line."""
    git = GitUtils(args.repo_dir)
    return git.apply_stash(args.stash_id, args.pop, args.index)


def cli_set_user_info(args) -> GitResult:
    """Set user info from the command line."""
    git = GitUtils(args.repo_dir)
    return git.set_user_info(args.name, args.email, args.global_config)


def cli_diff(args) -> GitResult:
    """Show changes from the command line."""
    git = GitUtils(args.repo_dir)
    return git.diff(args.cached, args.other)


def cli_rebase(args) -> GitResult:
    """Rebase from the command line."""
    git = GitUtils(args.repo_dir)
    return git.rebase(args.branch, args.interactive)


def cli_cherry_pick(args) -> GitResult:
    """Cherry-pick a commit from the command line."""
    git = GitUtils(args.repo_dir)
    return git.cherry_pick(args.commit)


def cli_submodule_update(args) -> GitResult:
    """Update submodules from the command line."""
    git = GitUtils(args.repo_dir)
    return git.submodule_update(not args.no_init, not args.no_recursive)


def cli_get_config(args) -> GitResult:
    """Get a config value from the command line."""
    git = GitUtils(args.repo_dir)
    return git.get_config(args.key, args.global_config)


def cli_set_config(args) -> GitResult:
    """Set a config value from the command line."""
    git = GitUtils(args.repo_dir)
    return git.set_config(args.key, args.value, args.global_config)


def cli_is_dirty(args) -> GitResult:
    """Check if the repository is dirty from the command line."""
    git = GitUtils(args.repo_dir)
    is_dirty = git.is_dirty()
    return GitResult(
        success=True, message=str(is_dirty), output=str(is_dirty), data=is_dirty
    )


def cli_ahead_behind(args) -> GitResult:
    """Get ahead/behind info from the command line."""
    git = GitUtils(args.repo_dir)
    info = git.get_ahead_behind_info(args.branch)
    if info:
        return GitResult(
            success=True,
            message=f"Ahead: {info.ahead}, Behind: {info.behind}",
            data=info.__dict__,
        )
    return GitResult(success=False, message="Could not get ahead/behind info.")


def setup_parser() -> argparse.ArgumentParser:
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
  python -m python.tools.git_utils clone https://github.com/user/repo.git ./destination

  # Pull latest changes:
  python -m python.tools.git_utils pull --repo-dir ./my_repo

  # Create and switch to a new branch:
  python -m python.tools.git_utils create-branch --repo-dir ./my_repo new-feature

  # Add and commit changes:
  python -m python.tools.git_utils add --repo-dir ./my_repo
  python -m python.tools.git_utils commit --repo-dir ./my_repo -m "Added new feature"

  # Push changes to remote:
  python -m python.tools.git_utils push --repo-dir ./my_repo
        """,
    )

    subparsers = parser.add_subparsers(
        dest="command", help="Git command to run", required=True
    )

    def add_repo_dir(subparser):
        subparser.add_argument(
            "--repo-dir",
            "-d",
            default=".",
            help="Directory of the repository (default: current directory)",
        )

    # Clone command
    parser_clone = subparsers.add_parser("clone", help="Clone a repository")
    parser_clone.add_argument("repo_url", help="URL of the repository to clone")
    parser_clone.add_argument(
        "clone_dir", help="Directory to clone the repository into"
    )
    parser_clone.add_argument(
        "--depth", type=int, help="Create a shallow clone with specified depth"
    )
    parser_clone.add_argument("--branch", "-b", help="Clone a specific branch")
    parser_clone.set_defaults(func=cli_clone_repository)

    # Pull command
    parser_pull = subparsers.add_parser(
        "pull", help="Pull the latest changes from remote"
    )
    add_repo_dir(parser_pull)
    parser_pull.add_argument(
        "--remote", default="origin", help="Remote to pull from (default: origin)"
    )
    parser_pull.add_argument("--branch", help="Branch to pull")
    parser_pull.set_defaults(func=cli_pull_latest_changes)

    # ... (rest of the parser setup from the original file)

    # Status command
    parser_status = subparsers.add_parser("status", help="View the current status")
    add_repo_dir(parser_status)
    parser_status.add_argument(
        "--json", action="store_true", help="Output in JSON format"
    )
    parser_status.set_defaults(func=cli_view_status)

    # Log command
    parser_log = subparsers.add_parser("log", help="View commit history")
    add_repo_dir(parser_log)
    parser_log.add_argument(
        "-n", "--num-entries", type=int, help="Number of commits to display"
    )
    parser_log.add_argument(
        "--oneline", action="store_true", default=True, help="One line per commit"
    )
    parser_log.add_argument("--graph", action="store_true", help="Show branch graph")
    parser_log.add_argument(
        "-a", "--all", action="store_true", help="Show commits from all branches"
    )
    parser_log.add_argument("--json", action="store_true", help="Output in JSON format")
    parser_log.set_defaults(func=cli_view_log)

    # Diff command
    parser_diff = subparsers.add_parser("diff", help="Show changes")
    add_repo_dir(parser_diff)
    parser_diff.add_argument(
        "--cached", action="store_true", help="Show staged changes"
    )
    parser_diff.add_argument(
        "other", nargs="?", help="Commit or branch to compare against"
    )
    parser_diff.set_defaults(func=cli_diff)

    # Rebase command
    parser_rebase = subparsers.add_parser("rebase", help="Rebase current branch")
    add_repo_dir(parser_rebase)
    parser_rebase.add_argument("branch", help="Branch to rebase onto")
    parser_rebase.add_argument(
        "-i", "--interactive", action="store_true", help="Interactive rebase"
    )
    parser_rebase.set_defaults(func=cli_rebase)

    # Cherry-pick command
    parser_cherry_pick = subparsers.add_parser("cherry-pick", help="Apply a commit")
    add_repo_dir(parser_cherry_pick)
    parser_cherry_pick.add_argument("commit", help="Commit to cherry-pick")
    parser_cherry_pick.set_defaults(func=cli_cherry_pick)

    # Submodule command
    parser_submodule = subparsers.add_parser(
        "submodule-update", help="Update submodules"
    )
    add_repo_dir(parser_submodule)
    parser_submodule.add_argument(
        "--no-init", action="store_true", help="Do not initialize submodules"
    )
    parser_submodule.add_argument(
        "--no-recursive", action="store_true", help="Do not update recursively"
    )
    parser_submodule.set_defaults(func=cli_submodule_update)

    # Config commands
    parser_get_config = subparsers.add_parser("get-config", help="Get a config value")
    add_repo_dir(parser_get_config)
    parser_get_config.add_argument("key", help="Config key")
    parser_get_config.add_argument(
        "--global", dest="global_config", action="store_true", help="Get global config"
    )
    parser_get_config.set_defaults(func=cli_get_config)

    parser_set_config = subparsers.add_parser("set-config", help="Set a config value")
    add_repo_dir(parser_set_config)
    parser_set_config.add_argument("key", help="Config key")
    parser_set_config.add_argument("value", help="Config value")
    parser_set_config.add_argument(
        "--global", dest="global_config", action="store_true", help="Set global config"
    )
    parser_set_config.set_defaults(func=cli_set_config)

    # Status-related commands
    parser_is_dirty = subparsers.add_parser(
        "is-dirty", help="Check for uncommitted changes"
    )
    add_repo_dir(parser_is_dirty)
    parser_is_dirty.set_defaults(func=cli_is_dirty)

    parser_ahead_behind = subparsers.add_parser(
        "ahead-behind", help="Get ahead/behind info for a branch"
    )
    add_repo_dir(parser_ahead_behind)
    parser_ahead_behind.add_argument(
        "--branch", help="Branch to check (defaults to current)"
    )
    parser_ahead_behind.set_defaults(func=cli_ahead_behind)

    return parser
