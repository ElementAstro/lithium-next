"""Enhanced command-line interface for the .NET Framework Manager."""

from __future__ import annotations
import argparse
import asyncio
import json
import sys
import traceback
from pathlib import Path
from typing import Any, Optional

from loguru import logger

from .api import (
    get_system_info,
    check_dotnet_installed,
    list_installed_dotnets,
    list_available_dotnets,
    download_file_async,
    verify_checksum_async,
    install_software,
    uninstall_dotnet,
    get_latest_known_version,
    get_version_info,
    download_and_install_version,
)
from .models import (
    DotNetVersion,
    SystemInfo,
    DownloadResult,
    InstallationResult,
    DotNetManagerError,
    UnsupportedPlatformError,
)
from .manager import DotNetManager


def setup_logging(verbose: bool = False) -> None:
    """Configure enhanced logging with loguru."""
    logger.remove()

    log_level = "DEBUG" if verbose else "INFO"
    log_format = (
        "<green>{time:YYYY-MM-DD HH:mm:ss}</green> | "
        "<level>{level: <8}</level> | "
        "<cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> | "
        "<level>{message}</level>"
    )

    logger.add(
        sys.stderr, level=log_level, format=log_format, colorize=True, catch=True
    )

    if verbose:
        logger.debug("Verbose logging enabled")


def handle_json_output(data: Any, use_json: bool = False) -> None:
    """Handle JSON or human-readable output."""
    if use_json:
        # Convert objects to dictionaries for JSON serialization
        if hasattr(data, "to_dict"):
            json_data = data.to_dict()
        elif isinstance(data, list) and data and hasattr(data[0], "to_dict"):
            json_data = [item.to_dict() for item in data]
        elif isinstance(data, dict):
            json_data = data
        else:
            json_data = data

        print(json.dumps(json_data, indent=2, default=str))
    else:
        print(data)


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments with enhanced validation."""
    parser = argparse.ArgumentParser(
        description="Enhanced .NET Framework Manager with modern Python features",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Get comprehensive system and .NET installation overview:
  dotnet-manager info --json

  # Check if .NET 4.8 is installed:
  dotnet-manager check v4.8

  # List all available versions for download:
  dotnet-manager list-available

  # Download and install the latest .NET version:
  dotnet-manager install --latest --quiet

  # Download and install a specific version:
  dotnet-manager install --version v4.8

  # Download a file with checksum verification:
  dotnet-manager download https://example.com/file.exe output.exe --checksum <SHA256>

  # Verify a downloaded installer file:
  dotnet-manager verify installer.exe --checksum <SHA256>

  # Download and install in one command with cleanup:
  dotnet-manager download-install v4.8 --cleanup
""",
    )

    subparsers = parser.add_subparsers(
        dest="command",
        required=True,
        help="Available commands",
        metavar="{info,check,list,list-available,install,download,verify,uninstall,download-install}",
    )

    # Global options
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Enable verbose logging with debug information",
    )

    # Info command
    parser_info = subparsers.add_parser(
        "info", help="Display comprehensive system and .NET installation details"
    )
    parser_info.add_argument(
        "--json",
        action="store_true",
        help="Output information in JSON format for machine processing",
    )

    # Check command
    parser_check = subparsers.add_parser(
        "check", help="Check if a specific .NET version is installed"
    )
    parser_check.add_argument(
        "version", help="The version key to check (e.g., v4.8, v4.7.2)"
    )

    # List installed command
    parser_list = subparsers.add_parser(
        "list", help="List all installed .NET Framework versions"
    )
    parser_list.add_argument(
        "--json", action="store_true", help="Output in JSON format"
    )

    # List available command
    parser_list_available = subparsers.add_parser(
        "list-available", help="List all .NET versions available for download"
    )
    parser_list_available.add_argument(
        "--json", action="store_true", help="Output in JSON format"
    )

    # Install command
    parser_install = subparsers.add_parser(
        "install", help="Download and install a .NET Framework version"
    )
    install_group = parser_install.add_mutually_exclusive_group(required=True)
    install_group.add_argument(
        "--version", help="The version key to install (e.g., v4.8)"
    )
    install_group.add_argument(
        "--latest", action="store_true", help="Install the latest known version"
    )
    parser_install.add_argument(
        "--quiet",
        action="store_true",
        help="Run the installer silently without user interaction",
    )
    parser_install.add_argument(
        "--no-verify",
        action="store_true",
        help="Skip checksum verification during download",
    )
    parser_install.add_argument(
        "--no-cleanup",
        action="store_true",
        help="Keep the installer file after installation",
    )

    # Download command
    parser_download = subparsers.add_parser(
        "download", help="Download a .NET installer or any file"
    )
    parser_download.add_argument("url", help="URL of the file to download")
    parser_download.add_argument(
        "output", help="Local file path to save the downloaded file"
    )
    parser_download.add_argument(
        "--checksum", help="Expected SHA256 checksum for verification"
    )
    parser_download.add_argument(
        "--no-progress", action="store_true", help="Disable progress bar display"
    )

    # Verify command
    parser_verify = subparsers.add_parser(
        "verify", help="Verify the checksum of a file"
    )
    parser_verify.add_argument("file", help="Path to the file to verify")
    parser_verify.add_argument(
        "--checksum", required=True, help="Expected SHA256 checksum"
    )

    # Uninstall command
    parser_uninstall = subparsers.add_parser(
        "uninstall",
        help="Attempt to uninstall a .NET Framework version (not recommended)",
    )
    parser_uninstall.add_argument("version", help="The version key to uninstall")

    # Download and install command
    parser_download_install = subparsers.add_parser(
        "download-install", help="Download and install a .NET version in one operation"
    )
    parser_download_install.add_argument(
        "version", help="The version key to download and install (e.g., v4.8)"
    )
    parser_download_install.add_argument(
        "--quiet", action="store_true", help="Run the installer silently"
    )
    parser_download_install.add_argument(
        "--no-verify", action="store_true", help="Skip checksum verification"
    )
    parser_download_install.add_argument(
        "--cleanup", action="store_true", help="Delete the installer after installation"
    )

    return parser.parse_args()


