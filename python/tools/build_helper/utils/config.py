#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Enhanced configuration loading utilities with modern Python features and robust error handling.
"""

from __future__ import annotations

import configparser
import json
from pathlib import Path
from typing import Any, Dict, Union, Optional
from functools import lru_cache

from loguru import logger

from ..core.models import BuildOptions
from ..core.errors import ConfigurationError, ErrorContext


class BuildConfig:
    """
    Enhanced utility class for loading and parsing build configuration from files.

    This class provides functionality to load build configuration from different file formats
    (INI, JSON, YAML) with robust error handling, caching, and validation.
    """

    # Supported configuration file extensions
    _SUPPORTED_EXTENSIONS = {
        ".json": "json",
        ".yaml": "yaml",
        ".yml": "yaml",
        ".ini": "ini",
        ".conf": "ini",
        ".toml": "toml",
    }

    @classmethod
    def load_from_file(cls, file_path: Union[Path, str]) -> BuildOptions:
        """
        Load build configuration from a file with enhanced validation.

        Args:
            file_path: Path to the configuration file.

        Returns:
            BuildOptions: Enhanced build options object.

        Raises:
            ConfigurationError: If the file format is not supported or cannot be read.
        """
        config_path = Path(file_path)

        if not config_path.exists():
            raise ConfigurationError(
                f"Configuration file not found: {config_path}",
                config_file=config_path,
                context=ErrorContext(working_directory=config_path.parent),
            )

        if not config_path.is_file():
            raise ConfigurationError(
                f"Configuration path is not a file: {config_path}",
                config_file=config_path,
                context=ErrorContext(working_directory=config_path.parent),
            )

        suffix = config_path.suffix.lower()
        if suffix not in cls._SUPPORTED_EXTENSIONS:
            supported = ", ".join(cls._SUPPORTED_EXTENSIONS.keys())
            raise ConfigurationError(
                f"Unsupported configuration file format: {suffix}. Supported formats: {supported}",
                config_file=config_path,
                context=ErrorContext(working_directory=config_path.parent),
            )

        try:
            content = config_path.read_text(encoding="utf-8")
            logger.debug(
                f"Loading {cls._SUPPORTED_EXTENSIONS[suffix].upper()} configuration from {config_path}"
            )

            format_type = cls._SUPPORTED_EXTENSIONS[suffix]
            match format_type:
                case "json":
                    return cls.load_from_json(content, config_path)
                case "yaml":
                    return cls.load_from_yaml(content, config_path)
                case "ini":
                    return cls.load_from_ini(content, config_path)
                case "toml":
                    return cls.load_from_toml(content, config_path)
                case _:
                    raise ConfigurationError(
                        f"Internal error: unhandled format type {format_type}",
                        config_file=config_path,
                    )

        except UnicodeDecodeError as e:
            raise ConfigurationError(
                f"Failed to read configuration file (encoding error): {e}",
                config_file=config_path,
                context=ErrorContext(working_directory=config_path.parent),
            )
        except OSError as e:
            raise ConfigurationError(
                f"Failed to read configuration file: {e}",
                config_file=config_path,
                context=ErrorContext(working_directory=config_path.parent),
            )

    @classmethod
    def load_from_json(
        cls, json_str: str, source_file: Optional[Path] = None
    ) -> BuildOptions:
        """Load build configuration from a JSON string with validation."""
        try:
            config_data = json.loads(json_str)

            if not isinstance(config_data, dict):
                raise ConfigurationError(
                    "JSON configuration must be an object/dictionary",
                    config_file=source_file,
                )

            return cls._normalize_config(config_data, source_file)

        except json.JSONDecodeError as e:
            raise ConfigurationError(
                f"Invalid JSON configuration: {e}",
                config_file=source_file,
                context=ErrorContext(
                    additional_info=(
                        {"line": e.lineno, "column": e.colno}
                        if hasattr(e, "lineno")
                        else {}
                    )
                ),
            )

    @classmethod
    def load_from_yaml(
        cls, yaml_str: str, source_file: Optional[Path] = None
    ) -> BuildOptions:
        """Load build configuration from a YAML string with validation."""
        try:
            import yaml
        except ImportError:
            raise ConfigurationError(
                "PyYAML is not installed. Install it with: pip install pyyaml",
                config_file=source_file,
            )

        try:
            config_data = yaml.safe_load(yaml_str)

            if config_data is None:
                config_data = {}
            elif not isinstance(config_data, dict):
                raise ConfigurationError(
                    "YAML configuration must be a mapping/dictionary",
                    config_file=source_file,
                )

            return cls._normalize_config(config_data, source_file)

        except yaml.YAMLError as e:
            error_details = {}
            if hasattr(e, "problem_mark"):
                mark = e.problem_mark
                error_details.update({"line": mark.line + 1, "column": mark.column + 1})

            raise ConfigurationError(
                f"Invalid YAML configuration: {e}",
                config_file=source_file,
                context=ErrorContext(additional_info=error_details),
            )

    @classmethod
    def load_from_ini(
        cls, ini_str: str, source_file: Optional[Path] = None
    ) -> BuildOptions:
        """Load build configuration from an INI string with validation."""
        try:
            parser = configparser.ConfigParser()
            parser.read_string(ini_str)

            if "build" not in parser:
                raise ConfigurationError(
                    "INI configuration must contain a [build] section",
                    config_file=source_file,
                )

            config_data = dict(parser["build"])

            # Convert string values to appropriate types
            type_conversions = {
                "verbose": lambda x: parser.getboolean("build", x),
                "parallel": lambda x: parser.getint("build", x),
                "options": lambda x: [
                    item.strip() for item in config_data[x].split(",") if item.strip()
                ],
            }

            for key, converter in type_conversions.items():
                if key in config_data:
                    try:
                        config_data[key] = converter(key)
                    except ValueError as e:
                        raise ConfigurationError(
                            f"Invalid value for {key} in INI configuration: {e}",
                            config_file=source_file,
                            invalid_option=key,
                        )

            return cls._normalize_config(config_data, source_file)

        except (configparser.Error, ValueError) as e:
            raise ConfigurationError(
                f"Invalid INI configuration: {e}", config_file=source_file
            )

    @classmethod
    def load_from_toml(
        cls, toml_str: str, source_file: Optional[Path] = None
    ) -> BuildOptions:
        """Load build configuration from a TOML string with validation."""
        try:
            import tomllib  # Python 3.11+
        except ImportError:
            try:
                import tomli as tomllib  # Fallback for older Python versions
            except ImportError:
                raise ConfigurationError(
                    "TOML support requires Python 3.11+ or 'tomli' package. Install with: pip install tomli",
                    config_file=source_file,
                )

        try:
            config_data = tomllib.loads(toml_str)

            # Look for build configuration in 'build' section or root
            if "build" in config_data:
                config_data = config_data["build"]
            elif not any(key in config_data for key in ["source_dir", "build_dir"]):
                raise ConfigurationError(
                    "TOML configuration must contain build settings in root or [build] section",
                    config_file=source_file,
                )

            return cls._normalize_config(config_data, source_file)

        except Exception as e:
            raise ConfigurationError(
                f"Invalid TOML configuration: {e}", config_file=source_file
            )

    @classmethod
    def _normalize_config(
        cls, config_data: Dict[str, Any], source_file: Optional[Path] = None
    ) -> BuildOptions:
        """Normalize and validate configuration data."""
        try:
            # Convert string paths to Path objects
            path_keys = ["source_dir", "build_dir", "install_prefix"]
            for key in path_keys:
                if key in config_data and config_data[key] is not None:
                    config_data[key] = Path(config_data[key])

            # Ensure required keys exist
            if "source_dir" not in config_data:
                config_data["source_dir"] = Path(".")
            if "build_dir" not in config_data:
                config_data["build_dir"] = Path("build")

            # Validate and create BuildOptions
            return BuildOptions(config_data)

        except Exception as e:
            raise ConfigurationError(
                f"Failed to normalize configuration: {e}", config_file=source_file
            )

    @classmethod
    @lru_cache(maxsize=32)
    def get_default_config_files(cls, directory: Path) -> list[Path]:
        """Get list of potential configuration files in order of preference."""
        config_files = []
        base_names = ["build", "buildconfig", ".build"]

        for base_name in base_names:
            for ext in cls._SUPPORTED_EXTENSIONS:
                config_file = directory / f"{base_name}{ext}"
                if config_file.exists():
                    config_files.append(config_file)

        return config_files

    @classmethod
    def auto_discover_config(
        cls, start_directory: Union[Path, str]
    ) -> Optional[BuildOptions]:
        """
        Automatically discover and load configuration from common locations.

        Args:
            start_directory: Directory to start searching from

        Returns:
            BuildOptions if configuration found, None otherwise
        """
        search_dir = Path(start_directory)

        # Search current directory and parent directories
        for directory in [search_dir] + list(search_dir.parents):
            config_files = cls.get_default_config_files(directory)
            if config_files:
                logger.info(f"Auto-discovered configuration file: {config_files[0]}")
                return cls.load_from_file(config_files[0])

        logger.debug("No configuration file auto-discovered")
        return None

    @classmethod
    def merge_configs(cls, *configs: BuildOptions) -> BuildOptions:
        """
        Merge multiple configuration objects, with later configs taking precedence.

        Args:
            *configs: BuildOptions objects to merge

        Returns:
            Merged BuildOptions object
        """
        if not configs:
            return BuildOptions({})

        merged_data = {}
        for config in configs:
            merged_data.update(config.to_dict())

        return BuildOptions(merged_data)

    @classmethod
    def validate_config(cls, config: BuildOptions) -> list[str]:
        """
        Validate a configuration object and return list of warnings/issues.

        Args:
            config: BuildOptions object to validate

        Returns:
            List of validation warning messages
        """
        warnings = []

        # Check if source directory exists
        if not config.source_dir.exists():
            warnings.append(f"Source directory does not exist: {config.source_dir}")

        # Check parallel job count
        if config.parallel < 1:
            warnings.append(
                f"Parallel job count should be at least 1, got {config.parallel}"
            )
        elif config.parallel > 32:
            warnings.append(f"Parallel job count seems high: {config.parallel}")

        # Check build type
        valid_build_types = {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"}
        if (
            hasattr(config, "build_type")
            and config.get("build_type") not in valid_build_types
        ):
            warnings.append(f"Unusual build type: {config.get('build_type')}")

        return warnings
