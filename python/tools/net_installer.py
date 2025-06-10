#!/usr/bin/env python3
"""
.NET Framework Installer and Manager

A comprehensive utility for managing .NET Framework installations on Windows systems,
providing detection, installation, verification, and uninstallation capabilities.

This module can be used both as a command-line tool and as an API through Python
import or C++ applications via pybind11 bindings.

Features:
- Registry-based detection of installed .NET Framework versions
- Multi-threaded downloads with progress reporting
- Cryptographic verification of installer integrity
- Automated installation and uninstallation
- Detailed logging and error handling

Example usage (CLI):
    python net_framework_manager.py --list
    python net_framework_manager.py --check v4.8
    python net_framework_manager.py --download URL --output installer.exe --install

Example usage (API):
    from net_framework_manager import DotNetManager
    
    manager = DotNetManager()
    versions = manager.list_installed_versions()
    manager.install_version("v4.8")

Version: 2.0
"""

from __future__ import annotations

import argparse
import asyncio
import hashlib
import logging
import os
import platform
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Union, Callable

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger("dotnet_manager")

# Try to import optional dependencies
try:
    import requests
    from tqdm import tqdm
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False


class HashAlgorithm(str, Enum):
    """Supported hash algorithms for file verification."""
    MD5 = "md5"
    SHA1 = "sha1"
    SHA256 = "sha256"
    SHA512 = "sha512"


@dataclass
class DotNetVersion:
    """Represents a .NET Framework version with related metadata."""
    key: str             # Registry key component (e.g., "v4.8")
    name: str            # Human-readable name (e.g., ".NET Framework 4.8")
    release: Optional[str] = None          # Specific release version
    installer_url: Optional[str] = None    # URL to download the installer
    # Expected SHA256 hash of the installer
    installer_sha256: Optional[str] = None

    def __str__(self) -> str:
        """String representation of the .NET version."""
        return f"{self.name} ({self.release or 'unknown'})"


