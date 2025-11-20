#!/usr/bin/env python3
"""
Core functionality for interacting with the pacman package manager
"""

import subprocess
import platform
import os
import shutil
import re
import asyncio
import concurrent.futures
from functools import lru_cache
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Set, Any

from loguru import logger

from .exceptions import CommandError
from .models import PackageInfo, CommandResult, PackageStatus
from .config import PacmanConfig


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
