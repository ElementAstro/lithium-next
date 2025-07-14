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

import argparse
import json
import sys

from package_manager import PackageManager
from common import DependencyError, PackageOperationError, VersionError

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
                print(f"Package '{args.check}' is installed, version: {pm.get_installed_version(args.check)}")
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
                    print(f"Found {len(results)} packages matching '{args.search}':")
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
