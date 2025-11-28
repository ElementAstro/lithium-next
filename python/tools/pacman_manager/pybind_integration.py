#!/usr/bin/env python3
"""
pybind11 integration for the Pacman Package Manager

This module provides simplified interfaces for C++ bindings via pybind11,
with both synchronous and asynchronous operation support.

Author:
    Max Qian <lightapt.com>

License:
    GPL-3.0-or-later
"""

import asyncio
import platform
import importlib.util
from typing import Dict, Any, List, Optional
from dataclasses import asdict

from loguru import logger


def _check_platform_support() -> bool:
    """Check if the current platform supports pacman."""
    system = platform.system().lower()
    return system in ["linux", "windows"]


class PacmanPyBindAdapter:
    """
    Simplified adapter class for C++ pybind11 integration.

    This class provides static methods that return pybind11-compatible types
    (dict, list, str, int, bool) for easy consumption from C++ code.

    All methods handle exceptions internally and return structured results
    with success/failure indicators.
    """

    _instance: Optional["_PacmanManagerInstance"] = None

    @classmethod
    def _get_manager(cls):
        """Get or create a PacmanManager instance."""
        if not _check_platform_support():
            return None
        if cls._instance is None:
            try:
                from .manager import PacmanManager
                cls._instance = PacmanManager(use_sudo=True)
            except Exception as e:
                logger.error(f"Failed to initialize PacmanManager: {e}")
                return None
        return cls._instance

    @staticmethod
    def is_supported() -> Dict[str, Any]:
        """
        Check if pacman is supported on this platform.

        Returns:
            Dict with supported status and platform information
        """
        supported = _check_platform_support()
        return {
            "success": True,
            "supported": supported,
            "platform": platform.system(),
            "error": None if supported else "Pacman is only supported on Linux and Windows (MSYS2)"
        }

    @staticmethod
    def install_package(package: str, no_confirm: bool = True) -> Dict[str, Any]:
        """
        Install a package using pacman.

        Args:
            package: Package name to install
            no_confirm: Skip confirmation prompts (default True for automation)

        Returns:
            Dict with success status and result details
        """
        if not _check_platform_support():
            return {"success": False, "supported": False, "error": "Platform not supported"}

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return {"success": False, "error": "Failed to initialize PacmanManager"}

            result = manager.install_package(package, no_confirm=no_confirm)
            return {
                "success": result["success"],
                "stdout": result.get("stdout", ""),
                "stderr": result.get("stderr", ""),
                "return_code": result.get("return_code", -1),
                "package": package
            }
        except Exception as e:
            logger.exception(f"Error installing package {package}")
            return {"success": False, "error": str(e), "package": package}

    @staticmethod
    async def install_package_async(package: str, no_confirm: bool = True) -> Dict[str, Any]:
        """
        Asynchronously install a package using pacman.

        Args:
            package: Package name to install
            no_confirm: Skip confirmation prompts

        Returns:
            Dict with success status and result details
        """
        if not _check_platform_support():
            return {"success": False, "supported": False, "error": "Platform not supported"}

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return {"success": False, "error": "Failed to initialize PacmanManager"}

            result = await manager.install_package_async(package, no_confirm=no_confirm)
            return {
                "success": result["success"],
                "stdout": result.get("stdout", ""),
                "stderr": result.get("stderr", ""),
                "return_code": result.get("return_code", -1),
                "package": package
            }
        except Exception as e:
            logger.exception(f"Error installing package {package}")
            return {"success": False, "error": str(e), "package": package}

    @staticmethod
    def remove_package(package: str, remove_deps: bool = False,
                       no_confirm: bool = True) -> Dict[str, Any]:
        """
        Remove a package using pacman.

        Args:
            package: Package name to remove
            remove_deps: Also remove unneeded dependencies
            no_confirm: Skip confirmation prompts

        Returns:
            Dict with success status and result details
        """
        if not _check_platform_support():
            return {"success": False, "supported": False, "error": "Platform not supported"}

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return {"success": False, "error": "Failed to initialize PacmanManager"}

            result = manager.remove_package(package, remove_deps=remove_deps,
                                            no_confirm=no_confirm)
            return {
                "success": result["success"],
                "stdout": result.get("stdout", ""),
                "stderr": result.get("stderr", ""),
                "return_code": result.get("return_code", -1),
                "package": package
            }
        except Exception as e:
            logger.exception(f"Error removing package {package}")
            return {"success": False, "error": str(e), "package": package}

    @staticmethod
    async def remove_package_async(package: str, remove_deps: bool = False,
                                   no_confirm: bool = True) -> Dict[str, Any]:
        """
        Asynchronously remove a package using pacman.

        Args:
            package: Package name to remove
            remove_deps: Also remove unneeded dependencies
            no_confirm: Skip confirmation prompts

        Returns:
            Dict with success status and result details
        """
        if not _check_platform_support():
            return {"success": False, "supported": False, "error": "Platform not supported"}

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return {"success": False, "error": "Failed to initialize PacmanManager"}

            result = await manager.remove_package_async(package, remove_deps=remove_deps,
                                                        no_confirm=no_confirm)
            return {
                "success": result["success"],
                "stdout": result.get("stdout", ""),
                "stderr": result.get("stderr", ""),
                "return_code": result.get("return_code", -1),
                "package": package
            }
        except Exception as e:
            logger.exception(f"Error removing package {package}")
            return {"success": False, "error": str(e), "package": package}

    @staticmethod
    def search_package(query: str) -> List[Dict[str, Any]]:
        """
        Search for packages matching a query.

        Args:
            query: Search query string

        Returns:
            List of package information dicts
        """
        if not _check_platform_support():
            return []

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return []

            packages = manager.search_package(query)
            return [
                {
                    "name": pkg.name,
                    "version": pkg.version,
                    "description": pkg.description,
                    "repository": pkg.repository,
                    "installed": pkg.installed
                }
                for pkg in packages
            ]
        except Exception as e:
            logger.exception(f"Error searching for packages with query: {query}")
            return []

    @staticmethod
    async def search_package_async(query: str) -> List[Dict[str, Any]]:
        """
        Asynchronously search for packages.

        Args:
            query: Search query string

        Returns:
            List of package information dicts
        """
        if not _check_platform_support():
            return []

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return []

            packages = await manager.search_package_async(query)
            return [
                {
                    "name": pkg.name,
                    "version": pkg.version,
                    "description": pkg.description,
                    "repository": pkg.repository,
                    "installed": pkg.installed
                }
                for pkg in packages
            ]
        except Exception as e:
            logger.exception(f"Error searching for packages with query: {query}")
            return []

    @staticmethod
    def list_installed_packages(refresh: bool = False) -> List[Dict[str, Any]]:
        """
        List all installed packages.

        Args:
            refresh: Force refresh the package list cache

        Returns:
            List of installed package information dicts
        """
        if not _check_platform_support():
            return []

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return []

            packages = manager.list_installed_packages(refresh=refresh)
            return [
                {
                    "name": pkg.name,
                    "version": pkg.version,
                    "description": pkg.description,
                    "install_size": pkg.install_size,
                    "install_date": pkg.install_date,
                    "dependencies": pkg.dependencies,
                }
                for pkg in packages.values()
            ]
        except Exception as e:
            logger.exception("Error listing installed packages")
            return []

    @staticmethod
    async def list_installed_packages_async(refresh: bool = False) -> List[Dict[str, Any]]:
        """
        Asynchronously list all installed packages.

        Args:
            refresh: Force refresh the package list cache

        Returns:
            List of installed package information dicts
        """
        if not _check_platform_support():
            return []

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return []

            packages = await manager.list_installed_packages_async(refresh=refresh)
            return [
                {
                    "name": pkg.name,
                    "version": pkg.version,
                    "description": pkg.description,
                    "install_size": pkg.install_size,
                    "install_date": pkg.install_date,
                    "dependencies": pkg.dependencies,
                }
                for pkg in packages.values()
            ]
        except Exception as e:
            logger.exception("Error listing installed packages")
            return []

    @staticmethod
    def update_database() -> Dict[str, Any]:
        """
        Update the package database.

        Returns:
            Dict with success status and result details
        """
        if not _check_platform_support():
            return {"success": False, "supported": False, "error": "Platform not supported"}

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return {"success": False, "error": "Failed to initialize PacmanManager"}

            result = manager.update_package_database()
            return {
                "success": result["success"],
                "stdout": result.get("stdout", ""),
                "stderr": result.get("stderr", ""),
                "return_code": result.get("return_code", -1)
            }
        except Exception as e:
            logger.exception("Error updating package database")
            return {"success": False, "error": str(e)}

    @staticmethod
    async def update_database_async() -> Dict[str, Any]:
        """
        Asynchronously update the package database.

        Returns:
            Dict with success status and result details
        """
        if not _check_platform_support():
            return {"success": False, "supported": False, "error": "Platform not supported"}

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return {"success": False, "error": "Failed to initialize PacmanManager"}

            result = await manager.update_package_database_async()
            return {
                "success": result["success"],
                "stdout": result.get("stdout", ""),
                "stderr": result.get("stderr", ""),
                "return_code": result.get("return_code", -1)
            }
        except Exception as e:
            logger.exception("Error updating package database")
            return {"success": False, "error": str(e)}

    @staticmethod
    def upgrade_system(no_confirm: bool = True) -> Dict[str, Any]:
        """
        Upgrade all system packages.

        Args:
            no_confirm: Skip confirmation prompts

        Returns:
            Dict with success status and result details
        """
        if not _check_platform_support():
            return {"success": False, "supported": False, "error": "Platform not supported"}

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return {"success": False, "error": "Failed to initialize PacmanManager"}

            result = manager.upgrade_system(no_confirm=no_confirm)
            return {
                "success": result["success"],
                "stdout": result.get("stdout", ""),
                "stderr": result.get("stderr", ""),
                "return_code": result.get("return_code", -1)
            }
        except Exception as e:
            logger.exception("Error upgrading system")
            return {"success": False, "error": str(e)}

    @staticmethod
    async def upgrade_system_async(no_confirm: bool = True) -> Dict[str, Any]:
        """
        Asynchronously upgrade all system packages.

        Args:
            no_confirm: Skip confirmation prompts

        Returns:
            Dict with success status and result details
        """
        if not _check_platform_support():
            return {"success": False, "supported": False, "error": "Platform not supported"}

        try:
            manager = PacmanPyBindAdapter._get_manager()
            if manager is None:
                return {"success": False, "error": "Failed to initialize PacmanManager"}

            result = await manager.upgrade_system_async(no_confirm=no_confirm)
            return {
                "success": result["success"],
                "stdout": result.get("stdout", ""),
                "stderr": result.get("stderr", ""),
                "return_code": result.get("return_code", -1)
            }
        except Exception as e:
            logger.exception("Error upgrading system")
            return {"success": False, "error": str(e)}


