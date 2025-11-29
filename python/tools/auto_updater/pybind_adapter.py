"""
PyBind11 adapter for Auto Updater utilities.

This module provides simplified interfaces for C++ bindings via pybind11.
All methods return dict/list types for compatibility with pybind11 bindings.
"""

import platform
from pathlib import Path
from typing import Dict, Any, Optional

from loguru import logger
from .sync import AutoUpdaterSync
from .utils import compare_versions, parse_version, calculate_file_hash
from .types import UpdaterConfig


class AutoUpdaterPyBindAdapter:
    """
    Adapter class to expose AutoUpdater functionality to C++ via pybind11.

    This class simplifies the interface and handles conversions between Python and C++.
    All return values are dict/list types for pybind11 compatibility.
    Exception handling is performed internally with structured error results.
    """

    @staticmethod
    def is_platform_supported(platform_name: Optional[str] = None) -> bool:
        """
        Check if the current platform is supported.

        Args:
            platform_name: Optional specific platform to check. If None, checks current platform.
                          Valid values: "windows", "linux", "macos"

        Returns:
            bool: True if platform is supported, False otherwise
        """
        supported_platforms = ["windows", "linux", "macos"]

        if platform_name:
            return platform_name.lower() in supported_platforms

        current_platform = platform.system().lower()
        if current_platform == "darwin":
            current_platform = "macos"

        return current_platform in supported_platforms

    @staticmethod
    def get_current_platform() -> str:
        """
        Get the current platform name.

        Returns:
            str: Platform name ("windows", "linux", or "macos")
        """
        current = platform.system().lower()
        if current == "darwin":
            return "macos"
        return current

    @staticmethod
    def check_for_updates(config_dict: Dict[str, Any]) -> Dict[str, Any]:
        """
        Check for updates synchronously (C++ binding compatible).

        Args:
            config_dict: Configuration dictionary with update check parameters

        Returns:
            Dict with keys:
                - success (bool): Whether the check succeeded
                - updates_available (bool): Whether updates are available
                - latest_version (str): Latest version if available
                - current_version (str): Current version
                - error (str): Error message if failed
        """
        logger.info("C++ binding: Checking for updates")
        try:
            config = (
                UpdaterConfig(**config_dict)
                if isinstance(config_dict, dict)
                else config_dict
            )
            updater = AutoUpdaterSync(config)
            updates_available = updater.check_for_updates()

            return {
                "success": True,
                "updates_available": updates_available,
                "latest_version": getattr(config, "latest_version", "unknown"),
                "current_version": getattr(config, "current_version", "unknown"),
                "error": None,
            }
        except Exception as e:
            logger.exception(f"Error in check_for_updates: {e}")
            return {
                "success": False,
                "updates_available": False,
                "latest_version": None,
                "current_version": None,
                "error": str(e),
            }

    @staticmethod
    def download_update(config_dict: Dict[str, Any]) -> Dict[str, Any]:
        """
        Download the update package synchronously (C++ binding compatible).

        Args:
            config_dict: Configuration dictionary with download parameters

        Returns:
            Dict with keys:
                - success (bool): Whether download succeeded
                - download_path (str): Path to downloaded file if successful
                - file_size (int): Size of downloaded file in bytes
                - error (str): Error message if failed
        """
        logger.info("C++ binding: Downloading update")
        try:
            config = (
                UpdaterConfig(**config_dict)
                if isinstance(config_dict, dict)
                else config_dict
            )
            updater = AutoUpdaterSync(config)
            download_path = updater.download_update()

            if download_path:
                file_size = Path(download_path).stat().st_size
                return {
                    "success": True,
                    "download_path": str(download_path),
                    "file_size": file_size,
                    "error": None,
                }
            else:
                return {
                    "success": False,
                    "download_path": None,
                    "file_size": 0,
                    "error": "Download returned empty path",
                }
        except Exception as e:
            logger.exception(f"Error in download_update: {e}")
            return {
                "success": False,
                "download_path": None,
                "file_size": 0,
                "error": str(e),
            }

    @staticmethod
    def verify_update(
        config_dict: Dict[str, Any], download_path: str
    ) -> Dict[str, Any]:
        """
        Verify downloaded update integrity (C++ binding compatible).

        Args:
            config_dict: Configuration dictionary
            download_path: Path to the downloaded update file

        Returns:
            Dict with keys:
                - success (bool): Whether verification succeeded
                - verified (bool): Whether file verification passed
                - file_hash (str): Calculated hash of the file
                - expected_hash (str): Expected hash from config
                - algorithm (str): Hash algorithm used
                - error (str): Error message if failed
        """
        logger.info(f"C++ binding: Verifying update at {download_path}")
        try:
            config = (
                UpdaterConfig(**config_dict)
                if isinstance(config_dict, dict)
                else config_dict
            )
            updater = AutoUpdaterSync(config)
            verified = updater.verify_update(download_path)

            file_path = Path(download_path)
            file_hash = calculate_file_hash(file_path, algorithm="sha256")

            return {
                "success": True,
                "verified": verified,
                "file_hash": file_hash,
                "expected_hash": getattr(config, "expected_hash", None),
                "algorithm": "sha256",
                "error": None,
            }
        except Exception as e:
            logger.exception(f"Error in verify_update: {e}")
            return {
                "success": False,
                "verified": False,
                "file_hash": None,
                "expected_hash": None,
                "algorithm": "sha256",
                "error": str(e),
            }

    @staticmethod
    def backup_installation(config_dict: Dict[str, Any]) -> Dict[str, Any]:
        """
        Create backup of current installation (C++ binding compatible).

        Args:
            config_dict: Configuration dictionary with backup parameters

        Returns:
            Dict with keys:
                - success (bool): Whether backup succeeded
                - backup_path (str): Path to backup directory
                - backup_size (int): Total size of backup in bytes
                - files_backed_up (int): Number of files backed up
                - error (str): Error message if failed
        """
        logger.info("C++ binding: Backing up current installation")
        try:
            config = (
                UpdaterConfig(**config_dict)
                if isinstance(config_dict, dict)
                else config_dict
            )
            updater = AutoUpdaterSync(config)
            backup_path = updater.backup_current_installation()

            if backup_path:
                backup_dir = Path(backup_path)
                backup_size = sum(
                    f.stat().st_size for f in backup_dir.rglob("*") if f.is_file()
                )
                file_count = len(list(backup_dir.rglob("*")))

                return {
                    "success": True,
                    "backup_path": str(backup_path),
                    "backup_size": backup_size,
                    "files_backed_up": file_count,
                    "error": None,
                }
            else:
                return {
                    "success": False,
                    "backup_path": None,
                    "backup_size": 0,
                    "files_backed_up": 0,
                    "error": "Backup operation returned empty path",
                }
        except Exception as e:
            logger.exception(f"Error in backup_installation: {e}")
            return {
                "success": False,
                "backup_path": None,
                "backup_size": 0,
                "files_backed_up": 0,
                "error": str(e),
            }

    @staticmethod
    def extract_update(
        config_dict: Dict[str, Any], download_path: str
    ) -> Dict[str, Any]:
        """
        Extract update package (C++ binding compatible).

        Args:
            config_dict: Configuration dictionary
            download_path: Path to the downloaded update file

        Returns:
            Dict with keys:
                - success (bool): Whether extraction succeeded
                - extract_path (str): Path to extracted files
                - files_extracted (int): Number of files extracted
                - error (str): Error message if failed
        """
        logger.info(f"C++ binding: Extracting update from {download_path}")
        try:
            config = (
                UpdaterConfig(**config_dict)
                if isinstance(config_dict, dict)
                else config_dict
            )
            updater = AutoUpdaterSync(config)
            extract_path = updater.extract_update(download_path)

            if extract_path:
                extract_dir = Path(extract_path)
                file_count = len(list(extract_dir.rglob("*")))

                return {
                    "success": True,
                    "extract_path": str(extract_path),
                    "files_extracted": file_count,
                    "error": None,
                }
            else:
                return {
                    "success": False,
                    "extract_path": None,
                    "files_extracted": 0,
                    "error": "Extraction returned empty path",
                }
        except Exception as e:
            logger.exception(f"Error in extract_update: {e}")
            return {
                "success": False,
                "extract_path": None,
                "files_extracted": 0,
                "error": str(e),
            }

    @staticmethod
    def install_update(
        config_dict: Dict[str, Any], extract_path: str
    ) -> Dict[str, Any]:
        """
        Install extracted update (C++ binding compatible).

        Args:
            config_dict: Configuration dictionary
            extract_path: Path to extracted update files

        Returns:
            Dict with keys:
                - success (bool): Whether installation succeeded
                - files_installed (int): Number of files installed
                - installation_path (str): Path where update was installed
                - error (str): Error message if failed
        """
        logger.info(f"C++ binding: Installing update from {extract_path}")
        try:
            config = (
                UpdaterConfig(**config_dict)
                if isinstance(config_dict, dict)
                else config_dict
            )
            updater = AutoUpdaterSync(config)
            installed = updater.install_update(extract_path)

            if installed:
                extract_dir = Path(extract_path)
                file_count = len(list(extract_dir.rglob("*")))
                install_dir = getattr(config, "install_path", str(extract_path))

                return {
                    "success": True,
                    "files_installed": file_count,
                    "installation_path": install_dir,
                    "error": None,
                }
            else:
                return {
                    "success": False,
                    "files_installed": 0,
                    "installation_path": None,
                    "error": "Installation failed",
                }
        except Exception as e:
            logger.exception(f"Error in install_update: {e}")
            return {
                "success": False,
                "files_installed": 0,
                "installation_path": None,
                "error": str(e),
            }

    @staticmethod
    def rollback_update(
        config_dict: Dict[str, Any], backup_path: str
    ) -> Dict[str, Any]:
        """
        Rollback to previous version from backup (C++ binding compatible).

        Args:
            config_dict: Configuration dictionary
            backup_path: Path to backup directory

        Returns:
            Dict with keys:
                - success (bool): Whether rollback succeeded
                - files_restored (int): Number of files restored
                - error (str): Error message if failed
        """
        logger.info(f"C++ binding: Rolling back from backup {backup_path}")
        try:
            config = (
                UpdaterConfig(**config_dict)
                if isinstance(config_dict, dict)
                else config_dict
            )
            updater = AutoUpdaterSync(config)
            rolled_back = updater.rollback(backup_path)

            if rolled_back:
                backup_dir = Path(backup_path)
                file_count = len(list(backup_dir.rglob("*")))

                return {"success": True, "files_restored": file_count, "error": None}
            else:
                return {
                    "success": False,
                    "files_restored": 0,
                    "error": "Rollback operation failed",
                }
        except Exception as e:
            logger.exception(f"Error in rollback_update: {e}")
            return {"success": False, "files_restored": 0, "error": str(e)}

    @staticmethod
    def perform_update(config_dict: Dict[str, Any]) -> Dict[str, Any]:
        """
        Perform complete update process (C++ binding compatible).

        This is an async version that returns immediately with a status dict.
        For full blocking update, use run_update instead.

        Args:
            config_dict: Configuration dictionary with all update parameters

        Returns:
            Dict with keys:
                - success (bool): Whether update process started successfully
                - status (str): Current status of the update process
                - error (str): Error message if failed to start
        """
        logger.info("C++ binding: Performing complete update")
        try:
            config = (
                UpdaterConfig(**config_dict)
                if isinstance(config_dict, dict)
                else config_dict
            )
            updater = AutoUpdaterSync(config)
            success = updater.update()

            return {
                "success": success,
                "status": "completed" if success else "failed",
                "error": None,
            }
        except Exception as e:
            logger.exception(f"Error in perform_update: {e}")
            return {"success": False, "status": "error", "error": str(e)}

    @staticmethod
    def cleanup_update(config_dict: Dict[str, Any]) -> Dict[str, Any]:
        """
        Clean up temporary files from update process (C++ binding compatible).

        Args:
            config_dict: Configuration dictionary

        Returns:
            Dict with keys:
                - success (bool): Whether cleanup succeeded
                - space_freed (int): Bytes of space freed
                - error (str): Error message if failed
        """
        logger.info("C++ binding: Cleaning up update temporary files")
        try:
            config = (
                UpdaterConfig(**config_dict)
                if isinstance(config_dict, dict)
                else config_dict
            )
            updater = AutoUpdaterSync(config)
            updater.cleanup()

            return {
                "success": True,
                "space_freed": 0,  # Would require tracking before/after
                "error": None,
            }
        except Exception as e:
            logger.exception(f"Error in cleanup_update: {e}")
            return {"success": False, "space_freed": 0, "error": str(e)}

    @staticmethod
    def compare_version_strings(current: str, latest: str) -> Dict[str, Any]:
        """
        Compare two version strings (C++ binding compatible).

        Args:
            current: Current version string (e.g., "1.2.3")
            latest: Latest version string (e.g., "1.3.0")

        Returns:
            Dict with keys:
                - current (str): Current version
                - latest (str): Latest version
                - comparison (int): -1 if current < latest, 0 if equal, 1 if current > latest
                - needs_update (bool): True if current < latest
                - status (str): Human-readable status ("up_to_date", "update_available", "newer_installed")
        """
        logger.info(f"C++ binding: Comparing versions {current} vs {latest}")
        try:
            result = compare_versions(current, latest)

            status = (
                "up_to_date"
                if result == 0
                else "update_available" if result < 0 else "newer_installed"
            )

            return {
                "current": current,
                "latest": latest,
                "comparison": result,
                "needs_update": result < 0,
                "status": status,
            }
        except Exception as e:
            logger.exception(f"Error in compare_version_strings: {e}")
            return {
                "current": current,
                "latest": latest,
                "comparison": 0,
                "needs_update": False,
                "status": "error",
            }

    @staticmethod
    def parse_version_string(version_str: str) -> Dict[str, Any]:
        """
        Parse a version string into components (C++ binding compatible).

        Args:
            version_str: Version string (e.g., "1.2.3")

        Returns:
            Dict with keys:
                - version_string (str): Original version string
                - components (list): List of version components as integers
                - major (int): Major version component
                - minor (int): Minor version component (0 if not present)
                - patch (int): Patch version component (0 if not present)
        """
        logger.info(f"C++ binding: Parsing version string {version_str}")
        try:
            components = parse_version(version_str)

            return {
                "version_string": version_str,
                "components": list(components),
                "major": components[0] if len(components) > 0 else 0,
                "minor": components[1] if len(components) > 1 else 0,
                "patch": components[2] if len(components) > 2 else 0,
            }
        except Exception as e:
            logger.exception(f"Error in parse_version_string: {e}")
            return {
                "version_string": version_str,
                "components": [],
                "major": 0,
                "minor": 0,
                "patch": 0,
            }

    @staticmethod
    def calculate_hash(file_path: str, algorithm: str = "sha256") -> Dict[str, Any]:
        """
        Calculate file hash (C++ binding compatible).

        Args:
            file_path: Path to the file to hash
            algorithm: Hash algorithm ("sha256", "sha512", or "md5")

        Returns:
            Dict with keys:
                - success (bool): Whether hashing succeeded
                - file_path (str): Path to the file
                - hash_value (str): Calculated hash value
                - algorithm (str): Algorithm used
                - error (str): Error message if failed
        """
        logger.info(f"C++ binding: Calculating {algorithm} hash for {file_path}")
        try:
            file_hash = calculate_file_hash(Path(file_path), algorithm=algorithm)

            return {
                "success": True,
                "file_path": file_path,
                "hash_value": file_hash,
                "algorithm": algorithm,
                "error": None,
            }
        except Exception as e:
            logger.exception(f"Error in calculate_hash: {e}")
            return {
                "success": False,
                "file_path": file_path,
                "hash_value": None,
                "algorithm": algorithm,
                "error": str(e),
            }

    @staticmethod
    def get_adapter_info() -> Dict[str, Any]:
        """
        Get information about the pybind11 adapter.

        Returns:
            Dict with adapter metadata including supported methods and capabilities.
        """
        return {
            "adapter_name": "AutoUpdaterPyBindAdapter",
            "adapter_version": "1.0.0",
            "purpose": "pybind11 integration for auto_updater module",
            "supported_methods": [
                "is_platform_supported",
                "get_current_platform",
                "check_for_updates",
                "download_update",
                "verify_update",
                "backup_installation",
                "extract_update",
                "install_update",
                "rollback_update",
                "perform_update",
                "cleanup_update",
                "compare_version_strings",
                "parse_version_string",
                "calculate_hash",
            ],
            "return_types": ["dict", "bool", "int", "str", "list"],
            "exception_handling": "All exceptions are caught and returned as dict with 'success' and 'error' keys",
            "platform_support": ["windows", "linux", "macos"],
        }