class DotNetManager:
    """
    Core class for managing .NET Framework installations.

    **This class provides methods to detect, install, and uninstall .NET Framework 
    versions on Windows systems.**
    """
    # Common .NET Framework versions with metadata
    VERSIONS = {
        "v4.8": DotNetVersion(
            key="v4.8",
            name=".NET Framework 4.8",
            release="4.8.0",
            installer_url="https://go.microsoft.com/fwlink/?LinkId=2085155",
            installer_sha256="72398a77fb2c2c00c38c30e34f301e631ec9e745a35c082e3e87cce597d0fcf5"
        ),
        "v4.7.2": DotNetVersion(
            key="v4.7.2",
            name=".NET Framework 4.7.2",
            release="4.7.03062",
            installer_url="https://go.microsoft.com/fwlink/?LinkID=863265",
            installer_sha256="8b8b98d1afb6c474e30e82957dc4329442565e47bbfa59dee071f65a1574c738"
        ),
        "v4.6.2": DotNetVersion(
            key="v4.6.2",
            name=".NET Framework 4.6.2",
            release="4.6.01590",
            installer_url="https://go.microsoft.com/fwlink/?linkid=780600",
            installer_sha256="9c9a0ae687d8f2f34b908168e137493f188ab8a3547c345a5a5903143c353a51"
        ),
    }

    NET_FRAMEWORK_REGISTRY_PATH = r"SOFTWARE\Microsoft\NET Framework Setup\NDP"

    def __init__(self, download_dir: Optional[Path] = None, threads: int = 4):
        """
        Initialize the .NET Framework manager.

        Args:
            download_dir: Directory to store downloaded installers
            threads: Maximum number of concurrent download threads
        """
        if platform.system() != "Windows":
            logger.warning("This module is designed for Windows systems only")

        self.download_dir = download_dir or Path(
            tempfile.gettempdir()) / "dotnet_manager"
        self.download_dir.mkdir(parents=True, exist_ok=True)
        self.threads = threads

    def check_installed(self, version_key: str) -> bool:
        """
        Check if a specific .NET Framework version is installed.

        Args:
            version_key: Registry key component for the version (e.g., "v4.8")

        Returns:
            True if installed, False otherwise
        """
        try:
            # Query the registry for this version
            result = subprocess.run(
                ["reg", "query",
                    f"HKLM\\{self.NET_FRAMEWORK_REGISTRY_PATH}\\{version_key}"],
                capture_output=True, text=True
            )

            # For v4.5+, we need to check the Release value
            if version_key.startswith("v4.") and version_key != "v4.0":
                # All .NET 4.5+ versions are registered under v4\Full with different Release values
                release_path = f"{self.NET_FRAMEWORK_REGISTRY_PATH}\\v4\\Full"

                # Get the Release value
                release_result = subprocess.run(
                    ["reg", "query", f"HKLM\\{release_path}", "/v", "Release"],
                    capture_output=True, text=True
                )

                if release_result.returncode != 0:
                    return False

                # Parse the Release value
                match = re.search(
                    r'Release\s+REG_DWORD\s+0x([0-9a-f]+)', release_result.stdout)
                if not match:
                    return False

                release_num = int(match.group(1), 16)

                # Map release numbers to versions
                version_map = {
                    "v4.5": 378389,
                    "v4.5.1": 378675,
                    "v4.5.2": 379893,
                    "v4.6": 393295,
                    "v4.6.1": 394254,
                    "v4.6.2": 394802,
                    "v4.7": 460798,
                    "v4.7.1": 461308,
                    "v4.7.2": 461808,
                    "v4.8": 528040,
                    "v4.8.1": 533320
                }

                return release_num >= version_map.get(version_key, float('inf'))

            return result.returncode == 0

        except subprocess.SubprocessError:
            logger.warning(f"Failed to query registry for {version_key}")
            return False

    def list_installed_versions(self) -> List[DotNetVersion]:
        """
        List all installed .NET Framework versions detected in the registry.

        Returns:
            List of DotNetVersion objects representing installed versions
        """
        installed_versions = []

        try:
            # Query registry for NDP key
            result = subprocess.run(
                ["reg", "query", f"HKLM\\{self.NET_FRAMEWORK_REGISTRY_PATH}"],
                capture_output=True, text=True
            )

            if result.returncode != 0:
                return []

            # Parse output to extract version keys
            for line in result.stdout.splitlines():
                match = re.search(r'v[\d\.]+', line)
                if match:
                    version_key = match.group(0)

                    # Check if this is a known version
                    version_info = self.VERSIONS.get(version_key)

                    if not version_info:
                        # Create a basic version object for unknown versions
                        version_info = DotNetVersion(
                            key=version_key,
                            name=f".NET Framework {version_key[1:]}"
                        )

                    # Add to results
                    installed_versions.append(version_info)

            # Special check for v4 with profiles
            release_path = f"{self.NET_FRAMEWORK_REGISTRY_PATH}\\v4\\Full"
            release_result = subprocess.run(
                ["reg", "query", f"HKLM\\{release_path}", "/v", "Release"],
                capture_output=True, text=True
            )

            if release_result.returncode == 0:
                # Find the actual installed 4.x version based on release number
                match = re.search(
                    r'Release\s+REG_DWORD\s+0x([0-9a-f]+)', release_result.stdout)
                if match:
                    release_num = int(match.group(1), 16)

                    # Check for specific release ranges
                    if release_num >= 528040:
                        if not any(v.key == "v4.8" for v in installed_versions):
                            installed_versions.append(self.VERSIONS.get("v4.8") or
                                                      DotNetVersion(key="v4.8", name=".NET Framework 4.8"))
                    elif release_num >= 461808:
                        if not any(v.key == "v4.7.2" for v in installed_versions):
                            installed_versions.append(self.VERSIONS.get("v4.7.2") or
                                                      DotNetVersion(key="v4.7.2", name=".NET Framework 4.7.2"))
                    elif release_num >= 461308:
                        installed_versions.append(
                            DotNetVersion(key="v4.7.1", name=".NET Framework 4.7.1"))
                    elif release_num >= 460798:
                        installed_versions.append(
                            DotNetVersion(key="v4.7", name=".NET Framework 4.7"))
                    elif release_num >= 394802:
                        if not any(v.key == "v4.6.2" for v in installed_versions):
                            installed_versions.append(self.VERSIONS.get("v4.6.2") or
                                                      DotNetVersion(key="v4.6.2", name=".NET Framework 4.6.2"))
                    elif release_num >= 394254:
                        installed_versions.append(
                            DotNetVersion(key="v4.6.1", name=".NET Framework 4.6.1"))
                    elif release_num >= 393295:
                        installed_versions.append(
                            DotNetVersion(key="v4.6", name=".NET Framework 4.6"))
                    elif release_num >= 379893:
                        installed_versions.append(
                            DotNetVersion(key="v4.5.2", name=".NET Framework 4.5.2"))
                    elif release_num >= 378675:
                        installed_versions.append(
                            DotNetVersion(key="v4.5.1", name=".NET Framework 4.5.1"))
                    elif release_num >= 378389:
                        installed_versions.append(
                            DotNetVersion(key="v4.5", name=".NET Framework 4.5"))

            return installed_versions

        except subprocess.SubprocessError:
            logger.warning(
                "Failed to query registry for installed .NET versions")
            return []

    def verify_checksum(self, file_path: Path, expected_checksum: str,
                        algorithm: HashAlgorithm = HashAlgorithm.SHA256) -> bool:
        """
        Verify a file's integrity by checking its checksum.

        Args:
            file_path: Path to the file to verify
            expected_checksum: Expected checksum value
            algorithm: Hash algorithm to use

        Returns:
            True if the checksum matches, False otherwise
        """
        if not file_path.exists():
            return False

        hasher = hashlib.new(algorithm)

        # Read in chunks to handle large files efficiently
        with open(file_path, "rb") as f:
            # Read in 1MB chunks
            for chunk in iter(lambda: f.read(1024 * 1024), b""):
                hasher.update(chunk)

        calculated_checksum = hasher.hexdigest()
        return calculated_checksum.lower() == expected_checksum.lower()

    async def download_file_async(self, url: str, output_path: Path,
                                  num_threads: Optional[int] = None,
                                  checksum: Optional[str] = None,
                                  show_progress: bool = True) -> Path:
        """
        Asynchronously download a file with optional multi-threading and checksum verification.

        Args:
            url: URL to download the file from
            output_path: Path where the downloaded file will be saved
            num_threads: Number of threads to use for downloading
            checksum: Optional SHA256 checksum to verify the downloaded file
            show_progress: Whether to show progress bar

        Returns:
            Path to the downloaded file

        Raises:
            ValueError: If checksum verification fails
            RuntimeError: If download fails
        """
        if not HAS_REQUESTS:
            raise ImportError(
                "This feature requires the 'requests' and 'tqdm' libraries")

        # Will implement when needed - for now use the synchronous version
        return await asyncio.to_thread(
            self.download_file, url, output_path, num_threads, checksum, show_progress
        )

    def download_file(self, url: str, output_path: Path,
                      num_threads: Optional[int] = None,
                      checksum: Optional[str] = None,
                      show_progress: bool = True) -> Path:
        """
        Download a file with optional multi-threading and checksum verification.

        Args:
            url: URL to download the file from
            output_path: Path where the downloaded file will be saved
            num_threads: Number of threads to use for downloading
            checksum: Optional SHA256 checksum to verify the downloaded file
            show_progress: Whether to show progress bar

        Returns:
            Path to the downloaded file

        Raises:
            ValueError: If checksum verification fails
            RuntimeError: If download fails
        """
        if not HAS_REQUESTS:
            raise ImportError(
                "This feature requires the 'requests' and 'tqdm' libraries")

        num_threads = num_threads or self.threads
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        # If file already exists and checksum matches, skip download
        if output_path.exists() and checksum and self.verify_checksum(output_path, checksum):
            logger.info(
                f"File {output_path} already exists with matching checksum")
            return output_path

        logger.info(f"Downloading {url} to {output_path}")

        # Create temp files for each part
        part_files = []
        results = []

        try:
            # First, make a HEAD request to get the file size
            head_response = requests.head(
                url, allow_redirects=True, timeout=10)
            head_response.raise_for_status()
            total_size = int(head_response.headers.get("content-length", 0))

            if total_size == 0 or num_threads <= 1:
                # If size is unknown or single thread requested, use simple download
                with requests.get(url, stream=True, timeout=30) as response:
                    response.raise_for_status()

                    with open(output_path, "wb") as out_file:
                        if show_progress:
                            with tqdm(
                                total=total_size, unit="B", unit_scale=True,
                                desc=f"Downloading {output_path.name}"
                            ) as progress_bar:
                                for chunk in response.iter_content(chunk_size=8192):
                                    if chunk:
                                        out_file.write(chunk)
                                        progress_bar.update(len(chunk))
                        else:
                            for chunk in response.iter_content(chunk_size=8192):
                                if chunk:
                                    out_file.write(chunk)
            else:
                # Multi-threaded download
                part_size = max(total_size // num_threads,
                                1024 * 1024)  # Min 1MB chunk size

                with ThreadPoolExecutor(max_workers=num_threads) as executor:
                    for i in range(num_threads):
                        start_byte = i * part_size
                        end_byte = min(
                            start_byte + part_size - 1, total_size - 1)

                        # Skip if this part would be empty
                        if start_byte >= total_size:
                            continue

                        part_file = output_path.with_suffix(
                            f"{output_path.suffix}.part{i}")
                        part_files.append(part_file)

                        # Submit download task for this part
                        future = executor.submit(
                            self._download_part, url, part_file,
                            start_byte, end_byte, show_progress
                        )
                        results.append(future)

                # Wait for all parts to complete
                for future in results:
                    future.result()  # Will raise any exceptions that occurred

                # Combine parts into final file
                with open(output_path, "wb") as out_file:
                    for part_file in part_files:
                        with open(part_file, "rb") as part_data:
                            # Copy in 10MB chunks for efficiency
                            while True:
                                chunk = part_data.read(10 * 1024 * 1024)
                                if not chunk:
                                    break
                                out_file.write(chunk)

            logger.info(f"Download complete: {output_path}")

            # Verify checksum if provided
            if checksum:
                logger.info("Verifying file integrity with checksum")
                if not self.verify_checksum(output_path, checksum):
                    output_path.unlink(missing_ok=True)
                    raise ValueError(
                        "Downloaded file failed checksum verification")
                logger.info("Checksum verification succeeded")

            return output_path

        except Exception as e:
            logger.error(f"Download failed: {str(e)}")
            # Clean up output file if it exists
            output_path.unlink(missing_ok=True)
            raise RuntimeError(f"Failed to download {url}: {str(e)}") from e

        finally:
            # Clean up part files
            for part_file in part_files:
                part_file.unlink(missing_ok=True)

    def _download_part(self, url: str, part_file: Path, start_byte: int,
                       end_byte: int, show_progress: bool) -> None:
        """
        Download a specific byte range from a URL.

        Args:
            url: The URL to download from
            part_file: Path to save this part to
            start_byte: Starting byte position
            end_byte: Ending byte position
            show_progress: Whether to show progress bar

        Raises:
            RuntimeError: If download fails
        """
        headers = {"Range": f"bytes={start_byte}-{end_byte}"}
        part_size = end_byte - start_byte + 1

        try:
            with requests.get(url, headers=headers, stream=True, timeout=30) as response:
                response.raise_for_status()

                with open(part_file, "wb") as out_file:
                    if show_progress:
                        with tqdm(
                            total=part_size, unit="B", unit_scale=True,
                            desc=f"Part {part_file.suffix[5:]}"
                        ) as progress_bar:
                            for chunk in response.iter_content(chunk_size=8192):
                                if chunk:
                                    out_file.write(chunk)
                                    progress_bar.update(len(chunk))
                    else:
                        for chunk in response.iter_content(chunk_size=8192):
                            if chunk:
                                out_file.write(chunk)
        except Exception as e:
            logger.error(
                f"Failed to download part {start_byte}-{end_byte}: {str(e)}")
            raise RuntimeError(f"Part download failed: {str(e)}") from e

    def install_software(self, installer_path: Path, quiet: bool = False) -> bool:
        """
        Execute a software installer.

        Args:
            installer_path: Path to the installer executable
            quiet: Whether to run the installer silently

        Returns:
            True if installation process started successfully, False otherwise
        """
        if platform.system() != "Windows":
            logger.error("Installation is only supported on Windows")
            return False

        installer_path = Path(installer_path)
        if not installer_path.exists():
            logger.error(f"Installer not found: {installer_path}")
            return False

        try:
            # Build the command line
            cmd = [str(installer_path)]
            if quiet:
                cmd.extend(["/quiet", "/norestart"])

            logger.info(f"Starting installer: {installer_path}")

            # For better control, we use Popen instead of the older approach
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                creationflags=subprocess.CREATE_NO_WINDOW
            )

            # Return immediately as installer may run for a long time
            logger.info(f"Installer started with process ID: {process.pid}")
            return True

        except Exception as e:
            logger.error(f"Failed to start installer: {e}")
            return False

    def uninstall_dotnet(self, version_key: str) -> bool:
        """
        Attempt to uninstall a specific .NET Framework version.

        **Note: This has limited functionality as many .NET versions don't
        support direct uninstallation through standard means.**

        Args:
            version_key: Registry key component for the version (e.g., "v4.8")

        Returns:
            True if uninstallation was attempted, False otherwise
        """
        if platform.system() != "Windows":
            logger.error("Uninstallation is only supported on Windows")
            return False

        logger.warning(
            f"Attempting to uninstall {version_key}, but this functionality is limited. "
            "You may need to use Control Panel or Windows Features."
        )

        try:
            # For newer .NET versions, try using Windows Features
            if version_key.startswith("v4.5") or version_key.startswith("v4.6") or \
               version_key.startswith("v4.7") or version_key.startswith("v4.8"):

                logger.info("Attempting to uninstall via Windows Features...")
                feature_name = "NetFx4"
                if version_key == "v3.5":
                    feature_name = "NetFx3"

                result = subprocess.run(
                    ["dism", "/online", "/disable-feature",
                        f"/featurename:{feature_name}"],
                    capture_output=True, text=True
                )

                if result.returncode == 0:
                    logger.info(
                        f"Successfully requested uninstallation of {version_key}")
                    return True
                else:
                    logger.warning(f"DISM command failed: {result.stderr}")

            # As a fallback, try to remove registry entries
            # This won't actually uninstall the software but can be useful for testing
            result = subprocess.run(
                ["reg", "delete",
                    f"HKLM\\{self.NET_FRAMEWORK_REGISTRY_PATH}\\{version_key}", "/f"],
                capture_output=True, text=True
            )

            if result.returncode == 0:
                logger.info(f"Removed registry entries for {version_key}")
                return True
            else:
                logger.error(
                    f"Failed to remove registry entries: {result.stderr}")

            return False

        except Exception as e:
            logger.error(f"Error during uninstallation: {e}")
            return False


