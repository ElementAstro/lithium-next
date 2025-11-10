"""
Main entry point for command-line execution of Git utilities.
"""

import sys
import os
from pathlib import Path

from loguru import logger

from .cli import setup_parser
from .exceptions import (
    GitException,
    GitCommandError,
    GitRepositoryNotFound,
    GitBranchError,
    GitMergeConflict,
)
from .models import GitResult


def configure_logging():
    """Configure loguru logger."""
    # Remove the default handler
    logger.remove()

    # Add a new handler for stderr with custom format
    logger.add(
        sys.stderr,
        format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
        level="INFO",
        colorize=True,
    )

    # Add a file handler for more detailed logs
    log_dir = Path.home() / ".git_utils" / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    log_file = log_dir / "git_utils.log"

    logger.add(
        log_file,
        format="{time:YYYY-MM-DD HH:mm:ss} | {level: <8} | {name}:{function}:{line} - {message}",
        level="DEBUG",
        rotation="10 MB",
        retention="1 week",
    )


def main():
    """
    Main function for command-line execution.

    This function parses command-line arguments and executes the corresponding
    Git operation, printing the result to the console.
    """
    # Configure logging
    configure_logging()

    # Set up argument parser
    parser = setup_parser()

    # Parse arguments
    args = parser.parse_args()

    logger.debug(f"Command-line arguments: {args}")

    try:
        # Execute the selected function
        if hasattr(args, "func"):
            logger.info(f"Executing command: {args.command}")
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
                        f"Error: {result.error if result.error else result.message}",
                        file=sys.stderr,
                    )
                    sys.exit(1)
        else:
            logger.debug("No command specified, showing help")
            parser.print_help()

    except GitCommandError as e:
        logger.error(f"Git command error: {e}")
        print(f"Git command error: {e}", file=sys.stderr)
        sys.exit(1)
    except GitRepositoryNotFound as e:
        logger.error(f"Git repository error: {e}")
        print(f"Git repository error: {e}", file=sys.stderr)
        sys.exit(1)
    except GitBranchError as e:
        logger.error(f"Git branch error: {e}")
        print(f"Git branch error: {e}", file=sys.stderr)
        sys.exit(1)
    except GitMergeConflict as e:
        logger.error(f"Git merge conflict: {e}")
        print(f"Git merge conflict: {e}", file=sys.stderr)
        sys.exit(1)
    except GitException as e:
        logger.error(f"Git error: {e}")
        print(f"Git error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        logger.exception(f"Unexpected error: {e}")
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