async def run_async_command(args: argparse.Namespace) -> int:
    """Execute asynchronous commands with enhanced error handling."""
    try:
        match args.command:
            case "download":
                logger.info(f"Starting download: {args.url} -> {args.output}")

                result = await download_file_async(
                    args.url,
                    args.output,
                    args.checksum,
                    show_progress=not args.no_progress,
                )

                print(f"✅ Download successful!")
                print(f"   Path: {result.path}")
                print(f"   Size: {result.size_mb:.2f} MB")
                if result.checksum_matched is not None:
                    print(
                        f"   Checksum: {'✅ Verified' if result.checksum_matched else '❌ Failed'}"
                    )
                if result.download_time_seconds:
                    print(f"   Time: {result.download_time_seconds:.2f} seconds")
                if result.average_speed_mbps:
                    print(f"   Speed: {result.average_speed_mbps:.2f} MB/s")

                return 0 if result.success else 1

            case "verify":
                logger.info(f"Verifying checksum for: {args.file}")

                is_valid = await verify_checksum_async(args.file, args.checksum)

                if is_valid:
                    print(f"✅ Checksum verification passed for {args.file}")
                else:
                    print(f"❌ Checksum verification failed for {args.file}")

                return 0 if is_valid else 1

            case "install":
                version_key = args.version
                if args.latest:
                    latest_version = get_latest_known_version()
                    if not latest_version:
                        print("❌ No known .NET versions available")
                        return 1
                    version_key = latest_version.key

                version_info = get_version_info(version_key)
                if not version_info:
                    print(f"❌ Unknown version: {version_key}")
                    return 1

                if not version_info.is_downloadable:
                    print(f"❌ Version {version_key} is not available for download")
                    return 1

                print(f"📥 Downloading and installing {version_info.name}...")

                try:
                    download_result, install_result = download_and_install_version(
                        version_key,
                        quiet=args.quiet,
                        verify_checksum=not args.no_verify,
                        cleanup_installer=not args.no_cleanup,
                    )

                    print(f"✅ Download completed: {download_result.size_mb:.2f} MB")

                    if install_result.success:
                        print(f"✅ Installation completed successfully!")
                    else:
                        print(
                            f"❌ Installation failed (return code: {install_result.return_code})"
                        )
                        if install_result.error_message:
                            print(f"   Error: {install_result.error_message}")
                        return 1

                except Exception as e:
                    print(f"❌ Installation failed: {e}")
                    return 1

                return 0

            case _:
                print(f"❌ Unknown async command: {args.command}")
                return 1

    except UnsupportedPlatformError as e:
        print(f"❌ Platform Error: {e}")
        return 1
    except DotNetManagerError as e:
        print(f"❌ .NET Manager Error: {e}")
        if args.verbose and e.original_error:
            print(f"   Caused by: {e.original_error}")
        return 1
    except Exception as e:
        logger.error(f"Unexpected error in async command: {e}")
        if args.verbose:
            traceback.print_exc()
        return 1


