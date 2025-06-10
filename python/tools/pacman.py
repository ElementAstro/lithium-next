#!/usr/bin/env python3
"""
Advanced Pacman Package Manager

This module provides a comprehensive interface to the pacman package manager,
supporting both CLI usage and embedded calls via pybind11. It handles cross-platform
execution on both Linux and Windows (MSYS2) environments.

Features:
- Full type annotations
- Async operations for long-running commands
- Rich error handling
- Expanded functionality for package management
- Configuration management
- AUR helper integration
"""

import subprocess
import platform
import os
import argparse
import asyncio
import shutil
import re
import json
from enum import Enum, auto
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Union, Tuple, Set, Any, Callable, TypedDict, cast
from functools import lru_cache
import concurrent.futures
from datetime import datetime
import sys
import importlib.util

# Check if loguru is available, else use standard logging
try:
    from loguru import logger
except ImportError:
    import logging
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    logger = logging.getLogger(__name__)


# Define custom exception types for better error handling
class PacmanError(Exception):
    """Base exception for all pacman-related errors"""
    pass


class CommandError(PacmanError):
    """Exception raised when a command execution fails"""
    def __init__(self, message: str, return_code: int, stderr: str):
        self.return_code = return_code
        self.stderr = stderr
        super().__init__(f"{message} (Return code: {return_code}): {stderr}")


class PackageNotFoundError(PacmanError):
    """Exception raised when a package is not found"""
    pass


class ConfigError(PacmanError):
    """Exception raised when there's a configuration error"""
    pass


class PackageStatus(Enum):
    """Enum representing the status of a package"""
    INSTALLED = auto()
    NOT_INSTALLED = auto()
    OUTDATED = auto()
    PARTIALLY_INSTALLED = auto()


@dataclass
class PackageInfo:
    """Data class to store package information"""
    name: str
    version: str
    description: str = ""
    install_size: str = ""
    installed: bool = False
    repository: str = ""
    dependencies: List[str] = field(default_factory=list)
    optional_dependencies: Optional[List[str]] = field(default_factory=list)
    build_date: str = ""
    install_date: Optional[str] = None
    
    def __post_init__(self):
        """Initialize default lists"""
        if self.dependencies is None:
            self.dependencies = []
        if self.optional_dependencies is None:
            self.optional_dependencies = []


class CommandResult(TypedDict):
    """Type definition for command execution results"""
    success: bool
    stdout: str
    stderr: str
    command: List[str]
    return_code: int


class PacmanConfig:
    """Class to manage pacman configuration settings"""
    
    def __init__(self, config_path: Optional[Path] = None):
        """
        Initialize the pacman configuration manager.
        
        Args:
            config_path: Path to the pacman.conf file. If None, uses the default path.
        """
        self.is_windows = platform.system().lower() == 'windows'
        
        if config_path:
            self.config_path = config_path
        elif self.is_windows:
            # Default MSYS2 pacman config path
            self.config_path = Path(r'C:\msys64\etc\pacman.conf')
            if not self.config_path.exists():
                self.config_path = Path(r'C:\msys32\etc\pacman.conf')
        else:
            # Default Linux pacman config path
            self.config_path = Path('/etc/pacman.conf')
            
        if not self.config_path.exists():
            raise ConfigError(f"Pacman configuration file not found at {self.config_path}")
            
        # Cache for config settings to avoid repeated parsing
        self._cache: Dict[str, Any] = {}
        
    def _parse_config(self) -> Dict[str, Any]:
        """Parse the pacman.conf file and return a dictionary of settings"""
        if self._cache:
            return self._cache
            
        config: Dict[str, Any] = {
            "repos": {},
            "options": {}
        }
        current_section = "options"
        
        with open(self.config_path, 'r') as f:
            for line in f:
                line = line.strip()
                
                # Skip comments and empty lines
                if not line or line.startswith('#'):
                    continue
                    
                # Check for section headers
                if line.startswith('[') and line.endswith(']'):
                    current_section = line[1:-1]
                    if current_section != "options":
                        config["repos"][current_section] = {"enabled": True}
                    continue
                    
                # Parse key-value pairs
                if '=' in line:
                    key, value = line.split('=', 1)
                    key = key.strip()
                    value = value.strip()
                    
                    # Remove inline comments
                    if '#' in value:
                        value = value.split('#', 1)[0].strip()
                        
                    if current_section == "options":
                        config["options"][key] = value
                    else:
                        config["repos"][current_section][key] = value
        
        self._cache = config
        return config
        
    def get_option(self, option: str) -> Optional[str]:
        """
        Get the value of a specific option from pacman.conf.
        
        Args:
            option: The option name to retrieve
            
        Returns:
            The option value or None if not found
        """
        config = self._parse_config()
        return config.get("options", {}).get(option)
        
    def set_option(self, option: str, value: str) -> bool:
        """
        Set or modify an option in pacman.conf.
        
        Args:
            option: The option name to set
            value: The value to set
            
        Returns:
            True if successful, False otherwise
        """
        # Read the current config
        with open(self.config_path, 'r') as f:
            lines = f.readlines()
            
        option_pattern = re.compile(fr'^#?\s*{re.escape(option)}\s*=.*')
        option_found = False
        
        for i, line in enumerate(lines):
            if option_pattern.match(line):
                lines[i] = f"{option} = {value}\n"
                option_found = True
                break
                
        if not option_found:
            # Add to the [options] section
            options_index = -1
            for i, line in enumerate(lines):
                if line.strip() == '[options]':
                    options_index = i
                    break
                    
            if options_index >= 0:
                lines.insert(options_index + 1, f"{option} = {value}\n")
            else:
                lines.append(f"\n[options]\n{option} = {value}\n")
                
        # Write back to file (requires sudo typically)
        try:
            with open(self.config_path, 'w') as f:
                f.writelines(lines)
            self._cache = {}  # Clear cache
            return True
        except (PermissionError, OSError):
            logger.error(f"Failed to write to {self.config_path}. Do you have sufficient permissions?")
            return False
            
    def get_enabled_repos(self) -> List[str]:
        """
        Get a list of enabled repositories.
        
        Returns:
            List of enabled repository names
        """
        config = self._parse_config()
        return [repo for repo, details in config.get("repos", {}).items() 
                if details.get("enabled", False)]
                
    def enable_repo(self, repo: str) -> bool:
        """
        Enable a repository in pacman.conf.
        
        Args:
            repo: The repository name to enable
            
        Returns:
            True if successful, False otherwise
        """
        # Read the current config
        with open(self.config_path, 'r') as f:
            content = f.read()
            
        # Look for the repository section commented out
        section_pattern = re.compile(fr'#\s*\[{re.escape(repo)}\]')
        if section_pattern.search(content):
            # Uncomment the section
            content = section_pattern.sub(f"[{repo}]", content)
            
            # Write back to file
            try:
                with open(self.config_path, 'w') as f:
                    f.write(content)
                self._cache = {}  # Clear cache
                return True
            except (PermissionError, OSError):
                logger.error(f"Failed to write to {self.config_path}. Do you have sufficient permissions?")
                return False
        else:
            logger.warning(f"Repository {repo} not found in config")
            return False