class Pybind11Integration:
    """
    Helper class for pybind11 integration metadata and code generation.
    """

    @staticmethod
    def check_pybind11_available() -> bool:
        """Check if pybind11 is available in the environment."""
        return importlib.util.find_spec("pybind11") is not None

    @staticmethod
    def generate_bindings() -> str:
        """
        Generate C++ code for pybind11 bindings.

        Returns:
            String containing the C++ binding code
        """
        if not Pybind11Integration.check_pybind11_available():
            raise ImportError(
                "pybind11 is not installed. Install with 'pip install pybind11'")

        binding_code = '''
// pacman_bindings.cpp - pybind11 bindings for PacmanManager
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(pacman_manager_native, m) {
    m.doc() = "Pacman Package Manager Python bindings";

    py::class_<PacmanPyBindAdapter>(m, "PacmanAdapter")
        .def_static("is_supported", &PacmanPyBindAdapter::is_supported)
        .def_static("install_package", &PacmanPyBindAdapter::install_package,
                    py::arg("package"), py::arg("no_confirm") = true)
        .def_static("remove_package", &PacmanPyBindAdapter::remove_package,
                    py::arg("package"), py::arg("remove_deps") = false,
                    py::arg("no_confirm") = true)
        .def_static("search_package", &PacmanPyBindAdapter::search_package,
                    py::arg("query"))
        .def_static("list_installed_packages", &PacmanPyBindAdapter::list_installed_packages,
                    py::arg("refresh") = false)
        .def_static("update_database", &PacmanPyBindAdapter::update_database)
        .def_static("upgrade_system", &PacmanPyBindAdapter::upgrade_system,
                    py::arg("no_confirm") = true);
}
'''
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

1. Ensure pybind11 is installed:
   pip install pybind11

2. Create a setup.py:
   from setuptools import setup
   from pybind11.setup_helpers import Pybind11Extension

   ext_modules = [
       Pybind11Extension(
           "pacman_manager_native",
           ["pacman_bindings.cpp"],
       ),
   ]
   setup(ext_modules=ext_modules)

3. Build:
   python setup.py build_ext --inplace

Or use CMake for integration with the main Lithium-Next build system.
"""