def main() -> int:
    """Enhanced main function with comprehensive error handling."""
    args = parse_args()

    # Setup logging first
    setup_logging(args.verbose)

    try:
        # Handle async commands
        if args.command in ["download", "verify", "install"]:
            return asyncio.run(run_async_command(args))

        # Handle synchronous commands
        match args.command:
            case "info":
                logger.debug("Getting system information")
                info = get_system_info()

                if args.json:
                    handle_json_output(info, use_json=True)
                else:
                    print(f"🖥️  System Information")
                    print(
                        f"   OS: {info.os_name} {info.os_build} ({info.architecture})"
                    )
                    print(
                        f"   Platform Compatible: {'✅ Yes' if info.platform_compatible else '❌ No'}"
                    )
                    print()
                    print(
                        f"📦 Installed .NET Framework Versions ({info.installed_version_count}):"
                    )

                    if info.installed_versions:
                        for version in info.installed_versions:
                            print(f"   • {version}")

                        if info.latest_installed_version:
                            print()
                            print(
                                f"🏆 Latest Installed: {info.latest_installed_version.name}"
                            )
                    else:
                        print("   None detected")

            case "check":
                logger.debug(f"Checking installation status for: {args.version}")
                is_installed = check_dotnet_installed(args.version)

                status_icon = "✅" if is_installed else "❌"
                status_text = "installed" if is_installed else "not installed"
                print(f"{status_icon} .NET Framework {args.version} is {status_text}")

                return 0 if is_installed else 1

            case "list":
                logger.debug("Listing installed .NET versions")
                versions = list_installed_dotnets()

                if args.json:
                    handle_json_output(versions, use_json=True)
                else:
                    if versions:
                        print(
                            f"📦 Installed .NET Framework versions ({len(versions)}):"
                        )
                        for version in versions:
                            print(f"   • {version}")
                    else:
                        print("❌ No .NET Framework versions detected")

            case "list-available":
                logger.debug("Listing available .NET versions")
                versions = list_available_dotnets()

                if args.json:
                    handle_json_output(versions, use_json=True)
                else:
                    if versions:
                        print(
                            f"📥 Available .NET Framework versions for download ({len(versions)}):"
                        )
                        for version in versions:
                            download_icon = "📥" if version.is_downloadable else "❌"
                            print(f"   {download_icon} {version}")
                    else:
                        print("❌ No .NET Framework versions available for download")

            case "download-install":
                logger.info(f"Starting download and install for: {args.version}")

                try:
                    download_result, install_result = download_and_install_version(
                        args.version,
                        quiet=args.quiet,
                        verify_checksum=not args.no_verify,
                        cleanup_installer=args.cleanup,
                    )

                    print(f"✅ Download completed: {download_result.size_mb:.2f} MB")

                    if install_result.success:
                        print(f"✅ Installation completed successfully!")
                    else:
                        print(
                            f"❌ Installation failed (return code: {install_result.return_code})"
                        )
                        if install_result.error_message:
                            print(f"   Error: {install_result.error_message}")
                        return 1

                except Exception as e:
                    print(f"❌ Download and install failed: {e}")
                    if args.verbose:
                        traceback.print_exc()
                    return 1

            case "uninstall":
                logger.warning(f"Uninstall requested for: {args.version}")
                result = uninstall_dotnet(args.version)
                print(f"⚠️  Uninstall operation completed (not supported): {result}")

            case _:
                print(f"❌ Unknown command: {args.command}")
                return 1

    except UnsupportedPlatformError as e:
        print(f"❌ Platform Error: {e}")
        return 1
    except DotNetManagerError as e:
        print(f"❌ .NET Manager Error: {e}")
        if args.verbose:
            logger.error("Exception details:", exc_info=True)
        return 1
    except KeyboardInterrupt:
        print("\n⚠️  Operation cancelled by user")
        return 130
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        if args.verbose:
            traceback.print_exc()
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