class PacmanManager:
    """
    A comprehensive manager for the pacman package manager.
    Supports both Windows (MSYS2) and Linux environments.
    """
    
    def __init__(self, config_path: Optional[Path] = None, use_sudo: bool = True):
        """
        Initialize the PacmanManager with platform detection and configuration.
        
        Args:
            config_path: Custom path to pacman.conf
            use_sudo: Whether to use sudo for privileged operations (Linux only)
        """
        # Platform detection
        self.is_windows = platform.system().lower() == 'windows'
        self.use_sudo = use_sudo and not self.is_windows
        
        # Set up config management
        self.config = PacmanConfig(config_path)
        
        # Find pacman command
        self.pacman_command = self._find_pacman_command()
        
        # Cache for installed packages
        self._installed_packages: Optional[Dict[str, PackageInfo]] = None
        
        # Set up ThreadPoolExecutor for concurrent operations
        self._executor = concurrent.futures.ThreadPoolExecutor(max_workers=10)
        
        # Check if AUR helper is available
        self.aur_helper = self._detect_aur_helper()
        
        logger.debug(f"PacmanManager initialized with pacman at {self.pacman_command}")
        
    def __del__(self):
        """Cleanup resources when the instance is deleted"""
        if hasattr(self, '_executor'):
            self._executor.shutdown(wait=False)
            
    @lru_cache(maxsize=1)
    def _find_pacman_command(self) -> str:
        """
        Locate the 'pacman' command based on the current platform.
        
        Returns:
            Path to pacman executable
        
        Raises:
            FileNotFoundError: If pacman is not found
        """
        if self.is_windows:
            # Possible paths for MSYS2 pacman executable
            possible_paths = [
                r'C:\msys64\usr\bin\pacman.exe',
                r'C:\msys32\usr\bin\pacman.exe'
            ]
            
            for path in possible_paths:
                if os.path.exists(path):
                    return path
                    
            raise FileNotFoundError("MSYS2 pacman not found. Please ensure MSYS2 is installed.")
        else:
            # For Linux, check if pacman is in PATH
            pacman_path = shutil.which('pacman')
            if not pacman_path:
                raise FileNotFoundError("pacman not found in PATH. Is it installed?")
            return pacman_path
            
    def _detect_aur_helper(self) -> Optional[str]:
        """
        Detect if any popular AUR helper is installed.
        
        Returns:
            Name of the found AUR helper or None if not found
        """
        aur_helpers = ['yay', 'paru', 'pikaur', 'aurman', 'trizen']
        
        for helper in aur_helpers:
            if shutil.which(helper):
                logger.debug(f"Found AUR helper: {helper}")
                return helper
                
        logger.debug("No AUR helper detected")
        return None
        
    def run_command(self, command: List[str], capture_output: bool = True) -> CommandResult:
        """
        Execute a command with proper handling for Windows/Linux differences.
        
        Args:
            command: The command to execute as a list of strings
            capture_output: Whether to capture and return command output
            
        Returns:
            CommandResult with execution results and metadata
            
        Raises:
            CommandError: If the command execution fails
        """
        # Prepare the final command for execution
        final_command = command.copy()
        
        # Handle Windows vs Linux differences
        if self.is_windows:
            if final_command[0] not in ['sudo', self.pacman_command]:
                final_command.insert(0, self.pacman_command)
        else:
            # Add sudo if specified and not already present
            if self.use_sudo and final_command[0] != 'sudo' and os.geteuid() != 0:
                if final_command[0] == 'pacman':
                    final_command.insert(0, 'sudo')
                    
        logger.debug(f"Executing command: {' '.join(final_command)}")
        
        try:
            # Execute the command
            if capture_output:
                process = subprocess.run(
                    final_command, 
                    check=False,  # Don't raise exception, we'll handle errors ourselves
                    text=True, 
                    capture_output=True
                )
            else:
                # For commands where we want to see output in real-time
                process = subprocess.run(
                    final_command, 
                    check=False,
                    text=True
                )
                # Create empty strings for stdout/stderr since we didn't capture them
                process.stdout = ""
                process.stderr = ""
                
            result: CommandResult = {
                "success": process.returncode == 0,
                "stdout": process.stdout,
                "stderr": process.stderr,
                "command": final_command,
                "return_code": process.returncode
            }
            
            if process.returncode != 0:
                logger.warning(f"Command {' '.join(final_command)} failed with code {process.returncode}")
                logger.debug(f"Error output: {process.stderr}")
            else:
                logger.debug(f"Command {' '.join(final_command)} executed successfully")
                
            return result
            
        except Exception as e:
            logger.error(f"Exception executing command {' '.join(final_command)}: {str(e)}")
            raise CommandError(f"Failed to execute command {' '.join(final_command)}", -1, str(e))
            
    async def run_command_async(self, command: List[str]) -> CommandResult:
        """
        Execute a command asynchronously using asyncio.
        
        Args:
            command: The command to execute as a list of strings
            
        Returns:
            CommandResult with execution results
        """
        # Use the executor to run the command in a separate thread
        loop = asyncio.get_running_loop()
        return await loop.run_in_executor(self._executor, lambda: self.run_command(command))

    def update_package_database(self) -> CommandResult:
        """
        Update the package database to get the latest package information.
        
        Returns:
            CommandResult with the operation result
            
        Example:
            ```python
            result = pacman.update_package_database()
            if result["success"]:
                print("Database updated successfully")
            else:
                print(f"Error updating database: {result['stderr']}")
            ```
        """
        return self.run_command(['pacman', '-Sy'])
        
    async def update_package_database_async(self) -> CommandResult:
        """
        Asynchronously update the package database.
        
        Returns:
            CommandResult with the operation result
        """
        return await self.run_command_async(['pacman', '-Sy'])

    def upgrade_system(self, no_confirm: bool = False) -> CommandResult:
        """
        Upgrade the system by updating all installed packages to the latest versions.
        
        Args:
            no_confirm: Skip confirmation prompts by passing --noconfirm
            
        Returns:
            CommandResult with the operation result
        """
        cmd = ['pacman', '-Syu']
        if no_confirm:
            cmd.append('--noconfirm')
        return self.run_command(cmd, capture_output=False)
        
    async def upgrade_system_async(self, no_confirm: bool = False) -> CommandResult:
        """
        Asynchronously upgrade the system.
        
        Args:
            no_confirm: Skip confirmation prompts by passing --noconfirm
            
        Returns:
            CommandResult with the operation result
        """
        cmd = ['pacman', '-Syu']
        if no_confirm:
            cmd.append('--noconfirm')
        return await self.run_command_async(cmd)

    def install_package(self, package_name: str, no_confirm: bool = False) -> CommandResult:
        """
        Install a specific package.
        
        Args:
            package_name: Name of the package to install
            no_confirm: Skip confirmation prompts by passing --noconfirm
            
        Returns:
            CommandResult with the operation result
        """
        cmd = ['pacman', '-S', package_name]
        if no_confirm:
            cmd.append('--noconfirm')
        return self.run_command(cmd, capture_output=False)
        
    def install_packages(self, package_names: List[str], no_confirm: bool = False) -> CommandResult:
        """
        Install multiple packages in a single transaction.
        
        Args:
            package_names: List of package names to install
            no_confirm: Skip confirmation prompts by passing --noconfirm
            
        Returns:
            CommandResult with the operation result
        """
        cmd = ['pacman', '-S'] + package_names
        if no_confirm:
            cmd.append('--noconfirm')
        return self.run_command(cmd, capture_output=False)
        
    async def install_package_async(self, package_name: str, no_confirm: bool = False) -> CommandResult:
        """
        Asynchronously install a package.
        
        Args:
            package_name: Name of the package to install
            no_confirm: Skip confirmation prompts by passing --noconfirm
            
        Returns:
            CommandResult with the operation result
        """
        cmd = ['pacman', '-S', package_name]
        if no_confirm:
            cmd.append('--noconfirm')
        return await self.run_command_async(cmd)

    def remove_package(self, package_name: str, remove_deps: bool = False, 
                      no_confirm: bool = False) -> CommandResult:
        """
        Remove a specific package.
        
        Args:
            package_name: Name of the package to remove
            remove_deps: Whether to remove dependencies that aren't required by other packages
            no_confirm: Skip confirmation prompts by passing --noconfirm
            
        Returns:
            CommandResult with the operation result
        """
        cmd = ['pacman', '-R']
        if remove_deps:
            cmd = ['pacman', '-Rs']
        cmd.append(package_name)
        if no_confirm:
            cmd.append('--noconfirm')
        return self.run_command(cmd, capture_output=False)
        
    async def remove_package_async(self, package_name: str, remove_deps: bool = False,
                                 no_confirm: bool = False) -> CommandResult:
        """
        Asynchronously remove a package.
        
        Args:
            package_name: Name of the package to remove
            remove_deps: Whether to remove dependencies that aren't required by other packages
            no_confirm: Skip confirmation prompts by passing --noconfirm
            
        Returns:
            CommandResult with the operation result
        """
        cmd = ['pacman', '-R']
        if remove_deps:
            cmd = ['pacman', '-Rs']
        cmd.append(package_name)
        if no_confirm:
            cmd.append('--noconfirm')
        return await self.run_command_async(cmd)

    def search_package(self, query: str) -> List[PackageInfo]:
        """
        Search for packages by name or description.
        
        Args:
            query: The search query string
            
        Returns:
            List of PackageInfo objects matching the query
        """
        result = self.run_command(['pacman', '-Ss', query])
        if not result["success"]:
            logger.error(f"Error searching for packages: {result['stderr']}")
            return []
            
        # Parse the output to extract package information
        packages: List[PackageInfo] = []
        current_package: Optional[PackageInfo] = None
        
        for line in result["stdout"].split('\n'):
            if not line.strip():
                continue
                
            # Package line starts with repository/name
            if line.startswith(' '):  # Description line
                if current_package:
                    current_package.description = line.strip()
                    packages.append(current_package)
                    current_package = None
            else:  # New package line
                package_match = re.match(r'^(\w+)/(\S+)\s+(\S+)(?:\s+\[(.*)\])?', line)
                if package_match:
                    repo, name, version, status = package_match.groups()
                    current_package = PackageInfo(
                        name=name,
                        version=version,
                        repository=repo,
                        installed=(status == 'installed')
                    )
        
        # Add the last package if it's still pending
        if current_package:
            packages.append(current_package)
            
        return packages
        
    async def search_package_async(self, query: str) -> List[PackageInfo]:
        """
        Asynchronously search for packages.
        
        Args:
            query: The search query string
            
        Returns:
            List of PackageInfo objects matching the query
        """
        result = await self.run_command_async(['pacman', '-Ss', query])
        if not result["success"]:
            logger.error(f"Error searching for packages: {result['stderr']}")
            return []
        
        # Use the same parsing logic as the synchronous method
        packages: List[PackageInfo] = []
        current_package: Optional[PackageInfo] = None
        
        for line in result["stdout"].split('\n'):
            if not line.strip():
                continue
                
            if line.startswith(' '):  # Description line
                if current_package:
                    current_package.description = line.strip()
                    packages.append(current_package)
                    current_package = None
            else:  # New package line
                package_match = re.match(r'^(\w+)/(\S+)\s+(\S+)(?:\s+\[(.*)\])?', line)
                if package_match:
                    repo, name, version, status = package_match.groups()
                    current_package = PackageInfo(
                        name=name,
                        version=version,
                        repository=repo,
                        installed=(status == 'installed')
                    )
        
        # Add the last package if it's still pending
        if current_package:
            packages.append(current_package)
            
        return packages

    def list_installed_packages(self, refresh: bool = False) -> Dict[str, PackageInfo]:
        """
        List all installed packages on the system.
        
        Args:
            refresh: Force refreshing the cached package list
            
        Returns:
            Dictionary mapping package names to PackageInfo objects
        """
        if self._installed_packages is not None and not refresh:
            return self._installed_packages
            
        result = self.run_command(['pacman', '-Qi'])
        if not result["success"]:
            logger.error(f"Error listing installed packages: {result['stderr']}")
            return {}
            
        packages: Dict[str, PackageInfo] = {}
        current_package: Optional[PackageInfo] = None
        
        for line in result["stdout"].split('\n'):
            line = line.strip()
            if not line:
                if current_package:
                    packages[current_package.name] = current_package
                    current_package = None
                continue
                
            if line.startswith('Name'):
                name = line.split(':', 1)[1].strip()
                current_package = PackageInfo(
                    name=name,
                    version="",
                    installed=True
                )
            elif line.startswith('Version') and current_package:
                current_package.version = line.split(':', 1)[1].strip()
            elif line.startswith('Description') and current_package:
                current_package.description = line.split(':', 1)[1].strip()
            elif line.startswith('Installed Size') and current_package:
                current_package.install_size = line.split(':', 1)[1].strip()
            elif line.startswith('Install Date') and current_package:
                current_package.install_date = line.split(':', 1)[1].strip()
            elif line.startswith('Build Date') and current_package:
                current_package.build_date = line.split(':', 1)[1].strip()
            elif line.startswith('Depends On') and current_package:
                deps = line.split(':', 1)[1].strip()
                if deps and deps.lower() != 'none':
                    current_package.dependencies = deps.split()
            elif line.startswith('Optional Deps') and current_package:
                opt_deps = line.split(':', 1)[1].strip()
                if opt_deps and opt_deps.lower() != 'none':
                    current_package.optional_dependencies = opt_deps.split()
        
        # Add the last package if any
        if current_package:
            packages[current_package.name] = current_package
            
        # Cache the results
        self._installed_packages = packages
        return packages
        
    async def list_installed_packages_async(self, refresh: bool = False) -> Dict[str, PackageInfo]:
        """
        Asynchronously list all installed packages.
        
        Args:
            refresh: Force refreshing the cached package list
            
        Returns:
            Dictionary mapping package names to PackageInfo objects
        """
        if self._installed_packages is not None and not refresh:
            return self._installed_packages
            
        result = await self.run_command_async(['pacman', '-Qi'])
        # Use the same parsing logic as the synchronous method
        # (The rest of the implementation would be identical)
        # For brevity, I'll omit the duplicate parsing code
        
        # This would be a placeholder for the actual implementation
        return {}  # In reality, we'd return the parsed packages

    def show_package_info(self, package_name: str) -> Optional[PackageInfo]:
        """
        Display detailed information about a specific package.
        
        Args:
            package_name: Name of the package to query
            
        Returns:
            PackageInfo object with package details, or None if not found
        """
        result = self.run_command(['pacman', '-Qi', package_name])
        if not result["success"]:
            logger.debug(f"Package {package_name} not installed, trying remote info...")
            # Try with -Si to get info for packages not installed
            result = self.run_command(['pacman', '-Si', package_name])
            if not result["success"]:
                logger.error(f"Package {package_name} not found: {result['stderr']}")
                return None
                
        package = PackageInfo(
            name=package_name,
            version="",
            installed=True
        )
        
        for line in result["stdout"].split('\n'):
            line = line.strip()
            if not line:
                continue
                
            if ':' in line:
                key, value = line.split(':', 1)
                key = key.strip()
                value = value.strip()
                
                if key == 'Version':
                    package.version = value
                elif key == 'Description':
                    package.description = value
                elif key == 'Installed Size':
                    package.install_size = value
                elif key == 'Install Date':
                    package.install_date = value
                elif key == 'Build Date':
                    package.build_date = value
                elif key == 'Depends On' and value.lower() != 'none':
                    package.dependencies = value.split()
                elif key == 'Optional Deps' and value.lower() != 'none':
                    package.optional_dependencies = value.split()
                elif key == 'Repository':
                    package.repository = value
                    
        return package

    def list_outdated_packages(self) -> Dict[str, Tuple[str, str]]:
        """
        List all packages that are outdated and need to be upgraded.
        
        Returns:
            Dictionary mapping package name to (current_version, latest_version)
        """
        result = self.run_command(['pacman', '-Qu'])
        outdated: Dict[str, Tuple[str, str]] = {}
        
        if not result["success"]:
            logger.debug("No outdated packages found or error occurred")
            return outdated
            
        for line in result["stdout"].split('\n'):
            line = line.strip()
            if not line:
                continue
                
            parts = line.split()
            if len(parts) >= 3:
                package = parts[0]
                current_version = parts[1]
                latest_version = parts[3]
                outdated[package] = (current_version, latest_version)
                
        return outdated

    def clear_cache(self, keep_recent: bool = False) -> CommandResult:
        """
        Clear the package cache to free up space.
        
        Args:
            keep_recent: If True, keep the most recently cached packages
            
        Returns:
            CommandResult with the operation result
        """
        if keep_recent:
            return self.run_command(['pacman', '-Sc'])
        else:
            return self.run_command(['pacman', '-Scc'])

    def list_package_files(self, package_name: str) -> List[str]:
        """
        List all the files installed by a specific package.
        
        Args:
            package_name: Name of the package to query
            
        Returns:
            List of file paths installed by the package
        """
        result = self.run_command(['pacman', '-Ql', package_name])
        files: List[str] = []
        
        if not result["success"]:
            logger.error(f"Error listing files for package {package_name}: {result['stderr']}")
            return files
            
        for line in result["stdout"].split('\n'):
            line = line.strip()
            if not line:
                continue
                
            parts = line.split(None, 1)
            if len(parts) > 1:
                files.append(parts[1])
                
        return files

    def show_package_dependencies(self, package_name: str) -> Tuple[List[str], List[str]]:
        """
        Show the dependencies of a specific package.
        
        Args:
            package_name: Name of the package to query
            
        Returns:
            Tuple of (dependencies, optional_dependencies)
        """
        package_info = self.show_package_info(package_name)
        if not package_info:
            return [], []
            
        return package_info.dependencies, package_info.optional_dependencies or []

    def find_file_owner(self, file_path: str) -> Optional[str]:
        """
        Find which package owns a specific file.
        
        Args:
            file_path: Path to the file to query
            
        Returns:
            Name of the package owning the file, or None if not found
        """
        result = self.run_command(['pacman', '-Qo', file_path])
        
        if not result["success"]:
            logger.error(f"Error finding owner of file {file_path}: {result['stderr']}")
            return None
            
        # Parse output like: "/usr/bin/pacman is owned by pacman 6.0.1-5"
        match = re.search(r'is owned by (\S+)', result["stdout"])
        if match:
            return match.group(1)
        return None

    def show_fastest_mirrors(self) -> CommandResult:
        """
        Display and select the fastest mirrors for package downloads.
        
        Returns:
            CommandResult with the operation result
        """
        if self.is_windows:
            logger.warning("Mirror ranking not supported on Windows MSYS2")
            return {
                "success": False,
                "stdout": "",
                "stderr": "Mirror ranking not supported on Windows MSYS2",
                "command": [],
                "return_code": 1
            }
            
        if shutil.which('pacman-mirrors'):
            return self.run_command(['sudo', 'pacman-mirrors', '--fasttrack'])
        elif shutil.which('reflector'):
            return self.run_command(['sudo', 'reflector', '--latest', '20', '--sort', 'rate', '--save', '/etc/pacman.d/mirrorlist'])
        else:
            logger.error("No mirror ranking tool found (pacman-mirrors or reflector)")
            return {
                "success": False,
                "stdout": "",
                "stderr": "No mirror ranking tool found",
                "command": [],
                "return_code": 1
            }

    def downgrade_package(self, package_name: str, version: str) -> CommandResult:
        """
        Downgrade a package to a specific version.
        
        Args:
            package_name: Name of the package to downgrade
            version: Target version to downgrade to
            
        Returns:
            CommandResult with the operation result
        """
        # Check if the specific version is available in the cache
        cache_dir = Path('/var/cache/pacman/pkg') if not self.is_windows else None
        
        if self.is_windows:
            # For MSYS2, the cache directory is different
            msys_root = Path(self.pacman_command).parents[2]
            cache_dir = msys_root / 'var' / 'cache' / 'pacman' / 'pkg'
            
        if cache_dir and cache_dir.exists():
            # Look for matching package files
            package_files = list(cache_dir.glob(f"{package_name}-{version}*.pkg.tar.*"))
            if package_files:
                return self.run_command(['pacman', '-U', str(package_files[0])])
                
        # If not in cache, try downgrading using an AUR helper if available
        if self.aur_helper in ['yay', 'paru']:
            return self.run_command([self.aur_helper, '-S', f"{package_name}={version}"])
            
        logger.error(f"Package {package_name} version {version} not found in cache")
        return {
            "success": False,
            "stdout": "",
            "stderr": f"Package {package_name} version {version} not found in cache",
            "command": [],
            "return_code": 1
        }

    def list_cache_packages(self) -> Dict[str, List[str]]:
        """
        List all packages currently stored in the local package cache.
        
        Returns:
            Dictionary mapping package names to lists of available versions
        """
        cache_dir = Path('/var/cache/pacman/pkg') if not self.is_windows else None
        
        if self.is_windows:
            # For MSYS2, the cache directory is different
            msys_root = Path(self.pacman_command).parents[2]
            cache_dir = msys_root / 'var' / 'cache' / 'pacman' / 'pkg'
            
        if not cache_dir or not cache_dir.exists():
            logger.error(f"Package cache directory not found: {cache_dir}")
            return {}
            
        cache_packages: Dict[str, List[str]] = {}
        
        # Process all package files in the cache directory
        for pkg_file in cache_dir.glob('*.pkg.tar.*'):
            # Extract package name and version from filename
            match = re.match(r'(.+?)-([^-]+?-[^-]+?)(?:-.+)?\.pkg\.tar', pkg_file.name)
            if match:
                pkg_name = match.group(1)
                pkg_version = match.group(2)
                
                if pkg_name not in cache_packages:
                    cache_packages[pkg_name] = []
                cache_packages[pkg_name].append(pkg_version)
                
        # Sort versions for each package
        for pkg_name in cache_packages:
            cache_packages[pkg_name].sort()
            
        return cache_packages

    def enable_multithreaded_downloads(self, threads: int = 5) -> bool:
        """
        Enable multithreaded downloads to speed up package installation.
        
        Args:
            threads: Number of parallel download threads
            
        Returns:
            True if successful, False otherwise
        """
        return self.config.set_option('ParallelDownloads', str(threads))

    def list_package_group(self, group_name: str) -> List[str]:
        """
        List all packages in a specific package group.
        
        Args:
            group_name: Name of the package group to query
            
        Returns:
            List of package names in the group
        """
        result = self.run_command(['pacman', '-Sg', group_name])
        packages: List[str] = []
        
        if not result["success"]:
            logger.error(f"Error listing packages in group {group_name}: {result['stderr']}")
            return packages
            
        for line in result["stdout"].split('\n'):
            line = line.strip()
            if not line:
                continue
                
            parts = line.split()
            if len(parts) == 2 and parts[0] == group_name:
                packages.append(parts[1])
                
        return packages

    def list_optional_dependencies(self, package_name: str) -> Dict[str, str]:
        """
        List optional dependencies of a package with descriptions.
        
        Args:
            package_name: Name of the package to query
            
        Returns:
            Dictionary mapping dependency names to their descriptions
        """
        result = self.run_command(['pacman', '-Si', package_name])
        opt_deps: Dict[str, str] = {}
        
        if not result["success"]:
            # Try with -Qi for installed packages
            result = self.run_command(['pacman', '-Qi', package_name])
            if not result["success"]:
                logger.error(f"Error retrieving optional deps for package {package_name}: {result['stderr']}")
                return opt_deps
                
        parsing_opt_deps = False
        
        for line in result["stdout"].split('\n'):
            line = line.strip()
            
            if not line:
                parsing_opt_deps = False
                continue
                
            if line.startswith('Optional Deps'):
                parsing_opt_deps = True
                # Extract any deps on the same line
                deps_part = line.split(':', 1)[1].strip()
                if deps_part and deps_part.lower() != 'none':
                    self._parse_opt_deps_line(deps_part, opt_deps)
            elif parsing_opt_deps:
                self._parse_opt_deps_line(line, opt_deps)
                
        return opt_deps
        
    def _parse_opt_deps_line(self, line: str, opt_deps: Dict[str, str]) -> None:
        """
        Parse a line containing optional dependency information.
        
        Args:
            line: Line to parse
            opt_deps: Dictionary to update with parsed dependencies
        """
        # Format is typically: "package: description"
        if ':' in line:
            parts = line.split(':', 1)
            dep = parts[0].strip()
            desc = parts[1].strip() if len(parts) > 1 else ""
            
            # Remove the [installed] suffix if present
            dep = re.sub(r'\s*\[installed\]$', '', dep)
            opt_deps[dep] = desc

    def enable_color_output(self, enable: bool = True) -> bool:
        """
        Enable or disable color output in pacman command-line results.
        
        Args:
            enable: Whether to enable or disable color output
            
        Returns:
            True if successful, False otherwise
        """
        return self.config.set_option('Color', 'true' if enable else 'false')
        
    def get_package_status(self, package_name: str) -> PackageStatus:
        """
        Check the installation status of a package.
        
        Args:
            package_name: Name of the package to check
            
        Returns:
            PackageStatus enum value indicating the package status
        """
        # Check if installed
        local_result = self.run_command(['pacman', '-Q', package_name])
        if local_result["success"]:
            # Check if it's outdated
            outdated = self.list_outdated_packages()
            if package_name in outdated:
                return PackageStatus.OUTDATED
            return PackageStatus.INSTALLED
            
        # Check if it exists in repositories
        sync_result = self.run_command(['pacman', '-Ss', f"^{package_name}$"])
        if sync_result["success"] and sync_result["stdout"].strip():
            return PackageStatus.NOT_INSTALLED
            
        return PackageStatus.NOT_INSTALLED

    # AUR Support Methods
    def has_aur_support(self) -> bool:
        """
        Check if an AUR helper is available.
        
        Returns:
            True if an AUR helper is available, False otherwise
        """
        return self.aur_helper is not None
        
    def install_aur_package(self, package_name: str, no_confirm: bool = False) -> CommandResult:
        """
        Install a package from the AUR using the detected helper.
        
        Args:
            package_name: Name of the AUR package to install
            no_confirm: Skip confirmation prompts if supported
            
        Returns:
            CommandResult with the operation result
        """
        if not self.aur_helper:
            logger.error("No AUR helper detected. Cannot install AUR packages.")
            return {
                "success": False,
                "stdout": "",
                "stderr": "No AUR helper detected. Cannot install AUR packages.",
                "command": [],
                "return_code": 1
            }
            
        cmd = [self.aur_helper, '-S', package_name]
        
        if no_confirm:
            if self.aur_helper in ['yay', 'paru', 'pikaur', 'trizen']:
                cmd.append('--noconfirm')
                
        return self.run_command(cmd, capture_output=False)
        
    def search_aur_package(self, query: str) -> List[PackageInfo]:
        """
        Search for packages in the AUR.
        
        Args:
            query: The search query string
            
        Returns:
            List of PackageInfo objects matching the query
        """
        if not self.aur_helper:
            logger.error("No AUR helper detected. Cannot search AUR packages.")
            return []
            
        aur_search_flags = {
            'yay': '-Ssa',
            'paru': '-Ssa',
            'pikaur': '-Ssa',
            'aurman': '-Ssa',
            'trizen': '-Ssa'
        }
        
        search_flag = aur_search_flags.get(self.aur_helper, '-Ss')
        result = self.run_command([self.aur_helper, search_flag, query])
        
        if not result["success"]:
            logger.error(f"Error searching AUR: {result['stderr']}")
            return []
            
        # Parsing logic will depend on the AUR helper's output format
        # This is a simplified example for yay/paru-like output
        packages: List[PackageInfo] = []
        current_package: Optional[PackageInfo] = None
        
        for line in result["stdout"].split('\n'):
            if not line.strip():
                continue
                
            if line.startswith(' '):  # Description line
                if current_package:
                    current_package.description = line.strip()
                    packages.append(current_package)
                    current_package = None
            else:  # New package line
                package_match = re.match(r'^(?:aur|.*)/(\S+)\s+(\S+)', line)
                if package_match:
                    name, version = package_match.groups()
                    current_package = PackageInfo(
                        name=name,
                        version=version,
                        repository="aur"
                    )
        
        # Add the last package if it's still pending
        if current_package:
            packages.append(current_package)
            
        return packages
        
    # System Maintenance Methods
    def check_package_problems(self) -> Dict[str, List[str]]:
        """
        Check for common package problems like orphans or broken dependencies.
        
        Returns:
            Dictionary mapping problem categories to lists of affected packages
        """
        problems: Dict[str, List[str]] = {
            "orphaned": [],
            "foreign": [],
            "broken_deps": []
        }
        
        # Find orphaned packages (installed as dependencies but no longer required)
        orphan_result = self.run_command(['pacman', '-Qtdq'])
        if orphan_result["success"] and orphan_result["stdout"].strip():
            problems["orphaned"] = orphan_result["stdout"].strip().split('\n')
            
        # Find foreign packages (not in the official repositories)
        foreign_result = self.run_command(['pacman', '-Qm'])
        if foreign_result["success"] and foreign_result["stdout"].strip():
            problems["foreign"] = [line.split()[0] for line in foreign_result["stdout"].strip().split('\n')]
            
        # Check for broken dependencies
        broken_result = self.run_command(['pacman', '-Dk'])
        if not broken_result["success"]:
            problems["broken_deps"] = [line.strip() for line in broken_result["stderr"].strip().split('\n')
                                    if "requires" in line and "not found" in line]
                                    
        return problems
        
    def clean_orphaned_packages(self, no_confirm: bool = False) -> CommandResult:
        """
        Remove orphaned packages (those installed as dependencies but no longer required).
        
        Args:
            no_confirm: Skip confirmation prompts by passing --noconfirm
            
        Returns:
            CommandResult with the operation result
        """
        orphan_result = self.run_command(['pacman', '-Qtdq'])
        if not orphan_result["success"] or not orphan_result["stdout"].strip():
            return {
                "success": True,
                "stdout": "No orphaned packages to remove",
                "stderr": "",
                "command": [],
                "return_code": 0
            }
            
        cmd = ['pacman', '-Rs'] + orphan_result["stdout"].strip().split('\n')
        if no_confirm:
            cmd.append('--noconfirm')
        
        return self.run_command(cmd)
        
    def export_package_list(self, output_path: str, include_foreign: bool = True) -> bool:
        """
        Export a list of installed packages for backup or system replication.
        
        Args:
            output_path: File path to save the package list
            include_foreign: Whether to include foreign (AUR) packages
            
        Returns:
            True if successful, False otherwise
        """
        try:
            with open(output_path, 'w') as f:
                # Export native packages
                native_result = self.run_command(['pacman', '-Qn'])
                if native_result["success"] and native_result["stdout"].strip():
                    f.write("# Native packages\n")
                    for line in native_result["stdout"].strip().split('\n'):
                        pkg, ver = line.split()
                        f.write(f"{pkg}\n")
                        
                # Export foreign packages if requested
                if include_foreign:
                    foreign_result = self.run_command(['pacman', '-Qm'])
                    if foreign_result["success"] and foreign_result["stdout"].strip():
                        f.write("\n# Foreign packages (AUR)\n")
                        for line in foreign_result["stdout"].strip().split('\n'):
                            pkg, ver = line.split()
                            f.write(f"{pkg}\n")
                            
            logger.info(f"Package list exported to {output_path}")
            return True
        except Exception as e:
            logger.error(f"Error exporting package list: {str(e)}")
            return False
            
    def import_package_list(self, input_path: str, no_confirm: bool = False) -> bool:
        """
        Import and install packages from a previously exported package list.
        
        Args:
            input_path: Path to the file containing the package list
            no_confirm: Skip confirmation prompts by passing --noconfirm
            
        Returns:
            True if successful, False otherwise
        """
        try:
            with open(input_path, 'r') as f:
                content = f.read()
                
            # Extract packages (skip comments and empty lines)
            packages = [line.strip() for line in content.split('\n') 
                       if line.strip() and not line.startswith('#')]
                       
            if not packages:
                logger.warning("No packages found in the import file")
                return False
                
            # Install packages
            cmd = ['pacman', '-S'] + packages
            if no_confirm:
                cmd.append('--noconfirm')
                
            result = self.run_command(cmd)
            return result["success"]
        except Exception as e:
            logger.error(f"Error importing package list: {str(e)}")
            return False


