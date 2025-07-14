#!/usr/bin/env python3
"""
High-level API interface for the enhanced pacman manager.
Provides a clean, intuitive interface for common operations.
"""

from __future__ import annotations

from typing import List, Dict, Optional, Any, Union
from pathlib import Path
from contextlib import contextmanager
from collections.abc import Generator

from loguru import logger

from .manager import PacmanManager
from .async_manager import AsyncPacmanManager
from .models import PackageInfo, CommandResult, PackageStatus
from .pacman_types import PackageName, PackageVersion, SearchFilter, CommandOptions
from .context import PacmanContext, AsyncPacmanContext
from .cache import PackageCache
from .plugins import PluginManager
from .decorators import benchmark, cache_result


class PacmanAPI:
    """
    High-level API for pacman package management operations.
    Provides a clean, intuitive interface with intelligent defaults.
    """

    def __init__(
        self,
        config_path: Optional[Path] = None,
        use_sudo: bool = True,
        enable_caching: bool = True,
        enable_plugins: bool = False,
        plugin_directories: Optional[List[Path]] = None,
    ):
        """
        Initialize the Pacman API.

        Args:
            config_path: Path to pacman.conf file
            use_sudo: Whether to use sudo for privileged operations
            enable_caching: Whether to enable package caching
            enable_plugins: Whether to enable plugin system
            plugin_directories: List of directories to search for plugins
        """
        self.config_path = config_path
        self.use_sudo = use_sudo
        self.enable_caching = enable_caching
        self.enable_plugins = enable_plugins

        # Initialize components
        self._manager: Optional[PacmanManager] = None
        self._cache: Optional[PackageCache] = None
        self._plugin_manager: Optional[PluginManager] = None

        # Set up caching if enabled
        if enable_caching:
            self._cache = PackageCache()

        # Set up plugin system if enabled
        if enable_plugins:
            self._plugin_manager = PluginManager(plugin_directories or [])

    def _get_manager(self) -> PacmanManager:
        """Get or create the manager instance."""
        if self._manager is None:
            self._manager = PacmanManager(
                {"config_path": self.config_path, "use_sudo": self.use_sudo}
            )

            # Load plugins if enabled
            if self._plugin_manager:
                self._plugin_manager.load_plugins(self._manager)

        return self._manager

    @contextmanager
    def _manager_context(self) -> Generator[PacmanManager, None, None]:
        """Context manager for manager operations."""
        try:
            manager = self._get_manager()
            yield manager
        finally:
            # Cleanup if needed
            pass

    # Package Installation
    @benchmark()
    def install(
        self, package: Union[str, List[str]], no_confirm: bool = True, **options
    ) -> Union[CommandResult, Dict[str, CommandResult]]:
        """
        Install one or more packages.

        Args:
            package: Package name(s) to install
            no_confirm: Skip confirmation prompts
            **options: Additional installation options

        Returns:
            CommandResult for single package or dict for multiple packages
        """
        with self._manager_context() as manager:
            # Call pre-install hooks
            if self._plugin_manager:
                if isinstance(package, str):
                    self._plugin_manager.call_hook("before_install", package, **options)
                else:
                    for pkg in package:
                        self._plugin_manager.call_hook("before_install", pkg, **options)

            # Perform installation
            if isinstance(package, str):
                result = manager.install_package(package, no_confirm)
                success = result["success"]

                # Call post-install hooks
                if self._plugin_manager:
                    self._plugin_manager.call_hook(
                        "after_install", package, success=success
                    )

                # Invalidate cache
                if self._cache:
                    self._cache.invalidate_package(PackageName(package))

                return result
            else:
                # Multiple packages
                results = {}
                for pkg in package:
                    result = manager.install_package(pkg, no_confirm)
                    results[pkg] = result

                    # Call post-install hooks
                    if self._plugin_manager:
                        self._plugin_manager.call_hook(
                            "after_install", pkg, success=result["success"]
                        )

                    # Invalidate cache
                    if self._cache:
                        self._cache.invalidate_package(PackageName(pkg))

                return results

    @benchmark()
    def remove(
        self,
        package: Union[str, List[str]],
        remove_deps: bool = False,
        no_confirm: bool = True,
        **options,
    ) -> Union[CommandResult, Dict[str, CommandResult]]:
        """
        Remove one or more packages.

        Args:
            package: Package name(s) to remove
            remove_deps: Whether to remove dependencies
            no_confirm: Skip confirmation prompts
            **options: Additional removal options

        Returns:
            CommandResult for single package or dict for multiple packages
        """
        with self._manager_context() as manager:
            # Call pre-remove hooks
            if self._plugin_manager:
                if isinstance(package, str):
                    self._plugin_manager.call_hook("before_remove", package, **options)
                else:
                    for pkg in package:
                        self._plugin_manager.call_hook("before_remove", pkg, **options)

            # Perform removal
            if isinstance(package, str):
                result = manager.remove_package(package, remove_deps, no_confirm)
                success = result["success"]

                # Call post-remove hooks
                if self._plugin_manager:
                    self._plugin_manager.call_hook(
                        "after_remove", package, success=success
                    )

                # Invalidate cache
                if self._cache:
                    self._cache.invalidate_package(PackageName(package))

                return result
            else:
                # Multiple packages
                results = {}
                for pkg in package:
                    result = manager.remove_package(pkg, remove_deps, no_confirm)
                    results[pkg] = result

                    # Call post-remove hooks
                    if self._plugin_manager:
                        self._plugin_manager.call_hook(
                            "after_remove", pkg, success=result["success"]
                        )

                    # Invalidate cache
                    if self._cache:
                        self._cache.invalidate_package(PackageName(pkg))

                return results

    @cache_result(ttl=300)  # Cache search results for 5 minutes
    def search(
        self,
        query: str,
        limit: Optional[int] = None,
        filters: Optional[SearchFilter] = None,
    ) -> List[PackageInfo]:
        """
        Search for packages.

        Args:
            query: Search query
            limit: Maximum number of results
            filters: Additional search filters

        Returns:
            List of matching packages
        """
        with self._manager_context() as manager:
            results = manager.search_package(query)

            # Apply filters if provided
            if filters:
                results = self._apply_search_filters(results, filters)

            # Apply limit
            if limit:
                results = results[:limit]

            return results

    def _apply_search_filters(
        self, packages: List[PackageInfo], filters: SearchFilter
    ) -> List[PackageInfo]:
        """Apply search filters to package list."""
        filtered = packages

        # Filter by repository
        if "repository" in filters and filters["repository"]:
            filtered = [
                pkg for pkg in filtered if pkg.repository == filters["repository"]
            ]

        # Filter by installed status
        if "installed_only" in filters and filters["installed_only"]:
            filtered = [pkg for pkg in filtered if pkg.installed]

        # Filter by outdated status
        if "outdated_only" in filters and filters["outdated_only"]:
            filtered = [pkg for pkg in filtered if pkg.needs_update]

        # Sort by specified field
        if "sort_by" in filters:
            sort_key = filters["sort_by"]
            if sort_key == "name":
                filtered.sort(key=lambda x: x.name)
            elif sort_key == "size":
                filtered.sort(key=lambda x: x.install_size, reverse=True)
            # Add more sorting options as needed

        return filtered

    def info(self, package: str) -> Optional[PackageInfo]:
        """
        Get detailed information about a package.

        Args:
            package: Package name

        Returns:
            Package information or None if not found
        """
        # Check cache first
        if self._cache:
            cached_info = self._cache.get_package(PackageName(package))
            if cached_info:
                return cached_info

        with self._manager_context() as manager:
            installed = manager.list_installed_packages()
            info = installed.get(package) if installed else None
            if info and self._cache:
                self._cache.put_package(info)
            return info

    def list_installed(self, refresh: bool = False) -> List[PackageInfo]:
        """
        Get list of installed packages.

        Args:
            refresh: Whether to refresh the cache

        Returns:
            List of installed packages
        """
        with self._manager_context() as manager:
            installed_dict = manager.list_installed_packages(refresh)
            return list(installed_dict.values())

    def list_outdated(self) -> Dict[str, tuple[str, str]]:
        """
        Get list of outdated packages.

        Returns:
            Dictionary mapping package names to (current, available) versions
        """
        with self._manager_context() as manager:
            return manager.list_outdated_packages()

    def update_database(self) -> CommandResult:
        """
        Update the package database.

        Returns:
            Command execution result
        """
        with self._manager_context() as manager:
            result = manager.update_package_database()

            # Clear cache after database update
            if self._cache:
                self._cache.clear_all()

            return result

    def upgrade_system(self, no_confirm: bool = True) -> CommandResult:
        """
        Upgrade all packages on the system.

        Args:
            no_confirm: Skip confirmation prompts

        Returns:
            Command execution result
        """
        with self._manager_context() as manager:
            # PacmanManager does not have upgrade_system, so run the command directly
            result = manager.run_command(
                ["pacman", "-Syu", "--noconfirm"] if no_confirm else ["pacman", "-Syu"]
            )
            if self._cache:
                self._cache.clear_all()
            return result

    # Utility methods
    def is_installed(self, package: str) -> bool:
        """Check if a package is installed."""
        info = self.info(package)
        return info is not None and info.installed

    def get_version(self, package: str) -> Optional[str]:
        """Get the version of an installed package."""
        info = self.info(package)
        return str(info.version) if info and info.installed else None

    def get_dependencies(self, package: str) -> List[str]:
        """Get dependencies of a package."""
        info = self.info(package)
        return [str(dep.name) for dep in info.dependencies] if info else []

    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache statistics."""
        if self._cache:
            return self._cache.get_stats()
        return {}

    def clear_cache(self) -> None:
        """Clear all cached data."""
        if self._cache:
            self._cache.clear_all()

    def get_plugin_info(self) -> Dict[str, Dict[str, Any]]:
        """Get information about loaded plugins."""
        if self._plugin_manager:
            return self._plugin_manager.get_plugin_info()
        return {}

    # Context managers for different operation modes
    @contextmanager
    def batch_mode(self):
        """Context manager for batch operations with optimized settings."""
        # Store original settings
        original_caching = self.enable_caching

        try:
            # Optimize for batch operations
            self.enable_caching = True
            yield self
        finally:
            # Restore settings
            self.enable_caching = original_caching

    @contextmanager
    def quiet_mode(self):
        """Context manager for suppressed output operations."""
        # This would integrate with the actual manager's output settings
        yield self

    def close(self) -> None:
        """Clean up resources."""
        if self._plugin_manager:
            # Unregister all plugins
            for plugin_name in list(self._plugin_manager.plugins.keys()):
                self._plugin_manager.unregister_plugin(plugin_name)

        if self._manager and hasattr(self._manager, "_executor"):
            self._manager._executor.shutdown(wait=False)


# Async API wrapper
class AsyncPacmanAPI:
    """
    Async version of the PacmanAPI.
    """

    def __init__(self, **kwargs):
        """Initialize with same parameters as PacmanAPI."""
        self.sync_api = PacmanAPI(**kwargs)
        self._async_manager: Optional[AsyncPacmanManager] = None

    async def _get_async_manager(self) -> AsyncPacmanManager:
        """Get or create async manager."""
        if self._async_manager is None:
            self._async_manager = AsyncPacmanManager(
                self.sync_api.config_path, self.sync_api.use_sudo
            )
        return self._async_manager

    async def install(self, package: Union[str, List[str]], **kwargs) -> Any:
        """Async install packages."""
        manager = await self._get_async_manager()

        if isinstance(package, str):
            return await manager.install_package(package, **kwargs)
        else:
            return await manager.install_multiple_packages(package, **kwargs)

    async def remove(self, package: Union[str, List[str]], **kwargs) -> Any:
        """Async remove packages."""
        manager = await self._get_async_manager()

        if isinstance(package, str):
            return await manager.remove_package(package, **kwargs)
        else:
            return await manager.remove_multiple_packages(package, **kwargs)

    async def search(self, query: str, **kwargs) -> List[PackageInfo]:
        """Async search packages."""
        manager = await self._get_async_manager()
        return await manager.search_packages(query, **kwargs)

    async def info(self, package: str) -> Optional[PackageInfo]:
        """Async get package info."""
        manager = await self._get_async_manager()
        return await manager.get_package_info(package)

    async def close(self) -> None:
        """Clean up async resources."""
        if self._async_manager:
            await self._async_manager.close()
        self.sync_api.close()


# Export API classes
__all__ = [
    "PacmanAPI",
    "AsyncPacmanAPI",
]
