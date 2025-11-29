"""
PyBind11 adapter for .NET Manager utilities.

This module provides simplified interfaces for C++ bindings via pybind11,
with graceful degradation for non-Windows platforms and comprehensive
error handling through structured result dictionaries.
"""

import platform
from pathlib import Path
from typing import List, Optional, Dict, Any
from loguru import logger
from .manager import DotNetManager
from .models import HashAlgorithm


class DotNetPyBindAdapter:
    """
    Adapter class to expose DotNetManager functionality to C++ via pybind11.

    This class provides simplified interfaces with:
    - Structured return types compatible with pybind11 (dict/list)
    - Graceful platform degradation (Windows-only module)
    - Comprehensive exception handling with error context
    - Both sync and async operation support
    """

    @staticmethod
    def _is_windows() -> bool:
        """Check if running on Windows platform."""
        return platform.system() == "Windows"

    @staticmethod
    def _not_supported_response(operation: str) -> Dict[str, Any]:
        """Generate a platform-not-supported response."""
        return {
            "success": False,
            "supported": False,
            "platform": platform.system(),
            "error": f"Operation '{operation}' is only supported on Windows platform",
            "operation": operation,
        }

    @staticmethod
    def check_dotnet_installed(version_key: str) -> Dict[str, Any]:
        """
        Check if a specific .NET Framework version is installed.

        Args:
            version_key: Registry key component for the version (e.g., "v4.8")

        Returns:
            dict: Structured result with keys:
                - success (bool): Operation succeeded
                - supported (bool): Platform is supported
                - installed (bool): .NET Framework version is installed
                - version_key (str): The queried version key
                - error (str, optional): Error message if operation failed
        """
        if not DotNetPyBindAdapter._is_windows():
            return DotNetPyBindAdapter._not_supported_response("check_dotnet_installed")

        try:
            logger.info(
                f"C++ binding: Checking if .NET Framework {version_key} is installed"
            )
            manager = DotNetManager()
            is_installed = manager.check_installed(version_key)

            return {
                "success": True,
                "supported": True,
                "installed": is_installed,
                "version_key": version_key,
            }
        except Exception as e:
            logger.exception(f"Error checking .NET installation for {version_key}: {e}")
            return {
                "success": False,
                "supported": True,
                "installed": False,
                "version_key": version_key,
                "error": str(e),
            }

    @staticmethod
    def list_installed_dotnets() -> Dict[str, Any]:
        """
        List all installed .NET Framework versions.

        Returns:
            dict: Structured result with keys:
                - success (bool): Operation succeeded
                - supported (bool): Platform is supported
                - versions (list): List of DotNetVersion objects (as dicts)
                - count (int): Number of installed versions
                - error (str, optional): Error message if operation failed
        """
        if not DotNetPyBindAdapter._is_windows():
            return {
                **DotNetPyBindAdapter._not_supported_response("list_installed_dotnets"),
                "versions": [],
                "count": 0,
            }

        try:
            logger.info("C++ binding: Listing installed .NET Framework versions")
            manager = DotNetManager()
            versions = manager.list_installed_versions()

            # Convert DotNetVersion objects to dict for pybind11 compatibility
            versions_list = [
                {
                    "key": v.key,
                    "name": v.name,
                    "release": v.release,
                    "installer_url": v.installer_url,
                    "installer_sha256": v.installer_sha256,
                }
                for v in versions
            ]

            return {
                "success": True,
                "supported": True,
                "versions": versions_list,
                "count": len(versions_list),
            }
        except Exception as e:
            logger.exception(f"Error listing installed .NET versions: {e}")
            return {
                "success": False,
                "supported": True,
                "versions": [],
                "count": 0,
                "error": str(e),
            }

    @staticmethod
    def download_file(
        url: str, output_path: str, num_threads: int = 4, checksum: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Download a file with optional multi-threading and checksum verification.

        Args:
            url: URL to download from
            output_path: Path where the file should be saved
            num_threads: Number of download threads to use (default: 4)
            checksum: Optional SHA256 checksum for verification

        Returns:
            dict: Structured result with keys:
                - success (bool): Operation succeeded
                - supported (bool): Platform is supported
                - file_path (str): Path to downloaded file
                - file_size (int): Size of downloaded file in bytes
                - checksum_verified (bool): Whether checksum was verified
                - error (str, optional): Error message if operation failed
        """
        if not DotNetPyBindAdapter._is_windows():
            return DotNetPyBindAdapter._not_supported_response("download_file")

        try:
            logger.info(f"C++ binding: Downloading {url} to {output_path}")
            manager = DotNetManager(threads=num_threads)
            output_file = manager.download_file(
                url, Path(output_path), num_threads=num_threads, checksum=checksum
            )

            file_size = output_file.stat().st_size if output_file.exists() else 0
            checksum_verified = checksum is not None and manager.verify_checksum(
                output_file, checksum
            )

            return {
                "success": True,
                "supported": True,
                "file_path": str(output_file),
                "file_size": file_size,
                "checksum_verified": checksum_verified,
            }
        except Exception as e:
            logger.exception(f"Error downloading file from {url}: {e}")
            return {
                "success": False,
                "supported": True,
                "file_path": output_path,
                "file_size": 0,
                "checksum_verified": False,
                "error": str(e),
            }

    @staticmethod
    def verify_checksum(
        file_path: str, expected_checksum: str, algorithm: str = "sha256"
    ) -> Dict[str, Any]:
        """
        Verify a file's integrity by checking its checksum.

        Args:
            file_path: Path to the file to verify
            expected_checksum: Expected checksum value
            algorithm: Hash algorithm to use (default: "sha256")

        Returns:
            dict: Structured result with keys:
                - success (bool): Operation succeeded
                - supported (bool): Platform is supported
                - verified (bool): Checksum matches
                - file_path (str): Path to the verified file
                - algorithm (str): Hash algorithm used
                - error (str, optional): Error message if operation failed
        """
        try:
            logger.info(f"C++ binding: Verifying checksum for {file_path}")

            # Validate algorithm
            try:
                hash_algo = HashAlgorithm(algorithm.lower())
            except ValueError:
                return {
                    "success": False,
                    "supported": True,
                    "verified": False,
                    "file_path": file_path,
                    "algorithm": algorithm,
                    "error": f"Unsupported hash algorithm: {algorithm}. Supported: md5, sha1, sha256, sha512",
                }

            manager = DotNetManager()
            file_path_obj = Path(file_path)

            if not file_path_obj.exists():
                return {
                    "success": False,
                    "supported": True,
                    "verified": False,
                    "file_path": file_path,
                    "algorithm": algorithm,
                    "error": f"File not found: {file_path}",
                }

            is_verified = manager.verify_checksum(
                file_path_obj, expected_checksum, hash_algo
            )

            return {
                "success": True,
                "supported": True,
                "verified": is_verified,
                "file_path": file_path,
                "algorithm": algorithm,
            }
        except Exception as e:
            logger.exception(f"Error verifying checksum for {file_path}: {e}")
            return {
                "success": False,
                "supported": True,
                "verified": False,
                "file_path": file_path,
                "algorithm": algorithm,
                "error": str(e),
            }

    @staticmethod
    def install_software(installer_path: str, quiet: bool = False) -> Dict[str, Any]:
        """
        Execute a software installer.

        Args:
            installer_path: Path to the installer executable
            quiet: Whether to run the installer silently (default: False)

        Returns:
            dict: Structured result with keys:
                - success (bool): Installation process started
                - supported (bool): Platform is supported
                - installer_path (str): Path to the installer
                - process_started (bool): Installer process started
                - error (str, optional): Error message if operation failed
        """
        if not DotNetPyBindAdapter._is_windows():
            return DotNetPyBindAdapter._not_supported_response("install_software")

        try:
            logger.info(f"C++ binding: Installing software from {installer_path}")
            manager = DotNetManager()
            result = manager.install_software(Path(installer_path), quiet=quiet)

            return {
                "success": result,
                "supported": True,
                "installer_path": installer_path,
                "process_started": result,
                "quiet_mode": quiet,
            }
        except Exception as e:
            logger.exception(f"Error installing software from {installer_path}: {e}")
            return {
                "success": False,
                "supported": True,
                "installer_path": installer_path,
                "process_started": False,
                "quiet_mode": quiet,
                "error": str(e),
            }

    @staticmethod
    def uninstall_dotnet(version_key: str) -> Dict[str, Any]:
        """
        Attempt to uninstall a specific .NET Framework version.

        Note: Most .NET Framework versions do not support direct uninstallation.

        Args:
            version_key: Registry key component for the version (e.g., "v4.8")

        Returns:
            dict: Structured result with keys:
                - success (bool): Uninstallation attempted
                - supported (bool): Platform is supported
                - version_key (str): The version attempted to uninstall
                - uninstall_attempted (bool): Whether uninstall was attempted
                - error (str, optional): Error message or uninstall limitation
        """
        if not DotNetPyBindAdapter._is_windows():
            return DotNetPyBindAdapter._not_supported_response("uninstall_dotnet")

        try:
            logger.info(
                f"C++ binding: Attempting to uninstall .NET Framework {version_key}"
            )
            manager = DotNetManager()
            result = manager.uninstall_dotnet(version_key)

            return {
                "success": result,
                "supported": True,
                "version_key": version_key,
                "uninstall_attempted": result,
            }
        except Exception as e:
            logger.exception(f"Error uninstalling .NET Framework {version_key}: {e}")
            return {
                "success": False,
                "supported": True,
                "version_key": version_key,
                "uninstall_attempted": False,
                "error": str(e),
            }

    @staticmethod
    def get_installed_versions_info() -> Dict[str, Any]:
        """
        Get detailed information about all installed .NET Framework versions.

        Returns:
            dict: Structured result with keys:
                - success (bool): Operation succeeded
                - supported (bool): Platform is supported
                - versions_info (list): List of version information dicts
                - total_count (int): Total number of installed versions
                - error (str, optional): Error message if operation failed
        """
        if not DotNetPyBindAdapter._is_windows():
            return {
                **DotNetPyBindAdapter._not_supported_response(
                    "get_installed_versions_info"
                ),
                "versions_info": [],
                "total_count": 0,
            }

        try:
            logger.info(
                "C++ binding: Getting detailed installed .NET versions information"
            )
            manager = DotNetManager()
            versions = manager.list_installed_versions()

            versions_info = [
                {
                    "key": v.key,
                    "name": v.name,
                    "release": v.release or "unknown",
                    "installer_url": v.installer_url,
                    "has_installer_url": bool(v.installer_url),
                    "has_checksum": bool(v.installer_sha256),
                    "installed": manager.check_installed(v.key),
                }
                for v in versions
            ]

            return {
                "success": True,
                "supported": True,
                "versions_info": versions_info,
                "total_count": len(versions_info),
            }
        except Exception as e:
            logger.exception(f"Error getting installed versions information: {e}")
            return {
                "success": False,
                "supported": True,
                "versions_info": [],
                "total_count": 0,
                "error": str(e),
            }