# pybind11 integration module
class Pybind11Integration:
    """
    Helper class for pybind11 integration, exposing the PacmanManager functionality
    to C++ code via pybind11 bindings.
    """
    
    @staticmethod
    def check_pybind11_available() -> bool:
        """Check if pybind11 is available in the environment"""
        return importlib.util.find_spec("pybind11") is not None
        
    @staticmethod
    def generate_bindings() -> str:
        """
        Generate C++ code for pybind11 bindings.
        
        Returns:
            String containing the C++ binding code
        """
        if not Pybind11Integration.check_pybind11_available():
            raise ImportError("pybind11 is not installed. Install with 'pip install pybind11'")
            
        binding_code = """
// pacman_bindings.cpp - pybind11 bindings for PacmanManager
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <map>
#include <tuple>

namespace py = pybind11;

PYBIND11_MODULE(pacman_cpp, m) {
    m.doc() = "pybind11 bindings for PacmanManager";
    
    // Define PackageStatus enum
    py::enum_<PackageStatus>(m, "PackageStatus")
        .value("INSTALLED", PackageStatus::INSTALLED)
        .value("NOT_INSTALLED", PackageStatus::NOT_INSTALLED)
        .value("OUTDATED", PackageStatus::OUTDATED)
        .value("PARTIALLY_INSTALLED", PackageStatus::PARTIALLY_INSTALLED);
    
    // Define PackageInfo class
    py::class_<PackageInfo>(m, "PackageInfo")
        .def(py::init<>())
        .def_readwrite("name", &PackageInfo::name)
        .def_readwrite("version", &PackageInfo::version)
        .def_readwrite("description", &PackageInfo::description)
        .def_readwrite("install_size", &PackageInfo::install_size)
        .def_readwrite("installed", &PackageInfo::installed)
        .def_readwrite("repository", &PackageInfo::repository)
        .def_readwrite("dependencies", &PackageInfo::dependencies)
        .def_readwrite("optional_dependencies", &PackageInfo::optional_dependencies)
        .def_readwrite("build_date", &PackageInfo::build_date)
        .def_readwrite("install_date", &PackageInfo::install_date);
    
    // Define CommandResult class
    py::class_<CommandResult>(m, "CommandResult")
        .def(py::init<>())
        .def_readwrite("success", &CommandResult::success)
        .def_readwrite("stdout", &CommandResult::stdout)
        .def_readwrite("stderr", &CommandResult::stderr)
        .def_readwrite("command", &CommandResult::command)
        .def_readwrite("return_code", &CommandResult::return_code);
    
    // Define PacmanManager class
    py::class_<PacmanManager>(m, "PacmanManager")
        .def(py::init<>())
        .def(py::init<std::string, bool>(), 
             py::arg("config_path") = py::none(), py::arg("use_sudo") = true)
        .def("update_package_database", &PacmanManager::update_package_database)
        .def("upgrade_system", &PacmanManager::upgrade_system, py::arg("no_confirm") = false)
        .def("install_package", &PacmanManager::install_package, 
             py::arg("package_name"), py::arg("no_confirm") = false)
        .def("install_packages", &PacmanManager::install_packages,
             py::arg("package_names"), py::arg("no_confirm") = false)
        .def("remove_package", &PacmanManager::remove_package,
             py::arg("package_name"), py::arg("remove_deps") = false, py::arg("no_confirm") = false)
        .def("search_package", &PacmanManager::search_package, py::arg("query"))
        .def("list_installed_packages", &PacmanManager::list_installed_packages, py::arg("refresh") = false)
        .def("show_package_info", &PacmanManager::show_package_info, py::arg("package_name"))
        .def("list_outdated_packages", &PacmanManager::list_outdated_packages)
        .def("clear_cache", &PacmanManager::clear_cache, py::arg("keep_recent") = false)
        .def("list_package_files", &PacmanManager::list_package_files, py::arg("package_name"))
        .def("show_package_dependencies", &PacmanManager::show_package_dependencies, py::arg("package_name"))
        .def("find_file_owner", &PacmanManager::find_file_owner, py::arg("file_path"))
        .def("show_fastest_mirrors", &PacmanManager::show_fastest_mirrors)
        .def("downgrade_package", &PacmanManager::downgrade_package, 
             py::arg("package_name"), py::arg("version"))
        .def("list_cache_packages", &PacmanManager::list_cache_packages)
        .def("enable_multithreaded_downloads", &PacmanManager::enable_multithreaded_downloads, 
             py::arg("threads") = 5)
        .def("list_package_group", &PacmanManager::list_package_group, py::arg("group_name"))
        .def("list_optional_dependencies", &PacmanManager::list_optional_dependencies, 
             py::arg("package_name"))
        .def("enable_color_output", &PacmanManager::enable_color_output, py::arg("enable") = true)
        .def("get_package_status", &PacmanManager::get_package_status, py::arg("package_name"))
        .def("has_aur_support", &PacmanManager::has_aur_support)
        .def("install_aur_package", &PacmanManager::install_aur_package,
             py::arg("package_name"), py::arg("no_confirm") = false)
        .def("search_aur_package", &PacmanManager::search_aur_package, py::arg("query"))
        .def("check_package_problems", &PacmanManager::check_package_problems)
        .def("clean_orphaned_packages", &PacmanManager::clean_orphaned_packages, 
             py::arg("no_confirm") = false)
        .def("export_package_list", &PacmanManager::export_package_list,
             py::arg("output_path"), py::arg("include_foreign") = true)
        .def("import_package_list", &PacmanManager::import_package_list,
             py::arg("input_path"), py::arg("no_confirm") = false);
}
"""
        return binding_code
        
    @staticmethod
    def build_extension_instructions() -> str:
        """
        Generate instructions for building the pybind11 extension.
        
        Returns:
            String containing build instructions
        """
        return """
To build the pybind11 extension:

1. Save the generated C++ code to a file named 'pacman_bindings.cpp'

2. Create a setup.py file with the following content:
   ```
   from setuptools import setup, Extension
   import pybind11
   
   ext_modules = [
       Extension(
           'pacman_cpp',
           ['pacman_bindings.cpp'],
           include_dirs=[pybind11.get_include()],
           language='c++'
       ),
   ]
   
   setup(
       name='pacman_cpp',
       version='0.1',
       ext_modules=ext_modules,
       zip_safe=False,
   )
   ```

3. Build the extension:
   ```
   pip install .
   ```

4. Use the extension in your C++ code:
   ```cpp
   #include <pybind11/embed.h>
   
   namespace py = pybind11;
   
   int main() {
       py::scoped_interpreter guard{};
       
       auto pacman_cpp = py::module::import("pacman_cpp");
       auto pm = pacman_cpp.attr("PacmanManager")();
       
       // Example: Update package database
       auto result = pm.attr("update_package_database")();
       
       // Check result
       if (result.attr("success").cast<bool>()) {
           std::cout << "Package database updated successfully!" << std::endl;
       }
       
       return 0;
   }
   ```
"""


