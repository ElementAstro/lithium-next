class GitRebaseConflictError(GitException):
    """Raised when a rebase results in conflicts."""
    pass

class GitCherryPickError(GitException):
    """Raised when a cherry-pick operation fails."""
    pass