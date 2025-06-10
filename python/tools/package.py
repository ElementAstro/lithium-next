#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@file         package_manager.py
@brief        Advanced Python package management utility

@details      This module provides comprehensive functionality for Python package management,
              supporting both command-line usage and programmatic API access via pybind11.
              
              The module handles package installation, upgrades, uninstallation, dependency
              analysis, security checks, and virtual environment management.

              Command-line usage:
                python package_manager.py --check <package_name>
                python package_manager.py --install <package_name> [--version <version>]
                python package_manager.py --upgrade <package_name>
                python package_manager.py --uninstall <package_name>
                python package_manager.py --list-installed [--format <format>]
                python package_manager.py --freeze [<filename>] [--with-hashes]
                python package_manager.py --search <search_term>
                python package_manager.py --deps <package_name> [--json]
                python package_manager.py --create-venv <venv_name> [--python-version <version>]
                python package_manager.py --security-check [<package_name>]
                python package_manager.py --batch-install <requirements_file>
                python package_manager.py --compare <package1> <package2>
                python package_manager.py --info <package_name>
              
              Python API usage:
                from package_manager import PackageManager
                
                pm = PackageManager()
                pm.install_package("requests")
                pm.check_security("flask")
                pm.get_package_info("numpy")

@requires     - Python 3.10+
              - `requests` Python library
              - `packaging` Python library
              - Optional dependencies installed as needed
              
@version      2.0
@date         2025-06-09
"""

import subprocess
import sys
import os
import argparse
import json
import re
import shutil
import logging
import io
from pathlib import Path
from typing import Optional, Union, List, Dict, Any, Tuple, Callable
import importlib.metadata as importlib_metadata
from dataclasses import dataclass
from enum import Enum, auto
from functools import lru_cache
from concurrent.futures import ThreadPoolExecutor
import tempfile
import datetime
import contextlib
import urllib.parse

# Third-party dependencies - handled with dynamic imports to make them optional
OPTIONAL_DEPENDENCIES = {
    'requests': 'HTTP requests for PyPI',
    'packaging': 'Version parsing and comparison',
    'rich': 'Enhanced terminal output',
    'safety': 'Security vulnerability checking',
    'pipdeptree': 'Dependency tree analysis',
    'virtualenv': 'Virtual environment management',
}


class DependencyError(Exception):
    """Exception raised when a required dependency is missing."""
    pass


class PackageOperationError(Exception):
    """Exception raised when a package operation fails."""
    pass


class VersionError(Exception):
    """Exception raised when there's an issue with package versions."""
    pass


