"""Command-line interface for the .NET Framework Manager."""

import argparse
import sys
import traceback

from loguru import logger

from .api import (
    check_dotnet_installed,
    list_installed_dotnets,
    download_file,
    install_software,
    uninstall_dotnet,
)


def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Check and install .NET Framework versions.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # List installed .NET versions
  python -m dotnet_manager --list

  # Check if a specific version is installed
  python -m dotnet_manager --check v4.8

  # Download and install a specific version
  python -m dotnet_manager --download URL --output installer.exe --install
""",
    )

    parser.add_argument(
        "--check",
        metavar="VERSION",
        help="Check if a specific .NET Framework version is installed.",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List all installed .NET Framework versions.",
    )
    parser.add_argument(
        "--download",
        metavar="URL",
        help="URL to download the .NET Framework installer from.",
    )
    parser.add_argument(
        "--output",
        metavar="FILE",
        help="Path where the downloaded file should be saved.",
    )
    parser.add_argument(
        "--install",
        action="store_true",
        help="Install the downloaded or specified .NET Framework installer.",
    )
    parser.add_argument(
        "--installer",
        metavar="FILE",
        help="Path to the .NET Framework installer to run.",
    )
    parser.add_argument(
        "--quiet", action="store_true", help="Run the installer in quiet mode."
    )
    parser.add_argument(
        "--threads",
        type=int,
        default=4,
        help="Number of threads to use for downloading.",
    )
    parser.add_argument(
        "--checksum",
        metavar="SHA256",
        help="Expected SHA256 checksum of the downloaded file.",
    )
    parser.add_argument(
        "--uninstall",
        metavar="VERSION",
        help="Attempt to uninstall a specific .NET Framework version.",
    )
    parser.add_argument(
        "--verbose", action="store_true", help="Enable verbose logging."
    )

    return parser.parse_args()


def main() -> int:
    """
    Main function for command-line execution.

    Returns:
        Integer exit code: 0 for success, 1 for error
    """
    args = parse_args()

    # Configure logging level with loguru
    logger.remove()
    log_level = "DEBUG" if args.verbose else "INFO"
    logger.add(sys.stderr, level=log_level)

    try:
        # Process commands
        if args.list:
            versions = list_installed_dotnets()
            if versions:
                print("Installed .NET Framework versions:")
                for version in versions:
                    print(f"  - {version}")
            else:
                print("No .NET Framework versions detected")

        elif args.check:
            is_installed = check_dotnet_installed(args.check)
            print(
                f".NET Framework {args.check} is {'installed' if is_installed else 'not installed'}"
            )
            return 0 if is_installed else 1

        elif args.uninstall:
            success = uninstall_dotnet(args.uninstall)
            print(f"Uninstallation {'succeeded' if success else 'failed'}")
            return 0 if success else 1

        elif args.download:
            if not args.output:
                print("Error: --output is required with --download")
                return 1

            success = download_file(
                args.download,
                args.output,
                num_threads=args.threads,
                expected_checksum=args.checksum,
            )

            if success:
                print(f"Successfully downloaded {args.output}")

                # Proceed to installation if requested
                if args.install:
                    install_success = install_software(args.output, quiet=args.quiet)
                    print(
                        f"Installation {'started successfully' if install_success else 'failed'}"
                    )
                    return 0 if install_success else 1
            else:
                print("Download failed")
                return 1

        elif args.install and args.installer:
            success = install_software(args.installer, quiet=args.quiet)
            print(f"Installation {'started successfully' if success else 'failed'}")
            return 0 if success else 1

        else:
            # If no action specified, show help
            print("No action specified. Use --help to see available options.")
            return 1

    except Exception as e:
        print(f"Error: {e}")
        if args.verbose:
            traceback.print_exc()
        return 1

    return 0
