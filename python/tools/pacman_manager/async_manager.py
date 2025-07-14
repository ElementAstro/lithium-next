#!/usr/bin/env python3
"""
Asynchronous pacman manager with modern async/await patterns.
Provides non-blocking package management operations.
"""

from __future__ import annotations

import asyncio
import asyncio.subprocess
from typing import Optional, Dict, List, Any
from pathlib import Path

from loguru import logger

from .manager import PacmanManager
from .models import PackageInfo, CommandResult, PackageStatus
from .pacman_types import PackageName, PackageVersion, CommandOptions
from .exceptions import CommandError, PackageNotFoundError
from .decorators import async_retry_on_failure, async_benchmark, async_cache_result
from .cache import PackageCache


class AsyncPacmanManager:
    """
    Asynchronous pacman package manager with concurrent operation support.
    Built on top of the synchronous manager but provides async interface.
    """

    def __init__(self, config_path: Optional[Path] = None, use_sudo: bool = True, **kwargs):
        """Initialize the async pacman manager."""
        self._sync_manager = PacmanManager({"config_path": config_path, "use_sudo": use_sudo})
        self._semaphore = asyncio.Semaphore(5)  # Limit concurrent operations
        self._session_cache = PackageCache()

    async def __aenter__(self) -> AsyncPacmanManager:
        """Async context manager entry."""
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        """Async context manager exit with cleanup."""
        await self.close()

    async def close(self) -> None:
        """Clean up resources."""
        # Cleanup cache and other resources
        if hasattr(self._sync_manager, '_executor'):
            self._sync_manager._executor.shutdown(wait=False)

    @async_retry_on_failure(max_attempts=3)
    @async_benchmark()
    async def install_package(
        self,
        package_name: str,
        no_confirm: bool = True,
        **options: Any
    ) -> CommandResult:
        """
        Asynchronously install a package.
        """
        async with self._semaphore:
            logger.info(f"Installing package: {package_name}")

            # Run in thread pool to avoid blocking
            loop = asyncio.get_event_loop()
            result = await loop.run_in_executor(
                None,
                lambda: self._sync_manager.install_package(
                    package_name, no_confirm)
            )

            # Invalidate cache for the installed package
            self._session_cache.invalidate_package(PackageName(package_name))

            return result

    @async_retry_on_failure(max_attempts=3)
    @async_benchmark()
    async def remove_package(
        self,
        package_name: str,
        remove_deps: bool = False,
        no_confirm: bool = True
    ) -> CommandResult:
        """
        Asynchronously remove a package.
        """
        async with self._semaphore:
            logger.info(f"Removing package: {package_name}")

            loop = asyncio.get_event_loop()
            result = await loop.run_in_executor(
                None,
                lambda: self._sync_manager.remove_package(
                    package_name, remove_deps, no_confirm)
            )

            # Invalidate cache for the removed package
            self._session_cache.invalidate_package(PackageName(package_name))

            return result

    @async_cache_result(ttl=300)  # Cache for 5 minutes
    @async_benchmark()
    async def search_packages(
        self,
        query: str,
        limit: Optional[int] = None
    ) -> List[PackageInfo]:
        """
        Asynchronously search for packages.
        """
        logger.info(f"Searching packages: {query}")

        loop = asyncio.get_event_loop()
        result = await loop.run_in_executor(
            None,
            lambda: self._sync_manager.search_package(query)
        )

        if limit:
            result = result[:limit]

        return result

    @async_cache_result(ttl=600)  # Cache for 10 minutes
    async def get_package_info(self, package_name: str) -> Optional[PackageInfo]:
        """
        Asynchronously get detailed package information.
        """
        # Check session cache first
        cached_info = self._session_cache.get_package(
            PackageName(package_name))
        if cached_info:
            return cached_info

        logger.info(f"Getting package info: {package_name}")

        loop = asyncio.get_event_loop()
        def get_info():
            installed = self._sync_manager.list_installed_packages()
            return installed.get(package_name) if installed else None
        result = await loop.run_in_executor(None, get_info)

        # Cache the result
        if result:
            self._session_cache.put_package(result)

        return result

    @async_benchmark()
    async def update_database(self) -> CommandResult:
        """
        Asynchronously update package database.
        """
        logger.info("Updating package database")

        loop = asyncio.get_event_loop()
        result = await loop.run_in_executor(
            None,
            self._sync_manager.update_package_database
        )

        # Clear cache after database update
        self._session_cache.clear_all()

        return result

    @async_benchmark()
    async def upgrade_system(self, no_confirm: bool = True) -> CommandResult:
        """
        Asynchronously upgrade the entire system.
        """
        logger.info("Upgrading system")

        loop = asyncio.get_event_loop()
        def upgrade():
            return self._sync_manager.run_command(["pacman", "-Syu", "--noconfirm"] if no_confirm else ["pacman", "-Syu"])
        result = await loop.run_in_executor(None, upgrade)

        # Clear cache after system upgrade
        self._session_cache.clear_all()

        return result

    async def get_installed_packages(self) -> List[PackageInfo]:
        """
        Asynchronously get list of installed packages.
        """
        logger.info("Getting installed packages")

        loop = asyncio.get_event_loop()
        result = await loop.run_in_executor(
            None,
            lambda: list(self._sync_manager.list_installed_packages().values())
        )

        return result

    async def get_outdated_packages(self) -> Dict[str, tuple[str, str]]:
        """
        Asynchronously get list of outdated packages.
        """
        logger.info("Getting outdated packages")

        loop = asyncio.get_event_loop()
        result = await loop.run_in_executor(
            None,
            self._sync_manager.list_outdated_packages
        )

        return result

    async def install_multiple_packages(
        self,
        package_names: List[str],
        max_concurrent: int = 3,
        no_confirm: bool = True
    ) -> Dict[str, CommandResult]:
        """
        Install multiple packages concurrently with controlled parallelism.
        """
        logger.info(f"Installing {len(package_names)} packages concurrently")

        # Create semaphore for controlling concurrency
        install_semaphore = asyncio.Semaphore(max_concurrent)

        async def install_single(package_name: str) -> tuple[str, CommandResult]:
            async with install_semaphore:
                result = await self.install_package(package_name, no_confirm)
                return package_name, result

        # Create tasks for all installations
        tasks = [install_single(package) for package in package_names]

        # Execute tasks and gather results
        results = await asyncio.gather(*tasks, return_exceptions=True)

        # Process results
        final_results = {}
        for result in results:
            if isinstance(result, Exception):
                logger.error(f"Package installation failed: {result}")
                continue

            package_name, command_result = result  # type: ignore
            final_results[package_name] = command_result

        return final_results

    async def remove_multiple_packages(
        self,
        package_names: List[str],
        max_concurrent: int = 3,
        remove_deps: bool = False,
        no_confirm: bool = True
    ) -> Dict[str, CommandResult]:
        """
        Remove multiple packages concurrently with controlled parallelism.
        """
        logger.info(f"Removing {len(package_names)} packages concurrently")

        remove_semaphore = asyncio.Semaphore(max_concurrent)

        async def remove_single(package_name: str) -> tuple[str, CommandResult]:
            async with remove_semaphore:
                result = await self.remove_package(package_name, remove_deps, no_confirm)
                return package_name, result

        tasks = [remove_single(package) for package in package_names]
        results = await asyncio.gather(*tasks, return_exceptions=True)

        final_results = {}
        for result in results:
            if isinstance(result, Exception):
                logger.error(f"Package removal failed: {result}")
                continue

            package_name, command_result = result  # type: ignore
            final_results[package_name] = command_result

        return final_results

    async def batch_package_info(
        self,
        package_names: List[str],
        max_concurrent: int = 10
    ) -> Dict[str, Optional[PackageInfo]]:
        """
        Get package information for multiple packages concurrently.
        """
        logger.info(f"Getting info for {len(package_names)} packages")

        info_semaphore = asyncio.Semaphore(max_concurrent)

        async def get_single_info(package_name: str) -> tuple[str, Optional[PackageInfo]]:
            async with info_semaphore:
                info = await self.get_package_info(package_name)
                return package_name, info

        tasks = [get_single_info(package) for package in package_names]
        results = await asyncio.gather(*tasks, return_exceptions=True)

        final_results = {}
        for result in results:
            if isinstance(result, Exception):
                logger.error(f"Package info retrieval failed: {result}")
                continue

            package_name, package_info = result  # type: ignore
            final_results[package_name] = package_info

        return final_results

    async def smart_search(
        self,
        query: str,
        include_descriptions: bool = True,
        min_relevance: float = 0.1
    ) -> List[PackageInfo]:
        """
        Enhanced search with relevance scoring and filtering.
        """
        logger.info(f"Smart search for: {query}")

        # Get initial search results
        packages = await self.search_packages(query)

        # Score packages based on relevance
        scored_packages = []
        query_lower = query.lower()

        for package in packages:
            score = 0.0

            # Exact name match gets highest score
            if package.name.lower() == query_lower:
                score += 1.0
            # Name contains query
            elif query_lower in package.name.lower():
                score += 0.8

            # Description match (if enabled)
            if include_descriptions and query_lower in package.description.lower():
                score += 0.5

            # Keywords match
            if any(query_lower in keyword.lower() for keyword in package.keywords):
                score += 0.6

            if score >= min_relevance:
                scored_packages.append((score, package))

        # Sort by score (descending) and return packages
        scored_packages.sort(key=lambda x: x[0], reverse=True)
        return [package for score, package in scored_packages]

    async def health_check(self) -> Dict[str, Any]:
        """
        Perform a health check of the package system.
        """
        logger.info("Performing system health check")

        health_status = {
            'pacman_available': False,
            'database_accessible': False,
            'cache_writable': False,
            'sudo_available': False,
            'errors': []
        }

        try:
            # Check if pacman is available
            loop = asyncio.get_event_loop()

            # Test database access
            await loop.run_in_executor(
                None,
                lambda: self._sync_manager.run_command(['pacman', '--version'])
            )
            health_status['pacman_available'] = True

            # Test database query
            await loop.run_in_executor(
                None,
                lambda: self._sync_manager.run_command(['pacman', '-Q'])
            )
            health_status['database_accessible'] = True

            # Test cache directory
            cache_dir = Path.home() / '.cache' / 'pacman_manager'
            cache_dir.mkdir(parents=True, exist_ok=True)
            test_file = cache_dir / '.write_test'
            test_file.write_text('test')
            test_file.unlink()
            health_status['cache_writable'] = True

            # Test sudo (if configured)
            if self._sync_manager._config.get('use_sudo', True):
                try:
                    await loop.run_in_executor(
                        None,
                        lambda: self._sync_manager.run_command(
                            ['sudo', '-n', 'true'])
                    )
                    health_status['sudo_available'] = True
                except Exception:
                    health_status['errors'].append(
                        'Sudo authentication required')
            else:
                health_status['sudo_available'] = True

        except Exception as e:
            health_status['errors'].append(str(e))

        return health_status


# Export the async manager
__all__ = [
    "AsyncPacmanManager",
]
