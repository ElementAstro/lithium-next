#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Configuration loading utilities for the build system helper.
"""

import configparser
import json
from pathlib import Path
from typing import cast

from loguru import logger

from ..core.models import BuildOptions


class BuildConfig:
    """
    Utility class for loading and parsing build configuration from files.

    This class provides functionality to load build configuration from different file formats
    (INI, JSON, YAML) and convert it to a format usable by the builder classes.
    """

    @staticmethod
    def load_from_file(file_path: Path) -> BuildOptions:
        """
        Load build configuration from a file.

        This method determines the file format based on the file extension and calls
        the appropriate loader method.

        Args:
            file_path (Path): Path to the configuration file.

        Returns:
            BuildOptions: Dictionary containing the build options.

        Raises:
            ValueError: If the file format is not supported or the file cannot be read.
        """
        if not file_path.exists():
            raise ValueError(f"Configuration file not found: {file_path}")

        suffix = file_path.suffix.lower()
        with open(file_path, "r") as f:
            content = f.read()

        match suffix:
            case ".json":
                logger.debug(f"Loading JSON configuration from {file_path}")
                return BuildConfig.load_from_json(content)
            case ".yaml" | ".yml":
                logger.debug(f"Loading YAML configuration from {file_path}")
                return BuildConfig.load_from_yaml(content)
            case ".ini" | ".conf":
                logger.debug(f"Loading INI configuration from {file_path}")
                return BuildConfig.load_from_ini(content)
            case _:
                raise ValueError(
                    f"Unsupported configuration file format: {suffix}")

    @staticmethod
    def load_from_json(json_str: str) -> BuildOptions:
        """Load build configuration from a JSON string."""
        try:
            config = json.loads(json_str)

            # Convert string paths to Path objects
            if "source_dir" in config:
                config["source_dir"] = Path(config["source_dir"])
            if "build_dir" in config:
                config["build_dir"] = Path(config["build_dir"])
            if "install_prefix" in config:
                config["install_prefix"] = Path(config["install_prefix"])

            # Cast the dictionary to the correct type
            return cast(BuildOptions, config)

        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON configuration: {e}")
            raise ValueError(f"Invalid JSON configuration: {e}")

    @staticmethod
    def load_from_yaml(yaml_str: str) -> BuildOptions:
        """Load build configuration from a YAML string."""
        try:
            # Import yaml only when needed
            import yaml

            config = yaml.safe_load(yaml_str)

            # Convert string paths to Path objects
            if "source_dir" in config:
                config["source_dir"] = Path(config["source_dir"])
            if "build_dir" in config:
                config["build_dir"] = Path(config["build_dir"])
            if "install_prefix" in config:
                config["install_prefix"] = Path(config["install_prefix"])

            # Cast the dictionary to the correct type
            return cast(BuildOptions, config)

        except ImportError:
            logger.error("PyYAML is not installed")
            raise ValueError(
                "PyYAML is not installed. Install it with: pip install pyyaml")
        except Exception as e:
            logger.error(f"Invalid YAML configuration: {e}")
            raise ValueError(f"Invalid YAML configuration: {e}")

    @staticmethod
    def load_from_ini(ini_str: str) -> BuildOptions:
        """Load build configuration from an INI string."""
        try:
            parser = configparser.ConfigParser()
            parser.read_string(ini_str)

            if "build" not in parser:
                raise ValueError(
                    "Configuration must contain a [build] section")

            config = dict(parser["build"])

            # Convert string boolean values to actual booleans
            for key in ["verbose"]:
                if key in config:
                    config[key] = str(parser.getboolean("build", key))

            # Convert string integer values to string
            for key in ["parallel"]:
                if key in config:
                    config[key] = str(parser.getint("build", key))

            # Parse lists as comma-separated string
            for key in ["options"]:
                if key in config:
                    config[key] = ",".join(
                        [item.strip() for item in config[key].split(",")]
                    )

            # Cast the dictionary to the correct type
            return cast(BuildOptions, config)

        except (configparser.Error, ValueError) as e:
            logger.error(f"Invalid INI configuration: {e}")
            raise ValueError(f"Invalid INI configuration: {e}")