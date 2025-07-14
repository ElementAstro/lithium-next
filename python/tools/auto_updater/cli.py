# cli.py
import argparse
import json
import sys
import traceback
from pathlib import Path

from .core import AutoUpdater
from .types import UpdaterError, UpdaterConfig


def main() -> int:
    """
    The main entry point for the command-line interface.

    Returns:
        int: Exit code (0 for success, non-zero for failure)
    """
    parser = argparse.ArgumentParser(
        description="Auto Updater - Check for and install software updates"
    )

    parser.add_argument(
        "--config",
        type=str,
        required=True,
        help="Path to the configuration file (JSON)",
    )

    parser.add_argument(
        "--check-only",
        action="store_true",
        help="Only check for updates, don't download or install",
    )

    parser.add_argument(
        "--download-only",
        action="store_true",
        help="Download but don't install updates",
    )

    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="Download and verify but don't install updates",
    )

    parser.add_argument(
        "--rollback", type=str, help="Path to backup directory to rollback to"
    )

    parser.add_argument(
        "--verbose",
        "-v",
        action="count",
        default=0,
        help="Increase verbosity (can be used multiple times)",
    )

    args = parser.parse_args()

    # Configure logger based on verbosity level
    if args.verbose > 0:
        from .logger import logger

        logger.remove()
        logger.add(
            sink=sys.stderr,
            level="DEBUG" if args.verbose > 1 else "INFO",
            format="<green>{time:YYYY-MM-DD HH:mm:ss.SSS}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
        )

    updater = None  # Ensure updater is always defined
    try:
        # Load configuration
        with open(args.config, "r") as f:
            config = json.load(f)

        # Create updater
        updater = AutoUpdater(config)

        # Process command
        if args.rollback:
            # Rollback to specified backup
            rollback_dir = Path(args.rollback)
            if not rollback_dir.exists():
                print(f"Error: Backup directory not found: {rollback_dir}")
                return 1

            success = updater.rollback(rollback_dir)
            print("Rollback " + ("successful" if success else "failed"))
            return 0 if success else 1

        elif args.check_only:
            # Only check for updates
            update_available = updater.check_for_updates()
            if update_available and updater.update_info:
                print(f"Update available: {updater.update_info.version}")
            else:
                print(
                    f"No updates available (current version: {config['current_version']})"
                )
            return 0

        elif args.download_only:
            # Check and download only
            update_available = updater.check_for_updates()
            if not update_available:
                print(
                    f"No updates available (current version: {config['current_version']})"
                )
                return 0

            download_path = updater.download_update()
            print(f"Update downloaded to: {download_path}")
            return 0

        elif args.verify_only:
            # Check, download and verify
            update_available = updater.check_for_updates()
            if not update_available:
                print(
                    f"No updates available (current version: {config['current_version']})"
                )
                return 0

            download_path = updater.download_update()
            # Ensure download_path is a Path object
            if not isinstance(download_path, Path):
                download_path = Path(download_path)

            verified = updater.verify_update(download_path)
            if verified:
                print("Update verification successful")
                return 0
            else:
                print("Update verification failed")
                return 1

        else:
            # Full update process
            success = updater.update()
            if success and updater.update_info:
                print(
                    f"Update to version {updater.update_info.version} completed successfully"
                )
                return 0
            else:
                print("No updates installed")
                return 0

    except FileNotFoundError as e:
        print(f"Error: {e}")
        return 1
    except json.JSONDecodeError:
        print(f"Error: Config file is not valid JSON: {args.config}")
        return 1
    except UpdaterError as e:
        print(f"Error: {e}")
        return 1
    except Exception as e:
        print(f"Unexpected error: {e}")
        import traceback

        traceback.print_exc()
        return 1
    finally:
        if updater is not None:
            updater.cleanup()
