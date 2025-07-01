"""Command-line interface for the .NET Framework Manager."""

import argparse
import asyncio
import sys
import traceback
import json

from loguru import logger

from .api import (
    get_system_info,
    check_dotnet_installed,
    list_installed_dotnets,
    download_file_async,
    verify_checksum_async,
    install_software,
    uninstall_dotnet,
    get_latest_known_version
)
from .models import DotNetVersion, SystemInfo, DownloadResult


def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="A modern tool for managing .NET Framework installations on Windows.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Get system and .NET installation overview:
  python -m python.tools.dotnet_manager info

  # Check if .NET 4.8 is installed:
  python -m python.tools.dotnet_manager check v4.8

  # Download and install the latest known .NET version:
  python -m python.tools.dotnet_manager install --latest

  # Verify a downloaded installer file:
  python -m python.tools.dotnet_manager verify C:\Downloads\installer.exe --checksum <SHA256>
"""
    )
    subparsers = parser.add_subparsers(dest="command", required=True, help="Available commands")

    # Info command
    parser_info = subparsers.add_parser("info", help="Display system and .NET installation details.")
    parser_info.add_argument("--json", action="store_true", help="Output information in JSON format.")

    # Check command
    parser_check = subparsers.add_parser("check", help="Check if a specific .NET version is installed.")
    parser_check.add_argument("version", help="The version key to check (e.g., v4.8)")

    # List command
    parser_list = subparsers.add_parser("list", help="List all installed .NET versions.")
    parser_list.add_argument("--json", action="store_true", help="Output in JSON format.")

    # Install command
    parser_install = subparsers.add_parser("install", help="Download and install a .NET version.")
    install_group = parser_install.add_mutually_exclusive_group(required=True)
    install_group.add_argument("--version", help="The version key to install (e.g., v4.8)")
    install_group.add_argument("--latest", action="store_true", help="Install the latest known version.")
    parser_install.add_argument("--quiet", action="store_true", help="Run the installer silently.")

    # Download command
    parser_download = subparsers.add_parser("download", help="Download a .NET installer.")
    parser_download.add_argument("url", help="URL of the installer.")
    parser_download.add_argument("output", help="File path to save the installer.")
    parser_download.add_argument("--checksum", help="SHA256 checksum for verification.")

    # Verify command
    parser_verify = subparsers.add_parser("verify", help="Verify the checksum of a file.")
    parser_verify.add_argument("file", help="Path to the file to verify.")
    parser_verify.add_argument("--checksum", required=True, help="Expected SHA256 checksum.")

    # Uninstall command
    parser_uninstall = subparsers.add_parser("uninstall", help="Attempt to uninstall a .NET version.")
    parser_uninstall.add_argument("version", help="The version key to uninstall.")

    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose logging.")

    return parser.parse_args()


async def run_async_command(args):
    """Runs the specified asynchronous command."""
    if args.command == 'download':
        result = await download_file_async(args.url, args.output, args.checksum)
        print(f"Download successful: {result.path} ({result.size} bytes)")
    elif args.command == 'verify':
        is_valid = await verify_checksum_async(args.file, args.checksum)
        print(f"Checksum for {args.file} is {'valid' if is_valid else 'invalid'}.")
        return 0 if is_valid else 1
    elif args.command == 'install':
        version_to_install = get_latest_known_version() if args.latest else DotNetManager.VERSIONS.get(args.version)
        if not version_to_install or not version_to_install.installer_url:
            print(f"Error: Version {args.version or 'latest'} is not known or has no installer URL.")
            return 1
        
        output_path = DotNetManager().download_dir / f"dotnet_installer_{version_to_install.key}.exe"
        print(f"Downloading {version_to_install.name}...")
        await download_file_async(version_to_install.installer_url, str(output_path), version_to_install.installer_sha256)
        print("Download complete. Starting installation...")
        if install_software(str(output_path), args.quiet):
            print("Installation started successfully.")
        else:
            print("Installation failed to start.")
            return 1
    return 0

def main() -> int:
    """Main function for command-line execution."""
    args = parse_args()

    logger.remove()
    log_level = "DEBUG" if args.verbose else "INFO"
    logger.add(sys.stderr, level=log_level)

    try:
        if args.command in ['download', 'verify', 'install']:
            return asyncio.run(run_async_command(args))

        if args.command == 'info':
            info = get_system_info()
            if args.json:
                print(json.dumps(info, default=lambda o: o.__dict__, indent=2))
            else:
                print(f"OS: {info.os_name} {info.os_build} ({info.architecture})")
                print("Installed .NET Versions:")
                if info.installed_versions:
                    for v in info.installed_versions:
                        print(f"  - {v}")
                else:
                    print("  None detected.")
        
        elif args.command == 'check':
            is_installed = check_dotnet_installed(args.version)
            print(f".NET Framework {args.version} is {'installed' if is_installed else 'not installed'}.")
            return 0 if is_installed else 1

        elif args.command == 'list':
            versions = list_installed_dotnets()
            if args.json:
                print(json.dumps([v.__dict__ for v in versions], indent=2))
            else:
                if versions:
                    print("Installed .NET Framework versions:")
                    for v in versions:
                        print(f"  - {v}")
                else:
                    print("No .NET Framework versions detected.")

        elif args.command == 'uninstall':
            uninstall_dotnet(args.version)

    except Exception as e:
        logger.error(f"An error occurred: {e}")
        if args.verbose:
            traceback.print_exc()
        return 1

    return 0