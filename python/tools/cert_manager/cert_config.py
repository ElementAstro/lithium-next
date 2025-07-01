#!/usr/bin/env python3
"""
Configuration Management Module.

This module handles loading and merging of certificate configuration
from a TOML file.
"""

from pathlib import Path
from typing import Any, Dict, Optional

import toml
from loguru import logger

from .cert_types import CertificateOptions, CertificateType


class ConfigManager:
    """Manages loading and merging of configuration profiles."""

    def __init__(self, config_path: Path, profile_name: Optional[str] = None):
        self._config = self._load_config(config_path)
        self._profile_name = profile_name

    def _load_config(self, config_path: Path) -> Dict[str, Any]:
        """Loads the TOML configuration file."""
        if config_path.exists():
            logger.info(f"Loading configuration from {config_path}")
            return toml.load(config_path)
        return {}

    def get_options(self, cli_args: Dict[str, Any]) -> CertificateOptions:
        """
        Merges settings from default, profile, and CLI arguments.

        The order of precedence is: CLI > profile > default.
        """
        default_settings = self._config.get("default", {})
        profile_settings = {}
        if self._profile_name:
            profile_settings = self._config.get("profiles", {}).get(self._profile_name, {})
            if not profile_settings:
                logger.warning(f"Profile '{self._profile_name}' not found in config.")

        # Merge settings
        merged = {**default_settings, **profile_settings}

        # CLI arguments override config file settings
        for key, value in cli_args.items():
            if value is not None:
                merged[key] = value

        # Ensure cert_dir is a Path object
        if "cert_dir" in merged:
            merged["cert_dir"] = Path(merged["cert_dir"])

        # Convert cert_type from string to Enum
        if "cert_type" in merged:
            merged["cert_type"] = CertificateType.from_string(merged["cert_type"])

        # Rename 'san' to 'san_list' to match CertificateOptions
        if 'san' in merged:
            merged['san_list'] = merged.pop('san')

        # Filter out keys not in CertificateOptions
        valid_keys = CertificateOptions.__annotations__.keys()
        filtered_merged = {k: v for k, v in merged.items() if k in valid_keys}

        return CertificateOptions(**filtered_merged)
