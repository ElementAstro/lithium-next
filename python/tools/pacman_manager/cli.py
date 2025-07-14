#!/usr/bin/env python3
"""
Modern Command-line interface for the Pacman Package Manager
Enhanced with latest Python features and improved UX.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, NoReturn, Optional

from loguru import logger

from .analytics import PackageAnalytics
from .cache import PackageCache
from .manager import PacmanManager
from .plugins import PluginManager


class CLI:
    """Modern command-line interface for Pacman Manager."""

    def __init__(self) -> None:
        """Initialize CLI with modern features."""
        self.parser = self._create_parser()
        self.analytics = PackageAnalytics()
        self.cache = PackageCache()
        self.plugin_manager = PluginManager()

    def _create_parser(self) -> argparse.ArgumentParser:
        """Create argument parser with modern CLI design."""
        parser = argparse.ArgumentParser(
            description="🚀 Advanced Pacman Package Manager CLI Tool",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Examples:
  %(prog)s install firefox         # Install a package
  %(prog)s search --query text     # Search packages
  %(prog)s analytics --report      # Show analytics report
            """,
        )

        # Global options
        parser.add_argument(
            "--version", action="version", version="Pacman Manager 2.0.0"
        )
        parser.add_argument(
            "--verbose",
            "-v",
            action="count",
            default=0,
            help="Increase verbosity (use -vv for debug)",
        )
        parser.add_argument("--config", type=Path, help="Custom config file path")

        # Subcommands
        subparsers = parser.add_subparsers(dest="command", help="Available commands")

        # Install command
        install_parser = subparsers.add_parser("install", help="Install packages")
        install_parser.add_argument(
            "packages", nargs="+", help="Package names to install"
        )

        # Remove command
        remove_parser = subparsers.add_parser("remove", help="Remove packages")
        remove_parser.add_argument(
            "packages", nargs="+", help="Package names to remove"
        )

        # Search command
        search_parser = subparsers.add_parser("search", help="Search packages")
        search_parser.add_argument("--query", "-q", required=True, help="Search query")
        search_parser.add_argument(
            "--limit", type=int, default=20, help="Limit number of results"
        )

        # Analytics command
        analytics_parser = subparsers.add_parser("analytics", help="Show analytics")
        analytics_parser.add_argument(
            "--report", action="store_true", help="Generate full report"
        )
        analytics_parser.add_argument(
            "--export", type=Path, help="Export metrics to file"
        )
        analytics_parser.add_argument(
            "--clear", action="store_true", help="Clear all metrics"
        )

        # Cache command
        cache_parser = subparsers.add_parser("cache", help="Cache management")
        cache_parser.add_argument("--clear", action="store_true", help="Clear cache")
        cache_parser.add_argument(
            "--stats", action="store_true", help="Show cache statistics"
        )

        return parser

    def run(self) -> int:
        """Run the CLI synchronously."""
        args = self.parser.parse_args()

        # Configure logging
        self._configure_logging(args.verbose)

        # Handle no command
        if not args.command:
            self.parser.print_help()
            return 1

        try:
            return self._execute_command(args)
        except Exception as e:
            logger.error(f"Command failed: {e}")
            return 1

    async def async_run(self) -> int:
        """Run the CLI asynchronously."""
        args = self.parser.parse_args()

        # Configure logging
        self._configure_logging(args.verbose)

        # Handle no command
        if not args.command:
            self.parser.print_help()
            return 1

        try:
            return await self._execute_command_async(args)
        except Exception as e:
            logger.error(f"Command failed: {e}")
            return 1

    def _configure_logging(self, verbose: int) -> None:
        """Configure logging based on verbosity."""
        logger.remove()

        if verbose == 0:
            level = "INFO"
        elif verbose == 1:
            level = "DEBUG"
        else:
            level = "TRACE"

        logger.add(
            sys.stderr,
            level=level,
            format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | "
            "<level>{level: <8}</level> | "
            "<cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - "
            "<level>{message}</level>",
        )

    def _execute_command(self, args: argparse.Namespace) -> int:
        """Execute command synchronously."""
        try:
            manager = PacmanManager()
            return self._handle_command(manager, args)
        except Exception as e:
            logger.error(f"Failed to initialize manager: {e}")
            return 1

    async def _execute_command_async(self, args: argparse.Namespace) -> int:
        """Execute command asynchronously."""
        try:
            from .async_manager import AsyncPacmanManager

            manager = AsyncPacmanManager()
            return await self._handle_command_async(manager, args)
        except Exception as e:
            logger.error(f"Failed to initialize async manager: {e}")
            return 1

    def _handle_command(self, manager: PacmanManager, args: argparse.Namespace) -> int:
        """Handle command execution with sync manager."""
        match args.command:
            case "install":
                return self._handle_install(manager, args)
            case "remove":
                return self._handle_remove(manager, args)
            case "search":
                return self._handle_search(manager, args)
            case "analytics":
                return self._handle_analytics(args)
            case "cache":
                return self._handle_cache(args)
            case _:
                print(f"❌ Unknown command: {args.command}")
                return 1

    async def _handle_command_async(self, manager, args: argparse.Namespace) -> int:
        """Handle command execution with async manager."""
        match args.command:
            case "install":
                return await self._handle_install_async(manager, args)
            case "remove":
                return await self._handle_remove_async(manager, args)
            case "search":
                return await self._handle_search_async(manager, args)
            case "analytics":
                return await self._handle_analytics_async(args)
            case "cache":
                return self._handle_cache(args)
            case _:
                print(f"❌ Unknown command: {args.command}")
                return 1

    def _handle_install(self, manager: PacmanManager, args: argparse.Namespace) -> int:
        """Handle package installation."""
        success_count = 0

        for package in args.packages:
            print(f"📦 Installing {package}...")
            self.analytics.start_operation("install", package)

            try:
                result = manager.install_package(package)
                # Handle different return types
                if hasattr(result, "__getitem__") and "success" in result:
                    success = result["success"]
                else:
                    success = bool(result)

                if success:
                    print(f"✅ Successfully installed {package}")
                    success_count += 1
                else:
                    print(f"❌ Failed to install {package}")

                self.analytics.end_operation("install", package, success)
            except Exception as e:
                print(f"❌ Error installing {package}: {e}")
                self.analytics.end_operation("install", package, False)

        return 0 if success_count == len(args.packages) else 1

    async def _handle_install_async(self, manager, args: argparse.Namespace) -> int:
        """Handle async package installation."""
        success_count = 0

        for package in args.packages:
            print(f"📦 Installing {package}...")
            self.analytics.start_operation("install", package)

            try:
                result = await manager.install_package(package)
                # Handle different return types
                if hasattr(result, "__getitem__") and "success" in result:
                    success = result["success"]
                else:
                    success = bool(result)

                if success:
                    print(f"✅ Successfully installed {package}")
                    success_count += 1
                    self.analytics.end_operation("install", package, True)
                else:
                    print(f"❌ Failed to install {package}")
                    self.analytics.end_operation("install", package, False)
            except Exception as e:
                print(f"❌ Error installing {package}: {e}")
                self.analytics.end_operation("install", package, False)

        return 0 if success_count == len(args.packages) else 1

    def _handle_remove(self, manager: PacmanManager, args: argparse.Namespace) -> int:
        """Handle package removal."""
        success_count = 0

        for package in args.packages:
            print(f"🗑️  Removing {package}...")
            self.analytics.start_operation("remove", package)

            try:
                result = manager.remove_package(package)
                # Handle different return types
                if hasattr(result, "__getitem__") and "success" in result:
                    success = result["success"]
                else:
                    success = bool(result)

                if success:
                    print(f"✅ Successfully removed {package}")
                    success_count += 1
                else:
                    print(f"❌ Failed to remove {package}")

                self.analytics.end_operation("remove", package, success)
            except Exception as e:
                print(f"❌ Error removing {package}: {e}")
                self.analytics.end_operation("remove", package, False)

        return 0 if success_count == len(args.packages) else 1

    async def _handle_remove_async(self, manager, args: argparse.Namespace) -> int:
        """Handle async package removal."""
        success_count = 0

        for package in args.packages:
            print(f"🗑️  Removing {package}...")
            self.analytics.start_operation("remove", package)

            try:
                result = await manager.remove_package(package)
                # Handle different return types
                if hasattr(result, "__getitem__") and "success" in result:
                    success = result["success"]
                else:
                    success = bool(result)

                if success:
                    print(f"✅ Successfully removed {package}")
                    success_count += 1
                    self.analytics.end_operation("remove", package, True)
                else:
                    print(f"❌ Failed to remove {package}")
                    self.analytics.end_operation("remove", package, False)
            except Exception as e:
                print(f"❌ Error removing {package}: {e}")
                self.analytics.end_operation("remove", package, False)

        return 0 if success_count == len(args.packages) else 1

    def _handle_search(self, manager: PacmanManager, args: argparse.Namespace) -> int:
        """Handle package search."""
        print(f"🔍 Searching for '{args.query}'...")

        try:
            # Use manager's search functionality if available
            if hasattr(manager, "search_package"):
                results = manager.search_package(args.query)
                if not isinstance(results, list):
                    results = [results] if results else []
            else:
                # Fallback message
                print("Search functionality not yet implemented in current manager")
                return 0

            if not results:
                print("No packages found.")
                return 0

            # Limit results
            if len(results) > args.limit:
                results = results[: args.limit]

            print(f"\n📋 Found {len(results)} package(s):")
            for pkg in results:
                if hasattr(pkg, "name") and hasattr(pkg, "version"):
                    print(f"  📦 {pkg.name} ({pkg.version})")
                    if hasattr(pkg, "description") and pkg.description:
                        print(f"      {pkg.description}")
                else:
                    print(f"  📦 {pkg}")
                print()

            return 0
        except Exception as e:
            print(f"❌ Search failed: {e}")
            return 1

    async def _handle_search_async(self, manager, args: argparse.Namespace) -> int:
        """Handle async package search."""
        print(f"🔍 Searching for '{args.query}'...")

        try:
            if hasattr(manager, "search_package"):
                results = await manager.search_package(args.query)
                if not isinstance(results, list):
                    results = [results] if results else []
            else:
                print("Search functionality not yet implemented in current manager")
                return 0

            if not results:
                print("No packages found.")
                return 0

            # Limit results
            if len(results) > args.limit:
                results = results[: args.limit]

            print(f"\n📋 Found {len(results)} package(s):")
            for pkg in results:
                if hasattr(pkg, "name") and hasattr(pkg, "version"):
                    print(f"  📦 {pkg.name} ({pkg.version})")
                    if hasattr(pkg, "description") and pkg.description:
                        print(f"      {pkg.description}")
                else:
                    print(f"  📦 {pkg}")
                print()

            return 0
        except Exception as e:
            print(f"❌ Search failed: {e}")
            return 1

    def _handle_analytics(self, args: argparse.Namespace) -> int:
        """Handle analytics operations."""
        if args.clear:
            self.analytics.clear_metrics()
            print("✅ Analytics cleared")
            return 0

        if args.export:
            self.analytics.export_metrics(args.export)
            print(f"✅ Metrics exported to {args.export}")
            return 0

        if args.report:
            report = self.analytics.generate_report(include_details=True)
            print(json.dumps(report, indent=2))
        else:
            stats = self.analytics.get_operation_stats()
            print("📊 Quick Analytics:")
            print(f"  Total operations: {stats.get('total_operations', 0)}")
            print(f"  Success rate: {stats.get('success_rate', 0):.2%}")
            print(f"  Avg duration: {stats.get('avg_duration', 0):.2f}s")

        return 0

    async def _handle_analytics_async(self, args: argparse.Namespace) -> int:
        """Handle async analytics operations."""
        if args.clear:
            self.analytics.clear_metrics()
            print("✅ Analytics cleared")
            return 0

        if args.export:
            self.analytics.export_metrics(args.export)
            print(f"✅ Metrics exported to {args.export}")
            return 0

        if args.report:
            report = await self.analytics.async_generate_report(include_details=True)
            print(json.dumps(report, indent=2))
        else:
            stats = self.analytics.get_operation_stats()
            print("📊 Quick Analytics:")
            print(f"  Total operations: {stats.get('total_operations', 0)}")
            print(f"  Success rate: {stats.get('success_rate', 0):.2%}")
            print(f"  Avg duration: {stats.get('avg_duration', 0):.2f}s")

        return 0

    def _handle_cache(self, args: argparse.Namespace) -> int:
        """Handle cache operations."""
        if args.clear:
            self.cache.clear_all()
            print("✅ Cache cleared")
        elif args.stats:
            stats = self.cache.get_stats()
            print("💾 Cache Statistics:")
            for key, value in stats.items():
                print(f"  {key}: {value}")
        else:
            print("❌ Please specify --clear or --stats")
            return 1

        return 0


# Legacy functions for backward compatibility
def parse_arguments():
    """Parse command-line arguments (legacy compatibility)."""
    cli = CLI()
    return cli.parser.parse_args()


def main():
    """Main function (legacy compatibility)."""
    cli = CLI()
    return cli.run()


if __name__ == "__main__":
    sys.exit(main())