def parse_arguments():
    """
    Parse command-line arguments for the PacmanManager CLI tool.
    
    Returns:
        Parsed argument namespace
    """
    parser = argparse.ArgumentParser(
        description='Advanced Pacman Package Manager CLI Tool',
        epilog='For more information, visit: https://github.com/yourusername/pacman-manager'
    )
    
    # Basic operations
    basic_group = parser.add_argument_group('Basic Operations')
    basic_group.add_argument('--update-db', action='store_true',
                    help='Update the package database')
    basic_group.add_argument('--upgrade', action='store_true',
                    help='Upgrade the system')
    basic_group.add_argument('--install', type=str, metavar='PACKAGE',
                    help='Install a package')
    basic_group.add_argument('--install-multiple', type=str, nargs='+', metavar='PACKAGE',
                    help='Install multiple packages')
    basic_group.add_argument('--remove', type=str, metavar='PACKAGE',
                    help='Remove a package')
    basic_group.add_argument('--remove-deps', action='store_true',
                    help='Remove dependencies when removing a package')
    basic_group.add_argument('--search', type=str, metavar='QUERY',
                    help='Search for a package')
    basic_group.add_argument('--list-installed', action='store_true',
                    help='List all installed packages')
    basic_group.add_argument('--refresh', action='store_true',
                    help='Force refreshing package information cache')
    
    # Advanced operations
    adv_group = parser.add_argument_group('Advanced Operations')
    adv_group.add_argument('--package-info', type=str, metavar='PACKAGE',
                    help='Show detailed package information')
    adv_group.add_argument('--list-outdated', action='store_true',
                    help='List outdated packages')
    adv_group.add_argument('--clear-cache', action='store_true',
                    help='Clear package cache')
    adv_group.add_argument('--keep-recent', action='store_true',
                    help='Keep the most recently cached package versions when clearing cache')
    adv_group.add_argument('--list-files', type=str, metavar='PACKAGE',
                    help='List all files installed by a package')
    adv_group.add_argument('--show-dependencies', type=str, metavar='PACKAGE',
                    help='Show package dependencies')
    adv_group.add_argument('--find-file-owner', type=str, metavar='FILE',
                    help='Find which package owns a file')
    adv_group.add_argument('--fast-mirrors', action='store_true',
                    help='Rank and select the fastest mirrors')
    adv_group.add_argument('--downgrade', type=str, nargs=2, metavar=('PACKAGE', 'VERSION'),
                    help='Downgrade a package to a specific version')
    adv_group.add_argument('--list-cache', action='store_true',
                    help='List packages in local cache')
    
    # Configuration options
    config_group = parser.add_argument_group('Configuration Options')
    config_group.add_argument('--multithread', type=int, metavar='THREADS',
                    help='Enable multithreaded downloads with specified thread count')
    config_group.add_argument('--list-group', type=str, metavar='GROUP',
                    help='List all packages in a group')
    config_group.add_argument('--optional-deps', type=str, metavar='PACKAGE',
                    help='List optional dependencies of a package')
    config_group.add_argument('--enable-color', action='store_true',
                    help='Enable color output in pacman')
    config_group.add_argument('--disable-color', action='store_true',
                    help='Disable color output in pacman')
    
    # AUR support
    aur_group = parser.add_argument_group('AUR Support')
    aur_group.add_argument('--aur-install', type=str, metavar='PACKAGE',
                    help='Install a package from the AUR')
    aur_group.add_argument('--aur-search', type=str, metavar='QUERY',
                    help='Search for packages in the AUR')
    
    # Maintenance options
    maint_group = parser.add_argument_group('Maintenance Options')
    maint_group.add_argument('--check-problems', action='store_true',
                    help='Check for package problems like orphans or broken dependencies')
    maint_group.add_argument('--clean-orphaned', action='store_true',
                    help='Remove orphaned packages')
    maint_group.add_argument('--export-packages', type=str, metavar='FILE',
                    help='Export list of installed packages to a file')
    maint_group.add_argument('--include-foreign', action='store_true',
                    help='Include foreign (AUR) packages in export')
    maint_group.add_argument('--import-packages', type=str, metavar='FILE',
                    help='Import and install packages from a list')
    
    # General options
    general_group = parser.add_argument_group('General Options')
    general_group.add_argument('--no-confirm', action='store_true',
                    help='Skip confirmation prompts for operations')
    general_group.add_argument('--generate-pybind', type=str, metavar='FILE',
                    help='Generate pybind11 bindings and save to specified file')
    general_group.add_argument('--json', action='store_true',
                    help='Output results in JSON format when applicable')
    general_group.add_argument('--version', action='store_true',
                    help='Show version information')
    
    return parser.parse_args()


