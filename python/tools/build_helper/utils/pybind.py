#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pybind11 integration helpers for embedding the build system in C++.
"""

import sys
from typing import Dict, Any

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


# If the script is being loaded by pybind11, expose the module components
if hasattr(sys, "pybind11_module_name"):
    module_name = getattr(sys, "pybind11_module_name", None)
    logger.debug(f"Module loaded by pybind11: {module_name}")
    module_dict = create_python_module()
    for name, component in module_dict.items():
        globals()[name] = component