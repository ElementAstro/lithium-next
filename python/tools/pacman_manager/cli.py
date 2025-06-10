#!/usr/bin/env python3
"""
Command-line interface for the Pacman Package Manager
"""

import argparse
import json
import platform
import sys
from pathlib import Path

from loguru import logger

from .manager import PacmanManager
from .pybind_integration import Pybind11Integration


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

    Returns:
        Exit code (0 for success, non-zero for error)
    """
    args = parse_arguments()

    # Handle version information
    if args.version:
        print("PacmanManager v1.0.0")
        print(f"Python: {platform.python_version()}")
        print(f"Platform: {platform.system()} {platform.release()}")
        return 0

    # Generate pybind11 bindings if requested
    if args.generate_pybind:
        if not Pybind11Integration.check_pybind11_available():
            logger.error(
                "pybind11 is not installed. Install with 'pip install pybind11'")
            return 1

        binding_code = Pybind11Integration.generate_bindings()
        try:
            with open(args.generate_pybind, 'w') as f:
                f.write(binding_code)
            print(
                f"pybind11 bindings generated and saved to {args.generate_pybind}")
            print(Pybind11Integration.build_extension_instructions())
        except Exception as e:
            logger.error(f"Error writing pybind11 bindings: {str(e)}")
            return 1
        return 0

    # Create PacmanManager instance
    try:
        pacman = PacmanManager()
    except Exception as e:
        logger.error(f"Error initializing PacmanManager: {str(e)}")
        return 1

    json_output = args.json
    no_confirm = args.no_confirm

    # Handle different operations based on arguments
    try:
        if args.update_db:
            result = pacman.update_package_database()
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

        elif args.upgrade:
            result = pacman.upgrade_system(no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

        elif args.install:
            result = pacman.install_package(
                args.install, no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

        elif args.install_multiple:
            result = pacman.install_packages(
                args.install_multiple, no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

        elif args.remove:
            result = pacman.remove_package(
                args.remove, remove_deps=args.remove_deps, no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

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
                return 1

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
                print(
                    f"Dependencies: {', '.join(pkg_info.dependencies) if pkg_info.dependencies else 'None'}")
                print(
                    f"Optional Dependencies: {', '.join(pkg_info.optional_dependencies) if pkg_info.optional_dependencies else 'None'}")

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

        elif args.clear_cache:
            result = pacman.clear_cache(keep_recent=args.keep_recent)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

        elif args.list_files:
            files = pacman.list_package_files(args.list_files)
            if json_output:
                print(json.dumps(files))
            else:
                for file in files:
                    print(file)
                print(f"\nTotal: {len(files)} files")

        elif args.show_dependencies:
            deps, opt_deps = pacman.show_package_dependencies(
                args.show_dependencies)
            if json_output:
                print(json.dumps({"dependencies": deps,
                      "optional_dependencies": opt_deps}))
            else:
                print("Dependencies:")
                if deps:
                    for dep in deps:
                        print(f"  {dep}")
                else:
                    print("  None")

                print("\nOptional Dependencies:")
                if opt_deps:
                    for dep in opt_deps:
                        print(f"  {dep}")
                else:
                    print("  None")

        elif args.find_file_owner:
            owner = pacman.find_file_owner(args.find_file_owner)
            if json_output:
                print(json.dumps(
                    {"file": args.find_file_owner, "owner": owner}))
            else:
                if owner:
                    print(
                        f"'{args.find_file_owner}' is owned by package: {owner}")
                else:
                    print(f"No package owns '{args.find_file_owner}'")

        elif args.fast_mirrors:
            result = pacman.show_fastest_mirrors()
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

        elif args.downgrade:
            package_name, version = args.downgrade
            result = pacman.downgrade_package(package_name, version)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

        elif args.list_cache:
            cache_packages = pacman.list_cache_packages()
            if json_output:
                print(json.dumps(cache_packages))
            else:
                for pkg_name, versions in sorted(cache_packages.items()):
                    print(f"{pkg_name}:")
                    for version in versions:
                        print(f"  {version}")
                print(f"\nTotal: {len(cache_packages)} packages in cache")

        elif args.multithread:
            success = pacman.enable_multithreaded_downloads(args.multithread)
            if json_output:
                print(json.dumps(
                    {"success": success, "threads": args.multithread}))
            else:
                if success:
                    print(
                        f"Multithreaded downloads enabled with {args.multithread} threads")
                else:
                    print("Failed to enable multithreaded downloads")

        elif args.list_group:
            packages = pacman.list_package_group(args.list_group)
            if json_output:
                print(json.dumps({args.list_group: packages}))
            else:
                print(f"Packages in group '{args.list_group}':")
                for pkg in packages:
                    print(f"  {pkg}")
                print(f"\nTotal: {len(packages)} packages")

        elif args.optional_deps:
            opt_deps = pacman.list_optional_dependencies(args.optional_deps)
            if json_output:
                print(json.dumps(opt_deps))
            else:
                print(f"Optional dependencies for '{args.optional_deps}':")
                for dep, desc in opt_deps.items():
                    print(f"  {dep}: {desc}")

        elif args.enable_color:
            success = pacman.enable_color_output(True)
            if json_output:
                print(json.dumps({"success": success}))
            else:
                print(
                    "Color output enabled" if success else "Failed to enable color output")

        elif args.disable_color:
            success = pacman.enable_color_output(False)
            if json_output:
                print(json.dumps({"success": success}))
            else:
                print(
                    "Color output disabled" if success else "Failed to disable color output")

        elif args.aur_install:
            if not pacman.has_aur_support():
                logger.error(
                    "No AUR helper detected. Cannot install AUR packages.")
                return 1

            result = pacman.install_aur_package(
                args.aur_install, no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

        elif args.aur_search:
            if not pacman.has_aur_support():
                logger.error(
                    "No AUR helper detected. Cannot search AUR packages.")
                return 1

            packages = pacman.search_aur_package(args.aur_search)
            if json_output:
                # Convert to serializable format
                pkg_list = [{
                    "name": p.name,
                    "version": p.version,
                    "description": p.description,
                    "repository": p.repository
                } for p in packages]
                print(json.dumps(pkg_list))
            else:
                for pkg in packages:
                    print(f"{pkg.repository}/{pkg.name} {pkg.version}")
                    print(f"    {pkg.description}")
                print(f"\nFound {len(packages)} packages in AUR")

        elif args.check_problems:
            problems = pacman.check_package_problems()
            if json_output:
                print(json.dumps(problems))
            else:
                print("Package problems found:")
                print(f"  Orphaned packages: {len(problems['orphaned'])}")
                for pkg in problems['orphaned']:
                    print(f"    {pkg}")

                print(f"  Foreign packages: {len(problems['foreign'])}")
                for pkg in problems['foreign']:
                    print(f"    {pkg}")

                print(f"  Broken dependencies: {len(problems['broken_deps'])}")
                for dep in problems['broken_deps']:
                    print(f"    {dep}")

        elif args.clean_orphaned:
            result = pacman.clean_orphaned_packages(no_confirm=no_confirm)
            if json_output:
                print(json.dumps(result))
            else:
                print(result["stdout"] if result["success"]
                      else result["stderr"])

        elif args.export_packages:
            success = pacman.export_package_list(
                args.export_packages, include_foreign=args.include_foreign)
            if json_output:
                print(json.dumps(
                    {"success": success, "file": args.export_packages}))
            else:
                if success:
                    print(f"Package list exported to {args.export_packages}")
                else:
                    print(
                        f"Failed to export package list to {args.export_packages}")

        elif args.import_packages:
            success = pacman.import_package_list(
                args.import_packages, no_confirm=no_confirm)
            if json_output:
                print(json.dumps(
                    {"success": success, "file": args.import_packages}))
            else:
                if success:
                    print(f"Packages imported from {args.import_packages}")
                else:
                    print(
                        f"Failed to import packages from {args.import_packages}")

        else:
            # If no specific operation was requested, show usage information
            print("No operation specified. Use --help to see available options.")
            return 1

    except Exception as e:
        logger.error(f"Error executing operation: {str(e)}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
