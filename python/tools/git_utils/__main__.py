"""
Main entry point for command-line execution of Git utilities.
"""

import sys
import os
import json
from pathlib import Path

from loguru import logger

from .cli import setup_parser
from .exceptions import (
    GitException,
    GitCommandError,
    GitRepositoryNotFound,
    GitBranchError,
    GitMergeConflict,
    GitRebaseConflictError,
    GitCherryPickError,
)
from .models import GitResult


def configure_logging():
    """Configure loguru logger."""
    logger.remove()

    logger.add(
        sys.stderr,
        format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
        level="INFO",
        colorize=True,
    )

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
    configure_logging()

    parser = setup_parser()

    args = parser.parse_args()

    logger.debug(f"Command-line arguments: {args}")

    try:
        if hasattr(args, "func"):
            logger.info(f"Executing command: {args.command}")
            result = args.func(args)

            if isinstance(result, GitResult):
                if result.success:
                    if hasattr(args, "json") and args.json and result.data is not None:
                        print(
                            json.dumps(
                                result.data, default=lambda o: o.__dict__, indent=2
                            )
                        )
                    elif result.output:
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
    except (
        GitRepositoryNotFound,
        GitBranchError,
        GitMergeConflict,
        GitRebaseConflictError,
        GitCherryPickError,
    ) as e:
        logger.error(f"Git operation error: {e}")
        print(f"Git operation error: {e}", file=sys.stderr)
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