class PackageManager:
    """
    A comprehensive Python package management class with support for installation,
    upgrades, dependency analysis, and security checking.

    This class can be used programmatically or via command line interface.
    It also supports integration with C++ applications via pybind11.
    """

    class OutputFormat(Enum):
        """Output format options for package information."""
        TEXT = auto()
        JSON = auto()
        TABLE = auto()
        MARKDOWN = auto()

    @dataclass
    class PackageInfo:
        """Data class for storing package information."""
        name: str
        version: Optional[str] = None
        latest_version: Optional[str] = None
        summary: Optional[str] = None
        homepage: Optional[str] = None
        author: Optional[str] = None
        author_email: Optional[str] = None
        license: Optional[str] = None
        requires: Optional[List[str]] = None
        required_by: Optional[List[str]] = None
        location: Optional[str] = None

        def __post_init__(self):
            """Initialize list attributes if they are None."""
            if self.requires is None:
                self.requires = []
            if self.required_by is None:
                self.required_by = []

    def __init__(self, *, verbose: bool = False, pip_path: Optional[str] = None,
                 cache_dir: Optional[str] = None, timeout: int = 30):
        """
        Initialize the PackageManager with configurable options.

        Args:
            verbose (bool): Whether to output detailed logs
            pip_path (str, optional): Path to pip executable to use
            cache_dir (str, optional): Directory to use for caching package info
            timeout (int): Timeout in seconds for network operations
        """
        # Setup logging
        self.logger = logging.getLogger('package_manager')
        log_level = logging.DEBUG if verbose else logging.INFO
        self.logger.setLevel(log_level)

        if not self.logger.handlers:
            handler = logging.StreamHandler()
            formatter = logging.Formatter('%(levelname)s: %(message)s')
            handler.setFormatter(formatter)
            self.logger.addHandler(handler)

        # Initialize instance variables
        self._pip_path = pip_path or sys.executable.replace("python", "pip")
        self._timeout = timeout
        self._package_cache = {}

        # Create cache directory if specified
        self._cache_dir = None
        if cache_dir:
            self._cache_dir = Path(cache_dir)
            self._cache_dir.mkdir(parents=True, exist_ok=True)

    def _ensure_dependencies(self, *dependencies):
        """
        Ensure that the required dependencies are installed.

        Args:
            *dependencies: Names of required dependencies

        Raises:
            DependencyError: If any dependency is missing and can't be installed
        """
        missing = []

        for dep in dependencies:
            try:
                __import__(dep)
            except ImportError:
                missing.append(dep)

        if missing:
            self.logger.warning(f"Missing dependencies: {', '.join(missing)}")
            self.logger.info("Attempting to install missing dependencies...")

            for dep in missing:
                try:
                    self.install_package(dep, silent=True)
                    self.logger.info(f"Successfully installed {dep}")
                except PackageOperationError:
                    raise DependencyError(
                        f"Required dependency '{dep}' could not be installed. "
                        f"Purpose: {OPTIONAL_DEPENDENCIES.get(dep, 'Unknown')}"
                    )

    def _run_command(self, command: List[str], check: bool = True,
                     capture_output: bool = True) -> Tuple[int, str, str]:
        """
        Run a system command and return the result.

        Args:
            command (List[str]): Command to run as list of arguments
            check (bool): Whether to raise an exception on non-zero return code
            capture_output (bool): Whether to capture stdout/stderr

        Returns:
            Tuple[int, str, str]: Return code, stdout, and stderr

        Raises:
            PackageOperationError: If check is True and command returns non-zero
        """
        self.logger.debug(f"Running command: {' '.join(command)}")

        try:
            kwargs: Dict[str, Union[bool, int, Any]] = {
                'text': True,
                'check': False,
            }

            if capture_output:
                kwargs['stdout'] = subprocess.PIPE
                kwargs['stderr'] = subprocess.PIPE

            # Remove any keys from kwargs that are not valid for subprocess.run
            # (e.g., if they are accidentally set to bool)
            valid_keys = {
                'args', 'stdin', 'input', 'stdout', 'stderr', 'capture_output', 'shell',
                'cwd', 'timeout', 'check', 'encoding', 'errors', 'text', 'env', 'universal_newlines'
            }
            kwargs = {k: v for k, v in kwargs.items() if k in valid_keys}

            result = subprocess.run(command, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)

            if check and result.returncode != 0:
                error_msg = f"Command failed with code {result.returncode}"
                if hasattr(result, 'stderr') and result.stderr:
                    error_msg += f": {result.stderr.strip()}"
                raise PackageOperationError(error_msg)

            stdout = result.stdout.strip() if hasattr(
                result, 'stdout') and result.stdout else ""
            stderr = result.stderr.strip() if hasattr(
                result, 'stderr') and result.stderr else ""

            return result.returncode, stdout, stderr

        except FileNotFoundError:
            raise PackageOperationError(f"Command not found: {command[0]}")
        except Exception as e:
            raise PackageOperationError(f"Error running command: {e}")

    def is_package_installed(self, package_name: str) -> bool:
        """
        Check if a Python package is installed.

        Args:
            package_name (str): Name of the package to check

        Returns:
            bool: True if the package is installed, False otherwise
        """
        try:
            importlib_metadata.version(package_name)
            return True
        except importlib_metadata.PackageNotFoundError:
            return False

    def get_installed_version(self, package_name: str) -> Optional[str]:
        """
        Get the installed version of a Python package.

        Args:
            package_name (str): Name of the package to check

        Returns:
            Optional[str]: The installed version or None if not installed
        """
        try:
            return importlib_metadata.version(package_name)
        except importlib_metadata.PackageNotFoundError:
            return None

    @lru_cache(maxsize=100)
    def get_package_info(self, package_name: str) -> 'PackageInfo':
        """
        Get comprehensive information about a package.

        Args:
            package_name (str): Name of the package

        Returns:
            PackageInfo: Object containing package details

        Raises:
            PackageOperationError: If the package info cannot be retrieved
        """
        self._ensure_dependencies('requests')
        import requests

        # First check if the package is installed locally
        installed_version = self.get_installed_version(package_name)

        # Create the basic info object
        info = self.PackageInfo(name=package_name, version=installed_version)

        # Get additional local info if installed
        if installed_version:
            try:
                metadata = importlib_metadata.metadata(package_name)
                info.summary = metadata.get('Summary')
                info.homepage = metadata.get('Home-page')
                info.author = metadata.get('Author')
                info.author_email = metadata.get('Author-email')
                info.license = metadata.get('License')

                # Get package location
                dist = importlib_metadata.distribution(package_name)
                info.location = str(dist.locate_file(''))

                # Get package dependencies
                if dist.requires:
                    info.requires = [str(req).split(';')[0].strip()
                                     for req in dist.requires]
            except Exception as e:
                self.logger.warning(
                    f"Error getting local metadata for {package_name}: {e}")

        # Get PyPI info
        try:
            response = requests.get(
                f"https://pypi.org/pypi/{package_name}/json",
                timeout=self._timeout
            )
            response.raise_for_status()
            pypi_data = response.json()

            # Update with PyPI info
            if not info.summary:
                info.summary = pypi_data['info'].get('summary')
            if not info.homepage:
                info.homepage = pypi_data['info'].get(
                    'home_page') or pypi_data['info'].get('project_url')
            if not info.author:
                info.author = pypi_data['info'].get('author')
            if not info.author_email:
                info.author_email = pypi_data['info'].get('author_email')
            if not info.license:
                info.license = pypi_data['info'].get('license')

            # Get latest version from PyPI
            all_versions = list(pypi_data['releases'].keys())
            if all_versions:
                self._ensure_dependencies('packaging')
                from packaging import version as pkg_version
                latest = max(all_versions, key=pkg_version.parse)
                info.latest_version = latest

            # Get package dependencies from PyPI if not already found
            if not info.requires and 'requires_dist' in pypi_data['info'] and pypi_data['info']['requires_dist']:
                info.requires = [
                    req.split(';')[0].strip()
                    for req in pypi_data['info']['requires_dist']
                    if ';' not in req or 'extra ==' not in req
                ]

        except requests.RequestException as e:
            self.logger.warning(
                f"Error fetching PyPI data for {package_name}: {e}")
        except Exception as e:
            self.logger.warning(
                f"Error processing PyPI data for {package_name}: {e}")

        # Find which packages require this package
        try:
            # Use pip to find packages that depend on this
            cmd = [self._pip_path, "-m", "pip", "show", "-r", package_name]
            _, output, _ = self._run_command(cmd)

            # Parse the output to get packages that require this package
            required_by_section = False
            required_by = []

            for line in output.split('\n'):
                if line.startswith('Required-by:'):
                    required_by_section = True
                    value = line[len('Required-by:'):].strip()
                    if value and value != 'none':
                        required_by.extend([r.strip()
                                           for r in value.split(',')])
                elif required_by_section and line.startswith(' '):
                    # Continuation of the Required-by field
                    required_by.extend([r.strip()
                                       for r in line.strip().split(',')])
                elif required_by_section:
                    # No longer in the Required-by section
                    break

            info.required_by = [r for r in required_by if r]
        except Exception as e:
            self.logger.warning(
                f"Error getting packages that depend on {package_name}: {e}")

        return info

    def list_available_versions(self, package_name: str) -> List[str]:
        """
        List all available versions of a package from PyPI.

        Args:
            package_name (str): Name of the package

        Returns:
            List[str]: List of versions sorted from newest to oldest

        Raises:
            PackageOperationError: If versions cannot be retrieved
        """
        self._ensure_dependencies('requests', 'packaging')
        import requests
        from packaging import version as pkg_version

        cache_key = f"{package_name}_versions"
        if cache_key in self._package_cache:
            return self._package_cache[cache_key]

        try:
            response = requests.get(
                f"https://pypi.org/pypi/{package_name}/json",
                timeout=self._timeout
            )
            response.raise_for_status()
            data = response.json()

            versions = sorted(
                data['releases'].keys(),
                key=pkg_version.parse,
                reverse=True
            )

            # Cache the results
            self._package_cache[cache_key] = versions

            return versions
        except requests.RequestException as e:
            raise PackageOperationError(
                f"Error fetching versions for {package_name}: {e}")
        except Exception as e:
            raise PackageOperationError(
                f"Error processing versions for {package_name}: {e}")

    def compare_versions(self, package_name: str, version1: str, version2: str) -> int:
        """
        Compare two versions of a package.

        Args:
            package_name (str): Name of the package
            version1 (str): First version to compare
            version2 (str): Second version to compare

        Returns:
            int: -1 if version1 < version2, 0 if equal, 1 if version1 > version2

        Raises:
            VersionError: If versions cannot be compared
        """
        self._ensure_dependencies('packaging')
        from packaging import version as pkg_version

        try:
            v1 = pkg_version.parse(version1)
            v2 = pkg_version.parse(version2)

            if v1 < v2:
                return -1
            elif v1 > v2:
                return 1
            else:
                return 0
        except Exception as e:
            raise VersionError(
                f"Error comparing versions {version1} and {version2}: {e}")

    def install_package(
        self,
        package_name: str,
        version: Optional[str] = None,
        upgrade: bool = False,
        force_reinstall: bool = False,
        deps: bool = True,
        silent: bool = False
    ) -> bool:
        """
        Install a Python package using pip.

        Args:
            package_name (str): Name of the package to install
            version (str, optional): Specific version to install
            upgrade (bool): Whether to upgrade the package if already installed
            force_reinstall (bool): Force reinstallation even if already installed
            deps (bool): Whether to install dependencies
            silent (bool): Whether to suppress output

        Returns:
            bool: True if installation succeeded

        Raises:
            PackageOperationError: If installation fails
        """
        # Construct the package specifier
        if version:
            pkg_spec = f"{package_name}=={version}"
        else:
            pkg_spec = package_name

        # Start building the command
        cmd = [self._pip_path, "-m", "pip", "install"]

        # Add options
        if upgrade:
            cmd.append("--upgrade")
        if force_reinstall:
            cmd.append("--force-reinstall")
        if not deps:
            cmd.append("--no-deps")
        if silent:
            cmd.append("--quiet")

        cmd.append(pkg_spec)

        try:
            self._run_command(cmd)
            self.logger.info(f"Successfully installed {pkg_spec}")

            # Clear the package from cache to ensure fresh data after installation
            for key in list(self._package_cache.keys()):
                if package_name in key:
                    del self._package_cache[key]

            return True
        except PackageOperationError as e:
            if not silent:
                self.logger.error(f"Failed to install {pkg_spec}: {e}")
            raise

    def upgrade_package(self, package_name: str) -> bool:
        """
        Upgrade a Python package to the latest version.

        Args:
            package_name (str): Name of the package to upgrade

        Returns:
            bool: True if upgrade succeeded

        Raises:
            PackageOperationError: If upgrade fails
        """
        return self.install_package(package_name, upgrade=True)

    def uninstall_package(self, package_name: str, yes: bool = True) -> bool:
        """
        Uninstall a Python package.

        Args:
            package_name (str): Name of the package to uninstall
            yes (bool): Auto-confirm the uninstallation

        Returns:
            bool: True if uninstallation succeeded

        Raises:
            PackageOperationError: If uninstallation fails
        """
        cmd = [self._pip_path, "-m", "pip", "uninstall"]

        if yes:
            cmd.append("-y")

        cmd.append(package_name)

        try:
            self._run_command(cmd)
            self.logger.info(f"Successfully uninstalled {package_name}")

            # Clear the package from cache
            for key in list(self._package_cache.keys()):
                if package_name in key:
                    del self._package_cache[key]

            return True
        except PackageOperationError as e:
            self.logger.error(f"Failed to uninstall {package_name}: {e}")
            raise

    def list_installed_packages(
        self,
        output_format: OutputFormat = OutputFormat.TEXT
    ) -> Union[str, List[Dict[str, str]]]:
        """
        List all installed packages with their versions.

        Args:
            output_format: Format of the output (text, json, table or markdown)

        Returns:
            Union[str, List[Dict[str, str]]]: List of packages in the requested format
        """
        cmd = [self._pip_path, "-m", "pip", "list", "--format=json"]
        _, output, _ = self._run_command(cmd)

        packages = json.loads(output)

        match output_format:
            case self.OutputFormat.JSON:
                return packages
            case self.OutputFormat.TABLE:
                self._ensure_dependencies('rich')
                from rich.console import Console
                from rich.table import Table

                table = Table(title="Installed Packages")
                table.add_column("Package", style="cyan")
                table.add_column("Version", style="green")
                table.add_column("Latest", style="yellow")
                table.add_column("Status", style="blue")

                # Use ThreadPoolExecutor to parallelize version checking
                with ThreadPoolExecutor(max_workers=min(10, os.cpu_count() or 2)) as executor:
                    # Create a mapping of each package to its future for checking the latest version
                    futures = {
                        pkg['name']: executor.submit(
                            self.get_package_info, pkg['name'])
                        # Limit to avoid too many requests
                        for pkg in packages[:30]
                    }

                    for pkg in packages:
                        name = pkg['name']
                        version = pkg['version']

                        # Get the latest version if available
                        latest = "Unknown"
                        status = ""

                        if name in futures:
                            try:
                                info = futures[name].result(timeout=5)
                                if info.latest_version:
                                    latest = info.latest_version

                                    # Compare versions
                                    if self.compare_versions(name, version, latest) < 0:
                                        status = "Update available"
                                    else:
                                        status = "Up to date"
                            except Exception:
                                latest = "Error"
                                status = "Check failed"

                        table.add_row(name, version, latest, status)

                console = Console()
                console_output = Console(file=io.StringIO())
                console_output.print(table)
                return console_output.file.getvalue()
            case self.OutputFormat.MARKDOWN:
                lines = ["| Package | Version |", "|---------|---------|"]
                for pkg in packages:
                    lines.append(f"| {pkg['name']} | {pkg['version']} |")
                return '\n'.join(lines)
            # Default case (TEXT)
            case _:
                lines = []
                for pkg in packages:
                    lines.append(f"{pkg['name']} {pkg['version']}")
                return '\n'.join(lines)

    def search_packages(self, query: str, limit: int = 20) -> List[Dict[str, Any]]:
        """
        Search for packages on PyPI.

        Args:
            query (str): Search query
            limit (int): Maximum number of results to return

        Returns:
            List[Dict[str, Any]]: List of matching packages with their info
        """
        self._ensure_dependencies('requests')
        import requests

        query = query.strip()
        if not query:
            return []

        try:
            # URL-encode the query
            encoded_query = urllib.parse.quote(query)
            url = f"https://pypi.org/search/?q={encoded_query}&format=json"

            response = requests.get(url, timeout=self._timeout)
            response.raise_for_status()
            data = response.json()

            results = []
            for item in data.get('results', [])[:limit]:
                package_info = {
                    'name': item.get('name', ''),
                    'version': item.get('version', ''),
                    'description': item.get('description', ''),
                    'project_url': item.get('project_url', '')
                }
                results.append(package_info)

            return results
        except Exception as e:
            self.logger.error(f"Error searching for packages: {e}")
            return []

    def generate_requirements(self,
                              output_file: Optional[str] = "requirements.txt",
                              include_version: bool = True,
                              include_hashes: bool = False) -> str:
        """
        Generate a requirements.txt file for the current environment.

        Args:
            output_file (str, optional): Path to output file. If None, returns content as string.
            include_version (bool): Whether to include version specifications
            include_hashes (bool): Whether to include package hashes

        Returns:
            str: Content of the requirements file
        """
        cmd = [self._pip_path, "-m", "pip", "freeze"]
        _, output, _ = self._run_command(cmd)

        if not include_version:
            # Strip version info
            lines = []
            for line in output.splitlines():
                if '==' in line:
                    package = line.split('==')[0]
                    lines.append(package)
                else:
                    lines.append(line)
            output = '\n'.join(lines)

        if include_hashes:
            # Generate requirements with hashes
            with tempfile.NamedTemporaryFile(delete=False, mode='w+') as temp_file:
                temp_file.write(output)
                temp_file_path = temp_file.name

            try:
                cmd = [
                    self._pip_path, "-m", "pip", "install",
                    "--dry-run", "--report", "-", "-r", temp_file_path
                ]
                _, hash_output, _ = self._run_command(cmd)

                # Parse the JSON output
                try:
                    report = json.loads(hash_output)

                    # Generate requirements with hashes
                    lines = []
                    for install in report.get('install', []):
                        pkg_name = install.get('metadata', {}).get('name', '')
                        pkg_version = install.get(
                            'metadata', {}).get('version', '')
                        hashes = []

                        for download_info in install.get('download_info', []):
                            if 'sha256' in download_info:
                                hashes.append(
                                    f"sha256:{download_info['sha256']}")

                        if pkg_name and pkg_version:
                            line = f"{pkg_name}=={pkg_version}"
                            if hashes:
                                for h in hashes:
                                    line += f" \\\n    --hash={h}"
                            lines.append(line)

                    output = '\n'.join(lines)
                except Exception as e:
                    self.logger.warning(
                        f"Failed to generate requirements with hashes: {e}")
                    # Fall back to regular freeze output
            finally:
                # Clean up temp file
                try:
                    os.unlink(temp_file_path)
                except:
                    pass

        if output_file:
            with open(output_file, "w", encoding="utf-8") as f:
                f.write(output)
            self.logger.info(f"Requirements written to {output_file}")

        return output

    def check_security(self, package_name: Optional[str] = None) -> List[Dict[str, Any]]:
        """
        Check for security vulnerabilities in packages.

        Args:
            package_name (str, optional): Specific package to check, or None for all installed packages

        Returns:
            List[Dict[str, Any]]: List of vulnerabilities found
        """
        self._ensure_dependencies('safety')

        with tempfile.NamedTemporaryFile(mode='w+', delete=False) as temp_file:
            if package_name:
                temp_file.write(
                    f"{package_name}=={self.get_installed_version(package_name)}")
            else:
                # Get all installed packages
                cmd = [self._pip_path, "-m", "pip", "freeze"]
                _, output, _ = self._run_command(cmd)
                temp_file.write(output)

            temp_file_path = temp_file.name

        try:
            # Run safety check with JSON output
            cmd = ["safety", "check", "-r", temp_file_path, "--json"]
            try:
                _, output, _ = self._run_command(cmd)
                vulns = json.loads(output)
                return vulns.get('vulnerabilities', [])
            except Exception as e:
                self.logger.error(f"Error checking security: {e}")
                return []
        finally:
            # Clean up temp file
            try:
                os.unlink(temp_file_path)
            except:
                pass

    def analyze_dependencies(self, package_name: str, as_json: bool = False) -> Union[str, Dict]:
        """
        Analyze dependencies of a package and create a dependency tree.

        Args:
            package_name (str): Name of the package
            as_json (bool): Whether to return as JSON object instead of formatted string

        Returns:
            Union[str, Dict]: Dependency tree as string or JSON object
        """
        self._ensure_dependencies('pipdeptree')

        if as_json:
            cmd = [self._pip_path, "-m", "pipdeptree",
                   "-p", package_name, "--json"]
        else:
            cmd = [self._pip_path, "-m", "pipdeptree", "-p", package_name]

        _, output, _ = self._run_command(cmd)

        if as_json:
            return json.loads(output)
        return output

    def create_virtual_env(self,
                           venv_path: str,
                           python_version: Optional[str] = None,
                           system_site_packages: bool = False,
                           with_pip: bool = True) -> bool:
        """
        Create a new virtual environment.

        Args:
            venv_path (str): Path where to create the virtual environment
            python_version (str, optional): Python version to use (e.g., "3.10")
            system_site_packages (bool): Whether to give access to system site packages
            with_pip (bool): Whether to include pip in the environment

        Returns:
            bool: True if creation succeeded

        Raises:
            PackageOperationError: If creation fails
        """
        self._ensure_dependencies('virtualenv')

        cmd = ["virtualenv"]

        if python_version:
            cmd.extend(["-p", f"python{python_version}"])

        if system_site_packages:
            cmd.append("--system-site-packages")

        if not with_pip:
            cmd.append("--no-pip")

        cmd.append(venv_path)

        try:
            self._run_command(cmd)
            self.logger.info(
                f"Successfully created virtual environment at {venv_path}")
            return True
        except PackageOperationError as e:
            self.logger.error(f"Failed to create virtual environment: {e}")
            raise

    def batch_install(self, requirements_file: str) -> bool:
        """
        Install packages from a requirements file.

        Args:
            requirements_file (str): Path to requirements.txt file

        Returns:
            bool: True if installation succeeded

        Raises:
            PackageOperationError: If installation fails
        """
        cmd = [self._pip_path, "-m", "pip", "install", "-r", requirements_file]

        try:
            self._run_command(cmd)
            self.logger.info(
                f"Successfully installed packages from {requirements_file}")
            return True
        except PackageOperationError as e:
            self.logger.error(
                f"Failed to install packages from {requirements_file}: {e}")
            raise

    def compare_packages(self, package1: str, package2: str) -> Dict[str, Any]:
        """
        Compare two packages and their metadata.

        Args:
            package1 (str): First package name
            package2 (str): Second package name

        Returns:
            Dict[str, Any]: Comparison results
        """
        info1 = self.get_package_info(package1)
        info2 = self.get_package_info(package2)

        # Find common dependencies
        common_deps = set(info1.requires) & set(info2.requires) if (
            info1.requires and info2.requires) else set()

        # Find unique dependencies
        unique_deps1 = set(info1.requires) - set(info2.requires) if (
            info1.requires and info2.requires) else set(info1.requires or [])
        unique_deps2 = set(info2.requires) - set(info1.requires) if (
            info1.requires and info2.requires) else set(info2.requires or [])

        comparison = {
            'package1': {
                'name': info1.name,
                'version': info1.version,
                'latest_version': info1.latest_version,
                'unique_dependencies': list(unique_deps1),
                'license': info1.license,
                'author': info1.author,
                'summary': info1.summary
            },
            'package2': {
                'name': info2.name,
                'version': info2.version,
                'latest_version': info2.latest_version,
                'unique_dependencies': list(unique_deps2),
                'license': info2.license,
                'author': info2.author,
                'summary': info2.summary
            },
            'common': {
                'dependencies': list(common_deps),
            }
        }

        return comparison

    def validate_package(self, package_name: str,
                         check_security: bool = True,
                         check_license: bool = True) -> Dict[str, Any]:
        """
        Validate a package for security issues, license, and other metrics.

        Args:
            package_name (str): Name of the package to validate
            check_security (bool): Whether to check for security vulnerabilities
            check_license (bool): Whether to check license compatibility

        Returns:
            Dict[str, Any]: Validation results
        """
        validation = {
            'name': package_name,
            'is_installed': self.is_package_installed(package_name),
            'version': self.get_installed_version(package_name),
            'validation_time': datetime.datetime.now().isoformat(),
            'issues': []
        }

        # Get package info
        try:
            info = self.get_package_info(package_name)
            validation['info'] = {
                'summary': info.summary,
                'author': info.author,
                'license': info.license,
                'homepage': info.homepage,
                'dependencies_count': len(info.requires) if info.requires else 0
            }
        except Exception as e:
            validation['issues'].append(f"Error fetching package info: {e}")

        # Security check
        if check_security:
            try:
                vulnerabilities = self.check_security(package_name)
                validation['security'] = {
                    'vulnerabilities': vulnerabilities,
                    'vulnerability_count': len(vulnerabilities)
                }
                if vulnerabilities:
                    validation['issues'].append(
                        f"Found {len(vulnerabilities)} security vulnerabilities")
            except Exception as e:
                validation['issues'].append(f"Security check failed: {e}")

        # License check
        if check_license and 'info' in validation and validation['info'].get('license'):
            license_name = validation['info']['license']

            # List of approved licenses (example)
            approved_licenses = [
                'MIT', 'BSD', 'Apache', 'Apache 2.0', 'Apache-2.0',
                'ISC', 'Python', 'Python Software Foundation',
                'MPL', 'MPL-2.0', 'GPL', 'GPL-3.0'
            ]

            validation['license_check'] = {
                'license': license_name,
                'is_approved': any(al.lower() in license_name.lower() for al in approved_licenses)
            }

            if not validation['license_check']['is_approved']:
                validation['issues'].append(
                    f"License '{license_name}' may require review")

        return validation