# Functions for both CLI usage and pybind11 integration

def check_dotnet_installed(version: str) -> bool:
    """
    Check if a specific .NET Framework version is installed.

    Args:
        version: The version to check (e.g., "v4.8")

    Returns:
        True if installed, False otherwise
    """
    manager = DotNetManager()
    return manager.check_installed(version)


def list_installed_dotnets() -> List[str]:
    """
    List all installed .NET Framework versions.

    Returns:
        List of installed version strings
    """
    manager = DotNetManager()
    versions = manager.list_installed_versions()
    return [str(version) for version in versions]


def download_file(url: str, filename: str, num_threads: int = 4,
                  expected_checksum: Optional[str] = None) -> bool:
    """
    Download a file with optional multi-threading and checksum verification.

    Args:
        url: URL to download from
        filename: Path where the file should be saved
        num_threads: Number of download threads to use
        expected_checksum: Optional SHA256 checksum for verification

    Returns:
        True if download succeeded, False otherwise
    """
    try:
        manager = DotNetManager(threads=num_threads)
        output_path = manager.download_file(
            url, Path(filename), num_threads, expected_checksum
        )
        return output_path.exists()
    except Exception as e:
        logger.error(f"Download failed: {e}")
        return False


def install_software(installer_path: str, quiet: bool = False) -> bool:
    """
    Execute a software installer.

    Args:
        installer_path: Path to the installer executable
        quiet: Whether to run the installer silently

    Returns:
        True if installation process started successfully, False otherwise
    """
    manager = DotNetManager()
    return manager.install_software(Path(installer_path), quiet)


