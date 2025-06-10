"""Exception classes for git utilities."""


class GitException(Exception):
    """Base exception for Git-related errors."""
    pass


class GitCommandError(GitException):
    """Raised when a Git command fails."""

    def __init__(self, command, return_code, stderr, stdout=""):
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