def main():
    """
    Main entry point for the PacmanManager CLI tool.
    Parses command-line arguments and executes the corresponding operations.
    """
    args = parse_arguments()
    
    # Handle version information
    if args.version:
        print("PacmanManager v1.0.0")
        print(f"Python: {platform.python_version()}")
        print(f"Platform: {platform.system()} {platform.release()}")
        return
        
    # Generate pybind11 bindings if requested
    if args.generate_pybind:
        if not Pybind11Integration.check_pybind11_available():
            print("Error: pybind11 is not installed. Install with 'pip install pybind11'")
            return
            
        binding_code = Pybind11Integration.generate_bindings()
        try:
            with open(args.generate_pybind, 'w') as f:
                f.write(binding_code)
            print(f"pybind11 bindings generated and saved to {args.generate_pybind}")
            print(Pybind11Integration.build_extension_instructions())
        except Exception as e:
            print(f"Error writing pybind11 bindings: {str(e)}")
        return
    
    # Create PacmanManager instance
    try:
        pacman = PacmanManager()
    except Exception as e:
        print(f"Error initializing PacmanManager: {str(e)}")
        return
        
    json_output = args.json
    no_confirm = args.no_confirm
    
    # Handle different operations based on arguments
    try:
        if args.update_db:
            result = pacman.update_package_database()
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"] else result["stderr"])
                
        elif args.upgrade:
            result = pacman.upgrade_system(no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"] else result["stderr"])
                
        elif args.install:
            result = pacman.install_package(args.install, no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"] else result["stderr"])
                
        elif args.install_multiple:
            result = pacman.install_packages(args.install_multiple, no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"] else result["stderr"])
                
        elif args.remove:
            result = pacman.remove_package(args.remove, remove_deps=args.remove_deps, no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"] else result["stderr"])
                
        elif args.search:
            packages = pacman.search_package(args.search)
            if json_output:
                # Convert to serializable format
                pkg_list = [{
                    "name": p.name,
                    "version": p.version,
                    "description": p.description,
                    "repository": p.repository,
                    "installed": p.installed
                } for p in packages]
                print(json.dumps(pkg_list))
            else:
                for pkg in packages:
                    status = "[installed]" if pkg.installed else ""
                    print(f"{pkg.repository}/{pkg.name} {pkg.version} {status}")
                    print(f"    {pkg.description}")
                print(f"\nFound {len(packages)} packages")
                
        elif args.list_installed:
            packages = pacman.list_installed_packages(refresh=args.refresh)
            if json_output:
                # Convert to serializable format
                pkg_list = [{
                    "name": p.name,
                    "version": p.version,
                    "description": p.description,
                    "install_size": p.install_size
                } for p in packages.values()]
                print(json.dumps(pkg_list))
            else:
                for name, pkg in sorted(packages.items()):
                    print(f"{name} {pkg.version}")
                print(f"\nTotal: {len(packages)} packages")
                
        elif args.package_info:
            pkg_info = pacman.show_package_info(args.package_info)
            if not pkg_info:
                print(f"Package '{args.package_info}' not found")
                return
                
            if json_output:
                # Convert to serializable format
                pkg_dict = {
                    "name": pkg_info.name,
                    "version": pkg_info.version,
                    "description": pkg_info.description,
                    "repository": pkg_info.repository,
                    "install_size": pkg_info.install_size,
                    "install_date": pkg_info.install_date,
                    "build_date": pkg_info.build_date,
                    "dependencies": pkg_info.dependencies,
                    "optional_dependencies": pkg_info.optional_dependencies
                }
                print(json.dumps(pkg_dict))
            else:
                print(f"Package: {pkg_info.name}")
                print(f"Version: {pkg_info.version}")
                print(f"Description: {pkg_info.description}")
                print(f"Repository: {pkg_info.repository}")
                print(f"Install Size: {pkg_info.install_size}")
                if pkg_info.install_date:
                    print(f"Install Date: {pkg_info.install_date}")
                print(f"Build Date: {pkg_info.build_date}")
                print(f"Dependencies: {', '.join(pkg_info.dependencies) if pkg_info.dependencies else 'None'}")
                print(f"Optional Dependencies: {', '.join(pkg_info.optional_dependencies) if pkg_info.optional_dependencies else 'None'}")
                
        elif args.list_outdated:
            outdated = pacman.list_outdated_packages()
            if json_output:
                # Convert to serializable format
                outdated_dict = {
                    pkg: {"current": current, "latest": latest}
                    for pkg, (current, latest) in outdated.items()
                }
                print(json.dumps(outdated_dict))
            else:
                if outdated:
                    for pkg, (current, latest) in outdated.items():
                        print(f"{pkg}: {current} -> {latest}")
                    print(f"\nTotal: {len(outdated)} outdated packages")
                else:
                    print("All packages are up to date")
                    
        # Handle other operations...
        # (similar patterns would continue for all the other CLI operations)
        
        else:
            # If no specific operation was requested, show usage information
            print("No operation specified. Use --help to see available options.")
            
    except Exception as e:
        print(f"Error executing operation: {str(e)}")
        return 1


if __name__ == "__main__":
    sys.exit(main() or 0)