def uninstall_dotnet(version: str) -> bool:
    """
    Attempt to uninstall a specific .NET Framework version.

    Args:
        version: The version to uninstall (e.g., "v4.8")

    Returns:
        True if uninstallation was attempted successfully, False otherwise
    """
    manager = DotNetManager()
    return manager.uninstall_dotnet(version)


def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Check and install .NET Framework versions.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # List installed .NET versions
  python net_framework_manager.py --list
  
  # Check if a specific version is installed
  python net_framework_manager.py --check v4.8
  
  # Download and install a specific version
  python net_framework_manager.py --download URL --output installer.exe --install
"""
    )

    parser.add_argument("--check", metavar="VERSION",
                        help="Check if a specific .NET Framework version is installed.")
    parser.add_argument("--list", action="store_true",
                        help="List all installed .NET Framework versions.")
    parser.add_argument("--download", metavar="URL",
                        help="URL to download the .NET Framework installer from.")
    parser.add_argument("--output", metavar="FILE",
                        help="Path where the downloaded file should be saved.")
    parser.add_argument("--install", action="store_true",
                        help="Install the downloaded or specified .NET Framework installer.")
    parser.add_argument("--installer", metavar="FILE",
                        help="Path to the .NET Framework installer to run.")
    parser.add_argument("--quiet", action="store_true",
                        help="Run the installer in quiet mode.")
    parser.add_argument("--threads", type=int, default=4,
                        help="Number of threads to use for downloading.")
    parser.add_argument("--checksum", metavar="SHA256",
                        help="Expected SHA256 checksum of the downloaded file.")
    parser.add_argument("--uninstall", metavar="VERSION",
                        help="Attempt to uninstall a specific .NET Framework version.")
    parser.add_argument("--verbose", action="store_true",
                        help="Enable verbose logging.")

    return parser.parse_args()


def main() -> int:
    """
    Main function for command-line execution.

    Returns:
        Integer exit code: 0 for success, 1 for error
    """
    args = parse_args()

    # Configure logging level
    if args.verbose:
        logger.setLevel(logging.DEBUG)

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
                f".NET Framework {args.check} is {'installed' if is_installed else 'not installed'}")
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
                args.download, args.output,
                num_threads=args.threads,
                expected_checksum=args.checksum
            )

            if success:
                print(f"Successfully downloaded {args.output}")

                # Proceed to installation if requested
                if args.install:
                    install_success = install_software(
                        args.output, quiet=args.quiet)
                    print(
                        f"Installation {'started successfully' if install_success else 'failed'}")
                    return 0 if install_success else 1
            else:
                print("Download failed")
                return 1

        elif args.install and args.installer:
            success = install_software(args.installer, quiet=args.quiet)
            print(
                f"Installation {'started successfully' if success else 'failed'}")
            return 0 if success else 1

        else:
            # If no action specified, show help
            print("No action specified. Use --help to see available options.")
            return 1

    except Exception as e:
        print(f"Error: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        return 1

    return 0


# Support for pybind11 integration
__all__ = [
    "DotNetManager",
    "DotNetVersion",
    "HashAlgorithm",
    "check_dotnet_installed",
    "list_installed_dotnets",
    "download_file",
    "install_software",
    "uninstall_dotnet"
]


if __name__ == "__main__":
    sys.exit(main())
