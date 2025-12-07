"""
Lithium export declarations for git_utils module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from lithium_bridge import expose_command, expose_controller

from .git_utils import GitUtils


@expose_controller(
    endpoint="/api/git/clone",
    method="POST",
    description="Clone a Git repository",
    tags=["git", "repository"],
    version="1.0.0",
)
def clone_repository(
    url: str,
    destination: str,
    branch: str = None,
    depth: int = None,
) -> dict:
    """
    Clone a Git repository.

    Args:
        url: Repository URL to clone
        destination: Local path for the cloned repository
        branch: Specific branch to clone
        depth: Shallow clone depth

    Returns:
        Dictionary with clone result
    """
    try:
        git = GitUtils()
        result = git.clone_repository(
            url=url,
            destination=destination,
            branch=branch,
            depth=depth,
        )
        return {
            "url": url,
            "destination": destination,
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "url": url,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/git/pull",
    method="POST",
    description="Pull latest changes from remote",
    tags=["git", "repository"],
    version="1.0.0",
)
def pull_changes(
    repo_dir: str,
    remote: str = "origin",
    branch: str = None,
) -> dict:
    """
    Pull latest changes from remote repository.

    Args:
        repo_dir: Path to the repository
        remote: Remote name (default: origin)
        branch: Branch to pull

    Returns:
        Dictionary with pull result
    """
    try:
        git = GitUtils(repo_dir=repo_dir)
        result = git.pull_latest_changes(remote=remote, branch=branch)
        return {
            "repo_dir": repo_dir,
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "repo_dir": repo_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/git/push",
    method="POST",
    description="Push changes to remote",
    tags=["git", "repository"],
    version="1.0.0",
)
def push_changes(
    repo_dir: str,
    remote: str = "origin",
    branch: str = None,
    force: bool = False,
) -> dict:
    """
    Push changes to remote repository.

    Args:
        repo_dir: Path to the repository
        remote: Remote name (default: origin)
        branch: Branch to push
        force: Force push

    Returns:
        Dictionary with push result
    """
    try:
        git = GitUtils(repo_dir=repo_dir)
        result = git.push_changes(remote=remote, branch=branch, force=force)
        return {
            "repo_dir": repo_dir,
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "repo_dir": repo_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/git/commit",
    method="POST",
    description="Commit changes",
    tags=["git", "changes"],
    version="1.0.0",
)
def commit_changes(
    repo_dir: str,
    message: str,
    add_all: bool = False,
) -> dict:
    """
    Commit changes to the repository.

    Args:
        repo_dir: Path to the repository
        message: Commit message
        add_all: Add all changes before committing

    Returns:
        Dictionary with commit result
    """
    try:
        git = GitUtils(repo_dir=repo_dir)
        if add_all:
            git.add_changes(all_changes=True)
        result = git.commit_changes(message=message)
        return {
            "repo_dir": repo_dir,
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "repo_dir": repo_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/git/status",
    method="GET",
    description="Get repository status",
    tags=["git", "info"],
    version="1.0.0",
)
def get_status(repo_dir: str) -> dict:
    """
    Get the status of a repository.

    Args:
        repo_dir: Path to the repository

    Returns:
        Dictionary with status information
    """
    try:
        git = GitUtils(repo_dir=repo_dir)
        result = git.view_status()
        return {
            "repo_dir": repo_dir,
            "success": result.success,
            "status": result.message,
        }
    except Exception as e:
        return {
            "repo_dir": repo_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/git/branches",
    method="GET",
    description="List branches",
    tags=["git", "branch"],
    version="1.0.0",
)
def list_branches(repo_dir: str, all_branches: bool = False) -> dict:
    """
    List branches in a repository.

    Args:
        repo_dir: Path to the repository
        all_branches: Include remote branches

    Returns:
        Dictionary with branch list
    """
    try:
        git = GitUtils(repo_dir=repo_dir)
        result = git.list_branches(all_branches=all_branches)
        branches = result.message.split("\n") if result.message else []
        return {
            "repo_dir": repo_dir,
            "success": result.success,
            "branches": [b.strip() for b in branches if b.strip()],
        }
    except Exception as e:
        return {
            "repo_dir": repo_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/git/branch/create",
    method="POST",
    description="Create a new branch",
    tags=["git", "branch"],
    version="1.0.0",
)
def create_branch(
    repo_dir: str,
    branch_name: str,
    checkout: bool = True,
) -> dict:
    """
    Create a new branch.

    Args:
        repo_dir: Path to the repository
        branch_name: Name for the new branch
        checkout: Switch to the new branch after creation

    Returns:
        Dictionary with creation result
    """
    try:
        git = GitUtils(repo_dir=repo_dir)
        result = git.create_branch(branch_name=branch_name, checkout=checkout)
        return {
            "repo_dir": repo_dir,
            "branch_name": branch_name,
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "repo_dir": repo_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/git/branch/switch",
    method="POST",
    description="Switch to a branch",
    tags=["git", "branch"],
    version="1.0.0",
)
def switch_branch(repo_dir: str, branch_name: str) -> dict:
    """
    Switch to a different branch.

    Args:
        repo_dir: Path to the repository
        branch_name: Branch to switch to

    Returns:
        Dictionary with switch result
    """
    try:
        git = GitUtils(repo_dir=repo_dir)
        result = git.switch_branch(branch_name=branch_name)
        return {
            "repo_dir": repo_dir,
            "branch_name": branch_name,
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "repo_dir": repo_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/git/log",
    method="GET",
    description="View commit log",
    tags=["git", "info"],
    version="1.0.0",
)
def view_log(
    repo_dir: str,
    count: int = 10,
    oneline: bool = True,
) -> dict:
    """
    View commit log.

    Args:
        repo_dir: Path to the repository
        count: Number of commits to show
        oneline: Use one-line format

    Returns:
        Dictionary with log entries
    """
    try:
        git = GitUtils(repo_dir=repo_dir)
        result = git.view_log(count=count, oneline=oneline)
        logs = result.message.split("\n") if result.message else []
        return {
            "repo_dir": repo_dir,
            "success": result.success,
            "logs": [line.strip() for line in logs if line.strip()],
        }
    except Exception as e:
        return {
            "repo_dir": repo_dir,
            "success": False,
            "error": str(e),
        }


@expose_command(
    command_id="git.clone",
    description="Clone repository (command)",
    priority=5,
    timeout_ms=300000,
    tags=["git"],
)
def cmd_clone(url: str, destination: str) -> dict:
    """Clone repository via command dispatcher."""
    return clone_repository(url, destination)


@expose_command(
    command_id="git.pull",
    description="Pull changes (command)",
    priority=5,
    timeout_ms=60000,
    tags=["git"],
)
def cmd_pull(repo_dir: str) -> dict:
    """Pull changes via command dispatcher."""
    return pull_changes(repo_dir)


@expose_command(
    command_id="git.commit",
    description="Commit changes (command)",
    priority=5,
    timeout_ms=30000,
    tags=["git"],
)
def cmd_commit(repo_dir: str, message: str) -> dict:
    """Commit changes via command dispatcher."""
    return commit_changes(repo_dir, message, add_all=True)


@expose_command(
    command_id="git.status",
    description="Get status (command)",
    priority=5,
    timeout_ms=10000,
    tags=["git"],
)
def cmd_status(repo_dir: str) -> dict:
    """Get status via command dispatcher."""
    return get_status(repo_dir)
