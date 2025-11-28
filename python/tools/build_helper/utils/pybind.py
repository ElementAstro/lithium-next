#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pybind11 integration helpers for embedding the build system in C++.

This module provides adapter classes and functions for exposing build_helper
functionality to C++ code via pybind11 bindings.
"""

import sys
import asyncio
from typing import Dict, Any, List, Optional
from pathlib import Path

from loguru import logger


def create_python_module() -> Dict[str, Any]:
    """
    Create a Python module for pybind11 embedding.

    This function sets up the necessary classes and functions for exposing
    the build system helper functionality to C++ code via pybind11.

    Returns:
        Dict[str, Any]: Dictionary containing the module components.
    """
    from ..builders.cmake import CMakeBuilder
    from ..builders.meson import MesonBuilder
    from ..builders.bazel import BazelBuilder
    from ..utils.factory import BuilderFactory
    from ..utils.config import BuildConfig
    from ..core.models import BuildResult, BuildStatus
    from .. import __version__

    logger.info("Creating Python module for pybind11 embedding")

    return {
        "BuilderFactory": BuilderFactory,
        "CMakeBuilder": CMakeBuilder,
        "MesonBuilder": MesonBuilder,
        "BazelBuilder": BazelBuilder,
        "BuildConfig": BuildConfig,
        "BuildResult": BuildResult,
        "BuildStatus": BuildStatus,
        "__version__": __version__
    }


class BuildHelperPyBindAdapter:
    """
    Adapter class to expose BuildHelper functionality to C++ via pybind11.

    This class simplifies the interface and handles conversions between Python
    and C++ types. All methods return dictionaries/lists for pybind11 compatibility,
    and exceptions are handled internally with structured error results.
    """

    @staticmethod
    def configure(
        builder_type: str,
        source_dir: str,
        build_dir: str,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Configure a build system (synchronous).

        Args:
            builder_type: Type of builder ("cmake", "meson", or "bazel")
            source_dir: Path to source directory
            build_dir: Path to build directory
            **kwargs: Additional builder-specific options

        Returns:
            Dict containing success status, output, error message, and execution time.
        """
        logger.info(
            f"C++ binding: Configuring {builder_type} in {build_dir}")
        try:
            from ..utils.factory import BuilderFactory

            builder = BuilderFactory.create_builder(
                builder_type=builder_type,
                source_dir=source_dir,
                build_dir=build_dir,
                **kwargs
            )

            result = builder.configure()

            return {
                "success": result.success,
                "output": result.output,
                "error": result.error,
                "exit_code": result.exit_code,
                "execution_time": result.execution_time
            }
        except Exception as e:
            logger.exception(f"Error in configure: {e}")
            return {
                "success": False,
                "output": "",
                "error": str(e),
                "exit_code": 1,
                "execution_time": 0.0
            }

    @staticmethod
    def build(
        builder_type: str,
        source_dir: str,
        build_dir: str,
        target: str = "",
        **kwargs
    ) -> Dict[str, Any]:
        """
        Build the project (synchronous).

        Args:
            builder_type: Type of builder ("cmake", "meson", or "bazel")
            source_dir: Path to source directory
            build_dir: Path to build directory
            target: Build target (optional)
            **kwargs: Additional builder-specific options

        Returns:
            Dict containing success status, output, error message, and execution time.
        """
        logger.info(
            f"C++ binding: Building {builder_type} project in {build_dir}")
        try:
            from ..utils.factory import BuilderFactory

            builder = BuilderFactory.create_builder(
                builder_type=builder_type,
                source_dir=source_dir,
                build_dir=build_dir,
                **kwargs
            )

            result = builder.build(target=target)

            return {
                "success": result.success,
                "output": result.output,
                "error": result.error,
                "exit_code": result.exit_code,
                "execution_time": result.execution_time
            }
        except Exception as e:
            logger.exception(f"Error in build: {e}")
            return {
                "success": False,
                "output": "",
                "error": str(e),
                "exit_code": 1,
                "execution_time": 0.0
            }

    @staticmethod
    def install(
        builder_type: str,
        source_dir: str,
        build_dir: str,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Install the project (synchronous).

        Args:
            builder_type: Type of builder ("cmake", "meson", or "bazel")
            source_dir: Path to source directory
            build_dir: Path to build directory
            **kwargs: Additional builder-specific options

        Returns:
            Dict containing success status, output, error message, and execution time.
        """
        logger.info(
            f"C++ binding: Installing {builder_type} project from {build_dir}")
        try:
            from ..utils.factory import BuilderFactory

            builder = BuilderFactory.create_builder(
                builder_type=builder_type,
                source_dir=source_dir,
                build_dir=build_dir,
                **kwargs
            )

            result = builder.install()

            return {
                "success": result.success,
                "output": result.output,
                "error": result.error,
                "exit_code": result.exit_code,
                "execution_time": result.execution_time
            }
        except Exception as e:
            logger.exception(f"Error in install: {e}")
            return {
                "success": False,
                "output": "",
                "error": str(e),
                "exit_code": 1,
                "execution_time": 0.0
            }

    @staticmethod
    def test(
        builder_type: str,
        source_dir: str,
        build_dir: str,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Run tests (synchronous).

        Args:
            builder_type: Type of builder ("cmake", "meson", or "bazel")
            source_dir: Path to source directory
            build_dir: Path to build directory
            **kwargs: Additional builder-specific options

        Returns:
            Dict containing success status, output, error message, and execution time.
        """
        logger.info(
            f"C++ binding: Testing {builder_type} project in {build_dir}")
        try:
            from ..utils.factory import BuilderFactory

            builder = BuilderFactory.create_builder(
                builder_type=builder_type,
                source_dir=source_dir,
                build_dir=build_dir,
                **kwargs
            )

            result = builder.test()

            return {
                "success": result.success,
                "output": result.output,
                "error": result.error,
                "exit_code": result.exit_code,
                "execution_time": result.execution_time
            }
        except Exception as e:
            logger.exception(f"Error in test: {e}")
            return {
                "success": False,
                "output": "",
                "error": str(e),
                "exit_code": 1,
                "execution_time": 0.0
            }

    @staticmethod
    def configure_async(
        builder_type: str,
        source_dir: str,
        build_dir: str,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Configure a build system (asynchronous).

        Args:
            builder_type: Type of builder ("cmake", "meson", or "bazel")
            source_dir: Path to source directory
            build_dir: Path to build directory
            **kwargs: Additional builder-specific options

        Returns:
            Dict containing success status, output, error message, and execution time.
        """
        logger.info(
            f"C++ binding: Async configuring {builder_type} in {build_dir}")
        try:
            from ..utils.factory import BuilderFactory

            async def _configure_async():
                builder = BuilderFactory.create_builder(
                    builder_type=builder_type,
                    source_dir=source_dir,
                    build_dir=build_dir,
                    **kwargs
                )
                # Note: configure may not have async version, use sync
                return builder.configure()

            # Run async function
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                result = loop.run_until_complete(_configure_async())
            finally:
                loop.close()

            return {
                "success": result.success,
                "output": result.output,
                "error": result.error,
                "exit_code": result.exit_code,
                "execution_time": result.execution_time
            }
        except Exception as e:
            logger.exception(f"Error in configure_async: {e}")
            return {
                "success": False,
                "output": "",
                "error": str(e),
                "exit_code": 1,
                "execution_time": 0.0
            }

    @staticmethod
    def build_async(
        builder_type: str,
        source_dir: str,
        build_dir: str,
        target: str = "",
        **kwargs
    ) -> Dict[str, Any]:
        """
        Build the project (asynchronous).

        Args:
            builder_type: Type of builder ("cmake", "meson", or "bazel")
            source_dir: Path to source directory
            build_dir: Path to build directory
            target: Build target (optional)
            **kwargs: Additional builder-specific options

        Returns:
            Dict containing success status, output, error message, and execution time.
        """
        logger.info(
            f"C++ binding: Async building {builder_type} project in {build_dir}")
        try:
            from ..utils.factory import BuilderFactory

            async def _build_async():
                builder = BuilderFactory.create_builder(
                    builder_type=builder_type,
                    source_dir=source_dir,
                    build_dir=build_dir,
                    **kwargs
                )
                # Check if builder has async build method
                if hasattr(builder, 'build_async'):
                    return await builder.build_async(target=target)
                else:
                    return builder.build(target=target)

            # Run async function
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                result = loop.run_until_complete(_build_async())
            finally:
                loop.close()

            return {
                "success": result.success,
                "output": result.output,
                "error": result.error,
                "exit_code": result.exit_code,
                "execution_time": result.execution_time
            }
        except Exception as e:
            logger.exception(f"Error in build_async: {e}")
            return {
                "success": False,
                "output": "",
                "error": str(e),
                "exit_code": 1,
                "execution_time": 0.0
            }

    @staticmethod
    def install_async(
        builder_type: str,
        source_dir: str,
        build_dir: str,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Install the project (asynchronous).

        Args:
            builder_type: Type of builder ("cmake", "meson", or "bazel")
            source_dir: Path to source directory
            build_dir: Path to build directory
            **kwargs: Additional builder-specific options

        Returns:
            Dict containing success status, output, error message, and execution time.
        """
        logger.info(
            f"C++ binding: Async installing {builder_type} project from {build_dir}")
        try:
            from ..utils.factory import BuilderFactory

            async def _install_async():
                builder = BuilderFactory.create_builder(
                    builder_type=builder_type,
                    source_dir=source_dir,
                    build_dir=build_dir,
                    **kwargs
                )
                # Note: install may not have async version, use sync
                return builder.install()

            # Run async function
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                result = loop.run_until_complete(_install_async())
            finally:
                loop.close()

            return {
                "success": result.success,
                "output": result.output,
                "error": result.error,
                "exit_code": result.exit_code,
                "execution_time": result.execution_time
            }
        except Exception as e:
            logger.exception(f"Error in install_async: {e}")
            return {
                "success": False,
                "output": "",
                "error": str(e),
                "exit_code": 1,
                "execution_time": 0.0
            }

    @staticmethod
    def test_async(
        builder_type: str,
        source_dir: str,
        build_dir: str,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Run tests (asynchronous).

        Args:
            builder_type: Type of builder ("cmake", "meson", or "bazel")
            source_dir: Path to source directory
            build_dir: Path to build directory
            **kwargs: Additional builder-specific options

        Returns:
            Dict containing success status, output, error message, and execution time.
        """
        logger.info(
            f"C++ binding: Async testing {builder_type} project in {build_dir}")
        try:
            from ..utils.factory import BuilderFactory

            async def _test_async():
                builder = BuilderFactory.create_builder(
                    builder_type=builder_type,
                    source_dir=source_dir,
                    build_dir=build_dir,
                    **kwargs
                )
                # Check if builder has async test method
                if hasattr(builder, 'test_async'):
                    return await builder.test_async()
                else:
                    return builder.test()

            # Run async function
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                result = loop.run_until_complete(_test_async())
            finally:
                loop.close()

            return {
                "success": result.success,
                "output": result.output,
                "error": result.error,
                "exit_code": result.exit_code,
                "execution_time": result.execution_time
            }
        except Exception as e:
            logger.exception(f"Error in test_async: {e}")
            return {
                "success": False,
                "output": "",
                "error": str(e),
                "exit_code": 1,
                "execution_time": 0.0
            }

    @staticmethod
    def get_builder_info(builder_type: str) -> Dict[str, Any]:
        """
        Get information about a specific builder.

        Args:
            builder_type: Type of builder ("cmake", "meson", or "bazel")

        Returns:
            Dict containing builder information.
        """
        logger.info(f"C++ binding: Getting info for {builder_type} builder")
        try:
            from ..utils.factory import BuilderFactory
            from .. import get_tool_info

            builder_map = {
                "cmake": "CMakeBuilder",
                "meson": "MesonBuilder",
                "bazel": "BazelBuilder"
            }

            if builder_type not in builder_map:
                return {
                    "success": False,
                    "error": f"Unknown builder type: {builder_type}",
                    "supported_builders": list(builder_map.keys())
                }

            tool_info = get_tool_info()
            builder_name = builder_map[builder_type]

            return {
                "success": True,
                "builder_type": builder_type,
                "builder_class": builder_name,
                "description": tool_info["classes"].get(
                    builder_name, "Build system implementation"),
                "supported_builders": list(builder_map.keys())
            }
        except Exception as e:
            logger.exception(f"Error in get_builder_info: {e}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def get_supported_builders() -> List[str]:
        """
        Get list of supported builders.

        Returns:
            List of supported builder type names.
        """
        logger.info("C++ binding: Getting supported builders")
        return ["cmake", "meson", "bazel"]

    @staticmethod
    def get_module_info() -> Dict[str, Any]:
        """
        Get complete module information.

        Returns:
            Dict containing comprehensive module metadata.
        """
        logger.info("C++ binding: Getting module info")
        try:
            from .. import get_tool_info

            return get_tool_info()
        except Exception as e:
            logger.exception(f"Error in get_module_info: {e}")
            return {
                "success": False,
                "error": str(e)
            }


# If the script is being loaded by pybind11, expose the module components
if hasattr(sys, "pybind11_module_name"):
    module_name = getattr(sys, "pybind11_module_name", None)
    logger.debug(f"Module loaded by pybind11: {module_name}")
    module_dict = create_python_module()
    for name, component in module_dict.items():
        globals()[name] = component
