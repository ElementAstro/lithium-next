#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Factory for creating build system implementations.
"""

from pathlib import Path
from typing import Any, Dict, Optional, List, Union

from loguru import logger

from ..core.base import BuildHelperBase
from ..builders.cmake import CMakeBuilder
from ..builders.meson import MesonBuilder
from ..builders.bazel import BazelBuilder


class BuilderFactory:
    """
    Factory class for creating builder instances based on the specified build system.

    This class provides a centralized way to create builder instances, ensuring that
    the correct builder type is created based on the specified build system.
    """

    @staticmethod
    def create_builder(
        builder_type: str,
        source_dir: Union[Path, str],
        build_dir: Union[Path, str],
        **kwargs: Any
    ) -> BuildHelperBase:
        """
        Create a builder instance for the specified build system.

        Args:
            builder_type (str): The type of build system to use ("cmake", "meson", "bazel").
            source_dir (Union[Path, str]): Path to the source directory.
            build_dir (Union[Path, str]): Path to the build directory.
            **kwargs: Additional keyword arguments to pass to the builder constructor.

        Returns:
            BuildHelperBase: A builder instance of the specified type.

        Raises:
            ValueError: If the specified builder type is not supported.
        """
        match builder_type.lower():
            case "cmake":
                logger.info(
                    f"Creating CMake builder for source directory: {source_dir}")
                return CMakeBuilder(source_dir, build_dir, **kwargs)
            case "meson":
                logger.info(
                    f"Creating Meson builder for source directory: {source_dir}")
                return MesonBuilder(source_dir, build_dir, **kwargs)
            case "bazel":
                logger.info(
                    f"Creating Bazel builder for source directory: {source_dir}")
                return BazelBuilder(source_dir, build_dir, **kwargs)
            case _:
                logger.error(f"Unsupported builder type: {builder_type}")
                raise ValueError(f"Unsupported builder type: {builder_type}")
