"""
Command-line interface for Git utilities.

This module provides functions to interact with Git repositories via command line.
"""

import argparse
from typing import List, Optional

from loguru import logger

from .git_utils import GitUtils
from .models import GitResult


def cli_clone_repository(args) -> GitResult:
    """Clone a repository from the command line."""
    git = GitUtils()
    options = []
    if hasattr(args, 'depth') and args.depth:
        options.append(f"--depth={args.depth}")
    if hasattr(args, 'branch') and args.branch:
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
    return git.view_status(args.porcelain)


def cli_view_log(args) -> GitResult:
    """View log from the command line."""
    git = GitUtils(args.repo_dir)
    return git.view_log(args.num_entries, args.oneline, args.graph, args.all)


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
