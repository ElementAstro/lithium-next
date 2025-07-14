#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Enhanced factory for creating build system implementations with modern Python features.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, Optional, List, Union, Type
from functools import lru_cache

from loguru import logger

from ..core.base import BuildHelperBase
from ..core.models import BuildOptions
from ..core.errors import ConfigurationError, ErrorContext
from ..builders.cmake import CMakeBuilder
from ..builders.meson import MesonBuilder
from ..builders.bazel import BazelBuilder


class BuilderFactory:
    """
    Enhanced factory class for creating builder instances with auto-detection and validation.

    This class provides a centralized way to create builder instances, ensuring that
    the correct builder type is created based on the specified build system or auto-detection.
    """

    # Registry of available builders
    _BUILDERS: Dict[str, Type[BuildHelperBase]] = {
        "cmake": CMakeBuilder,
        "meson": MesonBuilder,
        "bazel": BazelBuilder,
    }

    # File patterns for auto-detection
    _BUILD_FILE_PATTERNS = {
        "cmake": ["CMakeLists.txt", "cmake"],
        "meson": ["meson.build", "meson_options.txt"],
        "bazel": ["BUILD", "BUILD.bazel", "WORKSPACE", "WORKSPACE.bazel"],
    }

    @classmethod
    def register_builder(cls, name: str, builder_class: Type[BuildHelperBase]) -> None:
        """Register a new builder type."""
        cls._BUILDERS[name.lower()] = builder_class
        logger.debug(f"Registered builder: {name} -> {builder_class.__name__}")

    @classmethod
    def get_available_builders(cls) -> List[str]:
        """Get list of available builder types."""
        return list(cls._BUILDERS.keys())

    @classmethod
    def create_builder(
        cls,
        builder_type: str,
        source_dir: Union[Path, str],
        build_dir: Union[Path, str],
        **kwargs: Any
    ) -> BuildHelperBase:
        """
        Create a builder instance for the specified build system.

        Args:
            builder_type: The type of build system to use.
            source_dir: Path to the source directory.
            build_dir: Path to the build directory.
            **kwargs: Additional keyword arguments to pass to the builder constructor.

        Returns:
            BuildHelperBase: A builder instance of the specified type.

        Raises:
            ConfigurationError: If the specified builder type is not supported.
        """
        builder_key = builder_type.lower()
        
        if builder_key not in cls._BUILDERS:
            available = ', '.join(cls._BUILDERS.keys())
            raise ConfigurationError(
                f"Unsupported builder type: {builder_type}. Available builders: {available}",
                context=ErrorContext(working_directory=Path(source_dir))
            )

        builder_class = cls._BUILDERS[builder_key]
        
        try:
            logger.info(f"Creating {builder_type.upper()} builder for source directory: {source_dir}")
            
            # Create builder instance
            builder = builder_class(
                source_dir=source_dir,
                build_dir=build_dir,
                **kwargs
            )
            
            logger.debug(f"Successfully created {builder_class.__name__} instance")
            return builder
            
        except Exception as e:
            raise ConfigurationError(
                f"Failed to create {builder_type} builder: {e}",
                context=ErrorContext(
                    working_directory=Path(source_dir),
                    additional_info={"builder_type": builder_type}
                )
            )

    @classmethod
    def create_from_options(cls, builder_type: str, options: BuildOptions) -> BuildHelperBase:
        """
        Create a builder instance from BuildOptions.

        Args:
            builder_type: The type of build system to use.
            options: BuildOptions object containing configuration.

        Returns:
            BuildHelperBase: A builder instance of the specified type.
        """
        # Extract specific options for the builder
        builder_kwargs = {
            "install_prefix": options.install_prefix,
            "env_vars": options.env_vars,
            "verbose": options.verbose,
            "parallel": options.parallel,
        }

        # Add builder-specific options
        if builder_type.lower() == "cmake":
            builder_kwargs.update({
                "generator": options.get("generator", "Ninja"),
                "build_type": options.build_type,
                "cmake_options": options.options,
            })
        elif builder_type.lower() == "meson":
            builder_kwargs.update({
                "build_type": options.get("meson_build_type", options.build_type),
                "meson_options": options.options,
            })
        elif builder_type.lower() == "bazel":
            builder_kwargs.update({
                "build_mode": options.get("bazel_mode", "dbg"),
                "bazel_options": options.options,
            })

        return cls.create_builder(
            builder_type=builder_type,
            source_dir=options.source_dir,
            build_dir=options.build_dir,
            **builder_kwargs
        )

    @classmethod
    @lru_cache(maxsize=128)
    def detect_build_system(cls, source_dir: Union[Path, str]) -> Optional[str]:
        """
        Auto-detect the build system based on files in the source directory.

        Args:
            source_dir: Path to the source directory to analyze.

        Returns:
            Detected build system name or None if none detected.
        """
        search_path = Path(source_dir)
        
        if not search_path.exists():
            logger.warning(f"Source directory does not exist: {search_path}")
            return None

        detected_systems = []
        
        for build_system, patterns in cls._BUILD_FILE_PATTERNS.items():
            for pattern in patterns:
                # Check for exact file matches
                if (search_path / pattern).exists():
                    detected_systems.append(build_system)
                    logger.debug(f"Detected {build_system} build system (found {pattern})")
                    break
                
                # Check for directory matches
                if (search_path / pattern).is_dir():
                    detected_systems.append(build_system)
                    logger.debug(f"Detected {build_system} build system (found {pattern}/ directory)")
                    break

        if not detected_systems:
            logger.debug(f"No build system detected in {search_path}")
            return None
        elif len(detected_systems) == 1:
            logger.info(f"Auto-detected build system: {detected_systems[0]}")
            return detected_systems[0]
        else:
            # Multiple build systems detected, prefer in order of sophistication
            preference_order = ["bazel", "meson", "cmake"]
            for preferred in preference_order:
                if preferred in detected_systems:
                    logger.info(f"Multiple build systems detected, preferring: {preferred}")
                    return preferred
            
            # Fallback to first detected
            logger.warning(f"Multiple build systems detected: {detected_systems}, using {detected_systems[0]}")
            return detected_systems[0]

    @classmethod
    def create_auto_detected(
        cls,
        source_dir: Union[Path, str],
        build_dir: Union[Path, str],
        **kwargs: Any
    ) -> BuildHelperBase:
        """
        Create a builder instance by auto-detecting the build system.

        Args:
            source_dir: Path to the source directory.
            build_dir: Path to the build directory.
            **kwargs: Additional keyword arguments to pass to the builder constructor.

        Returns:
            BuildHelperBase: A builder instance of the detected type.

        Raises:
            ConfigurationError: If no build system could be detected.
        """
        detected_system = cls.detect_build_system(source_dir)
        
        if detected_system is None:
            raise ConfigurationError(
                f"No supported build system detected in {source_dir}",
                context=ErrorContext(
                    working_directory=Path(source_dir),
                    additional_info={
                        "supported_patterns": cls._BUILD_FILE_PATTERNS,
                        "available_builders": list(cls._BUILDERS.keys())
                    }
                )
            )

        return cls.create_builder(
            builder_type=detected_system,
            source_dir=source_dir,
            build_dir=build_dir,
            **kwargs
        )

    @classmethod
    def validate_builder_requirements(cls, builder_type: str, source_dir: Union[Path, str]) -> List[str]:
        """
        Validate that requirements for a specific builder type are met.

        Args:
            builder_type: The type of build system to validate.
            source_dir: Path to the source directory.

        Returns:
            List of validation error messages (empty if all requirements met).
        """
        errors = []
        source_path = Path(source_dir)
        
        if not source_path.exists():
            errors.append(f"Source directory does not exist: {source_path}")
            return errors

        builder_key = builder_type.lower()
        if builder_key not in cls._BUILDERS:
            errors.append(f"Unknown builder type: {builder_type}")
            return errors

        # Check for required build files
        if builder_key in cls._BUILD_FILE_PATTERNS:
            patterns = cls._BUILD_FILE_PATTERNS[builder_key]
            found_any = False
            
            for pattern in patterns:
                if (source_path / pattern).exists():
                    found_any = True
                    break
            
            if not found_any:
                errors.append(f"No {builder_type} build files found. Expected one of: {patterns}")

        # Additional builder-specific validations
        if builder_key == "cmake":
            cmake_file = source_path / "CMakeLists.txt"
            if cmake_file.exists():
                try:
                    content = cmake_file.read_text(encoding='utf-8')
                    if not content.strip():
                        errors.append("CMakeLists.txt is empty")
                    elif "cmake_minimum_required" not in content.lower():
                        errors.append("CMakeLists.txt missing cmake_minimum_required")
                except Exception as e:
                    errors.append(f"Could not read CMakeLists.txt: {e}")

        return errors

    @classmethod
    def get_builder_info(cls, builder_type: str) -> Dict[str, Any]:
        """
        Get information about a specific builder type.

        Args:
            builder_type: The type of build system.

        Returns:
            Dictionary containing builder information.
        """
        builder_key = builder_type.lower()
        
        if builder_key not in cls._BUILDERS:
            return {"error": f"Unknown builder type: {builder_type}"}

        builder_class = cls._BUILDERS[builder_key]
        
        return {
            "name": builder_type,
            "class": builder_class.__name__,
            "module": builder_class.__module__,
            "file_patterns": cls._BUILD_FILE_PATTERNS.get(builder_key, []),
            "description": builder_class.__doc__.split('\n')[0] if builder_class.__doc__ else "No description available"
        }