def main():
    """
    Main function for command-line execution.

    Parses command-line arguments and invokes appropriate PackageManager methods.
    """
    parser = argparse.ArgumentParser(
        description="Advanced Python Package Management Utility",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python package_manager.py --check requests
  python package_manager.py --install flask --version 2.0.0
  python package_manager.py --search "data science"
  python package_manager.py --deps numpy
  python package_manager.py --security-check
  python package_manager.py --batch-install requirements.txt
  python package_manager.py --compare requests flask
        """
    )

    # Basic package operations
    parser.add_argument("--check", metavar="PACKAGE",
                        help="Check if a specific package is installed")
    parser.add_argument("--install", metavar="PACKAGE",
                        help="Install a specific package")
    parser.add_argument("--version", metavar="VERSION",
                        help="Specify the version of the package to install")
    parser.add_argument("--upgrade", metavar="PACKAGE",
                        help="Upgrade a specific package to the latest version")
    parser.add_argument("--uninstall", metavar="PACKAGE",
                        help="Uninstall a specific package")

    # Package listing and requirements
    parser.add_argument("--list-installed", action="store_true",
                        help="List all installed packages")
    parser.add_argument("--freeze", metavar="FILE", nargs="?",
                        const="requirements.txt", help="Generate a requirements.txt file")
    parser.add_argument("--with-hashes", action="store_true",
                        help="Include hashes in requirements.txt (use with --freeze)")

    # Advanced features
    parser.add_argument("--search", metavar="TERM",
                        help="Search for packages on PyPI")
    parser.add_argument("--deps", metavar="PACKAGE",
                        help="Show dependencies of a package")
    parser.add_argument("--create-venv", metavar="PATH",
                        help="Create a new virtual environment")
    parser.add_argument("--python-version", metavar="VERSION",
                        help="Python version for virtual environment (use with --create-venv)")
    parser.add_argument("--security-check", metavar="PACKAGE", nargs="?", const="all",
                        help="Check for security vulnerabilities")
    parser.add_argument("--batch-install", metavar="FILE",
                        help="Install packages from a requirements file")
    parser.add_argument("--compare", nargs=2, metavar=("PACKAGE1", "PACKAGE2"),
                        help="Compare two packages")
    parser.add_argument("--info", metavar="PACKAGE",
                        help="Show detailed information about a package")
    parser.add_argument("--validate", metavar="PACKAGE",
                        help="Validate a package (security, license, etc.)")

    # Output format options
    parser.add_argument("--json", action="store_true",
                        help="Output in JSON format when applicable")
    parser.add_argument("--markdown", action="store_true",
                        help="Output in Markdown format when applicable")
    parser.add_argument("--table", action="store_true",
                        help="Output as a rich text table when applicable")

    # Configuration options
    parser.add_argument("--verbose", action="store_true",
                        help="Enable verbose output")
    parser.add_argument("--timeout", type=int, default=30,
                        help="Timeout in seconds for network operations")
    parser.add_argument("--cache-dir", metavar="DIR",
                        help="Directory to use for caching package information")

    args = parser.parse_args()

    # Initialize PackageManager
    pm = PackageManager(
        verbose=args.verbose,
        timeout=args.timeout,
        cache_dir=args.cache_dir
    )

    # Determine output format
    output_format = pm.OutputFormat.TEXT
    if args.json:
        output_format = pm.OutputFormat.JSON
    elif args.markdown:
        output_format = pm.OutputFormat.MARKDOWN
    elif args.table:
        output_format = pm.OutputFormat.TABLE

    # Handle commands
    try:
        if args.check:
            if pm.is_package_installed(args.check):
                print(f"Package '{args.check}' is installed, version: {
                      pm.get_installed_version(args.check)}")
            else:
                print(f"Package '{args.check}' is not installed.")

        elif args.install:
            pm.install_package(args.install, version=args.version)
            print(f"Successfully installed {args.install}")

        elif args.upgrade:
            pm.upgrade_package(args.upgrade)
            print(f"Successfully upgraded {args.upgrade}")

        elif args.uninstall:
            pm.uninstall_package(args.uninstall)
            print(f"Successfully uninstalled {args.uninstall}")

        elif args.list_installed:
            output = pm.list_installed_packages(output_format)
            if isinstance(output, list) and args.json:
                print(json.dumps(output, indent=2))
            else:
                print(output)

        elif args.freeze is not None:
            content = pm.generate_requirements(
                args.freeze,
                include_hashes=args.with_hashes
            )
            if args.freeze == "-":
                print(content)

        elif args.search:
            results = pm.search_packages(args.search)
            if args.json:
                print(json.dumps(results, indent=2))
            else:
                if not results:
                    print(f"No packages found matching '{args.search}'")
                else:
                    print(
                        f"Found {len(results)} packages matching '{args.search}':")
                    for pkg in results:
                        print(f"{pkg['name']} ({pkg['version']})")
                        if pkg['description']:
                            print(f"  {pkg['description']}")
                        print()

        elif args.deps:
            result = pm.analyze_dependencies(args.deps, as_json=args.json)
            if args.json:
                print(json.dumps(result, indent=2))
            else:
                print(result)

        elif args.create_venv:
            success = pm.create_virtual_env(
                args.create_venv,
                python_version=args.python_version
            )
            if success:
                print(f"Virtual environment created at {args.create_venv}")

        elif args.security_check is not None:
            package = None if args.security_check == "all" else args.security_check
            vulns = pm.check_security(package)
            if args.json:
                print(json.dumps(vulns, indent=2))
            else:
                if not vulns:
                    print("No vulnerabilities found!")
                else:
                    print(f"Found {len(vulns)} vulnerabilities:")
                    for vuln in vulns:
                        print(
                            f"- {vuln['package_name']} {vuln['vulnerable_version']}: {vuln['advisory']}")

        elif args.batch_install:
            pm.batch_install(args.batch_install)
            print(f"Successfully installed packages from {args.batch_install}")

        elif args.compare:
            pkg1, pkg2 = args.compare
            comparison = pm.compare_packages(pkg1, pkg2)
            if args.json:
                print(json.dumps(comparison, indent=2))
            else:
                print(f"Comparison between {pkg1} and {pkg2}:")
                print(f"\n{pkg1}:")
                print(f"  Version: {comparison['package1']['version']}")
                print(
                    f"  Latest version: {comparison['package1']['latest_version']}")
                print(f"  License: {comparison['package1']['license']}")
                print(f"  Summary: {comparison['package1']['summary']}")

                print(f"\n{pkg2}:")
                print(f"  Version: {comparison['package2']['version']}")
                print(
                    f"  Latest version: {comparison['package2']['latest_version']}")
                print(f"  License: {comparison['package2']['license']}")
                print(f"  Summary: {comparison['package2']['summary']}")

                print("\nCommon dependencies:")
                for dep in comparison['common']['dependencies']:
                    print(f"  - {dep}")

                print(f"\nUnique dependencies in {pkg1}:")
                for dep in comparison['package1']['unique_dependencies']:
                    print(f"  - {dep}")

                print(f"\nUnique dependencies in {pkg2}:")
                for dep in comparison['package2']['unique_dependencies']:
                    print(f"  - {dep}")

        elif args.info:
            info = pm.get_package_info(args.info)
            if args.json:
                # Convert dataclass to dict for JSON serialization
                info_dict = {
                    'name': info.name,
                    'version': info.version,
                    'latest_version': info.latest_version,
                    'summary': info.summary,
                    'homepage': info.homepage,
                    'author': info.author,
                    'author_email': info.author_email,
                    'license': info.license,
                    'requires': info.requires,
                    'required_by': info.required_by,
                    'location': info.location
                }
                print(json.dumps(info_dict, indent=2))
            else:
                print(f"Package: {info.name}")
                print(f"Installed version: {info.version or 'Not installed'}")
                print(f"Latest version: {info.latest_version}")
                print(f"Summary: {info.summary}")
                print(f"Homepage: {info.homepage}")
                print(f"Author: {info.author} <{info.author_email}>")
                print(f"License: {info.license}")
                print(f"Installation path: {info.location}")

                print("\nDependencies:")
                if info.requires:
                    for dep in info.requires:
                        print(f"  - {dep}")
                else:
                    print("  No dependencies")

                print("\nRequired by:")
                if info.required_by:
                    for pkg in info.required_by:
                        print(f"  - {pkg}")
                else:
                    print("  No packages depend on this package")

        elif args.validate:
            validation = pm.validate_package(args.validate)
            if args.json:
                print(json.dumps(validation, indent=2))
            else:
                print(f"Validation results for {validation['name']}:")
                print(f"  Installed: {validation['is_installed']}")
                if validation['is_installed']:
                    print(f"  Version: {validation['version']}")

                if 'info' in validation:
                    print(f"  License: {validation['info']['license']}")
                    print(
                        f"  Dependencies: {validation['info']['dependencies_count']}")

                if 'security' in validation:
                    print(
                        f"  Security vulnerabilities: {validation['security']['vulnerability_count']}")

                if validation['issues']:
                    print("\nIssues found:")
                    for issue in validation['issues']:
                        print(f"  - {issue}")
                else:
                    print("\nNo issues found! Package looks good.")

        else:
            # No arguments provided, print help
            parser.print_help()

    except Exception as e:
        print(f"Error: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)


# For pybind11 export
def export_package_manager():
    """
    Export functions for use with pybind11 for C++ integration.

    This function prepares the Python classes and functions for binding to C++.
    It's called automatically when the module is imported but not run as a script.
    """
    try:
        import pybind11
        # When the C++ code includes this module, the export will be available
        return {
            'PackageManager': PackageManager,
            'OutputFormat': PackageManager.OutputFormat,
            'DependencyError': DependencyError,
            'PackageOperationError': PackageOperationError,
            'VersionError': VersionError
        }
    except ImportError:
        # pybind11 not available, just continue without exporting
        pass


# Entry point for command-line execution
if __name__ == "__main__":
    main()
else:
    # When imported as a module, prepare for pybind11 integration
    export_package_manager()
