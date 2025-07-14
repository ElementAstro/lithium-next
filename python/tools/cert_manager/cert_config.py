#!/usr/bin/env python3
"""
Enhanced configuration management module with modern Python features.

This module handles loading, validating, and merging certificate configuration
from TOML files with comprehensive validation using Pydantic v2.
"""

from __future__ import annotations

import asyncio
import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

import aiofiles
from loguru import logger
from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator

try:
    import tomllib  # Python 3.11+
except ImportError:
    try:
        import tomli as tomllib  # Fallback for older Python versions
    except ImportError:
        raise ImportError(
            "Neither tomllib (Python 3.11+) nor tomli is installed. Please install tomli for TOML parsing."
        )


def _dict_to_toml(data: Dict[str, Any], indent: int = 0) -> str:
    """Simple TOML writer function."""
    lines = []
    indent_str = "  " * indent

    for key, value in data.items():
        if isinstance(value, dict):
            if indent == 0:
                lines.append(f"\n[{key}]")
            else:
                lines.append(f"\n{indent_str}[{key}]")
            lines.append(_dict_to_toml(value, indent + 1))
        elif isinstance(value, list):
            if all(isinstance(item, str) for item in value):
                formatted_list = "[" + ", ".join(f'"{item}"' for item in value) + "]"
                lines.append(f"{indent_str}{key} = {formatted_list}")
            else:
                lines.append(f"{indent_str}{key} = {value}")
        elif isinstance(value, str):
            lines.append(f'{indent_str}{key} = "{value}"')
        elif isinstance(value, (int, float, bool)):
            lines.append(
                f"{indent_str}{key} = {str(value).lower() if isinstance(value, bool) else value}"
            )
        elif value is None:
            continue  # Skip None values
        else:
            lines.append(f'{indent_str}{key} = "{str(value)}"')

    return "\n".join(lines)


from .cert_types import (
    CertificateOptions,
    CertificateType,
    HashAlgorithm,
    KeySize,
    CertificateException,
)


class ConfigurationError(CertificateException):
    """Raised when configuration is invalid or cannot be loaded."""

    pass


class ProfileConfig(BaseModel):
    """Configuration for a certificate profile using Pydantic v2."""

    model_config = ConfigDict(
        extra="forbid", validate_assignment=True, str_strip_whitespace=True
    )

    # Certificate options
    hostname: Optional[str] = Field(default=None, description="Default hostname")
    cert_dir: Optional[Path] = Field(default=None, description="Certificate directory")
    key_size: Optional[KeySize] = Field(default=None, description="RSA key size")
    hash_algorithm: Optional[HashAlgorithm] = Field(
        default=None, description="Hash algorithm"
    )
    valid_days: Optional[int] = Field(
        default=None, ge=1, le=7300, description="Validity period in days"
    )
    san_list: Optional[List[str]] = Field(
        default=None, description="Subject Alternative Names"
    )
    cert_type: Optional[CertificateType] = Field(
        default=None, description="Certificate type"
    )

    # Distinguished Name fields
    country: Optional[str] = Field(
        default=None, min_length=2, max_length=2, description="Country code"
    )
    state: Optional[str] = Field(
        default=None, min_length=1, max_length=128, description="State/Province"
    )
    locality: Optional[str] = Field(
        default=None, min_length=1, max_length=128, description="Locality/City"
    )
    organization: Optional[str] = Field(
        default=None, min_length=1, max_length=128, description="Organization"
    )
    organizational_unit: Optional[str] = Field(
        default=None, min_length=1, max_length=128, description="Organizational Unit"
    )
    email: Optional[str] = Field(
        default=None,
        pattern=r"^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$",
        description="Email address",
    )

    # Advanced options
    path_length: Optional[int] = Field(
        default=None, ge=0, le=10, description="CA path length constraint"
    )

    @field_validator("country")
    @classmethod
    def validate_country_code(cls, v: Optional[str]) -> Optional[str]:
        """Validate country code format."""
        if v is None:
            return v
        v = v.upper()
        if len(v) != 2 or not v.isalpha():
            raise ValueError("Country code must be exactly 2 letters")
        return v

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary, excluding None values."""
        return {k: v for k, v in self.model_dump().items() if v is not None}


class CertificateConfig(BaseModel):
    """Complete certificate configuration with profiles using Pydantic v2."""

    model_config = ConfigDict(
        extra="allow",  # Allow additional fields for extensibility
        validate_assignment=True,
    )

    default: ProfileConfig = Field(
        default_factory=ProfileConfig, description="Default configuration profile"
    )
    profiles: Dict[str, ProfileConfig] = Field(
        default_factory=dict, description="Named configuration profiles"
    )

    # Global settings
    config_version: str = Field(
        default="2.0", description="Configuration format version"
    )
    backup_count: int = Field(
        default=5, ge=0, le=100, description="Number of backup files to keep"
    )

    @field_validator("profiles")
    @classmethod
    def validate_profiles(cls, v: Dict[str, Any]) -> Dict[str, ProfileConfig]:
        """Validate and convert profile configurations."""
        validated_profiles = {}

        for name, profile_data in v.items():
            if isinstance(profile_data, dict):
                try:
                    validated_profiles[name] = ProfileConfig.model_validate(
                        profile_data
                    )
                except Exception as e:
                    logger.error(f"Invalid profile '{name}': {e}")
                    raise ConfigurationError(
                        f"Invalid profile configuration '{name}': {e}",
                        error_code="INVALID_PROFILE",
                        profile_name=name,
                    ) from e
            elif isinstance(profile_data, ProfileConfig):
                validated_profiles[name] = profile_data
            else:
                raise ConfigurationError(
                    f"Profile '{name}' must be a dictionary or ProfileConfig object",
                    error_code="INVALID_PROFILE_TYPE",
                    profile_name=name,
                )

        return validated_profiles


class EnhancedConfigManager:
    """
    Enhanced configuration manager with async support and comprehensive validation.

    Features:
    - Async file I/O for better performance
    - Pydantic validation for type safety
    - Profile inheritance and merging
    - Configuration backup and versioning
    - Hot-reloading support
    """

    def __init__(
        self,
        config_path: Optional[Path] = None,
        profile_name: Optional[str] = None,
        auto_create: bool = True,
    ) -> None:
        self.config_path = config_path or Path.home() / ".cert_manager" / "config.toml"
        self.profile_name = profile_name
        self.auto_create = auto_create
        self._config: Optional[CertificateConfig] = None
        self._config_cache: Dict[str, Any] = {}
        self._last_modified: Optional[float] = None

    async def load_config_async(self, force_reload: bool = False) -> CertificateConfig:
        """
        Load configuration asynchronously with caching and validation.

        Args:
            force_reload: Force reload even if cached version exists

        Returns:
            Validated certificate configuration
        """
        # Check if we need to reload
        if not force_reload and self._config and not self._should_reload():
            return self._config

        try:
            if self.config_path.exists():
                logger.debug(f"Loading configuration from {self.config_path}")

                async with aiofiles.open(self.config_path, "rb") as f:
                    content = await f.read()
                    import io

                    config_data = tomllib.load(io.BytesIO(content))

                # Update last modified time
                self._last_modified = self.config_path.stat().st_mtime

                # Validate and create configuration
                self._config = CertificateConfig.model_validate(config_data)

                logger.info(
                    f"Configuration loaded successfully with {len(self._config.profiles)} profiles"
                )

            else:
                logger.warning(f"Configuration file not found: {self.config_path}")

                if self.auto_create:
                    logger.info("Creating default configuration")
                    self._config = CertificateConfig()
                    await self.save_config_async()
                else:
                    self._config = CertificateConfig()

            return self._config

        except Exception as e:
            raise ConfigurationError(
                f"Failed to load configuration from {self.config_path}: {e}",
                error_code="CONFIG_LOAD_FAILED",
                config_path=str(self.config_path),
            ) from e

    def load_config(self, force_reload: bool = False) -> CertificateConfig:
        """Synchronous wrapper for load_config_async."""
        return asyncio.run(self.load_config_async(force_reload))

    async def save_config_async(self, backup: bool = True) -> None:
        """
        Save configuration asynchronously with optional backup.

        Args:
            backup: Whether to create a backup of existing configuration
        """
        if not self._config:
            raise ConfigurationError("No configuration to save", error_code="NO_CONFIG")

        try:
            # Ensure directory exists
            self.config_path.parent.mkdir(parents=True, exist_ok=True)

            # Create backup if requested and file exists
            if backup and self.config_path.exists():
                await self._create_backup()

            # Convert to TOML-compatible format
            config_dict = self._config.model_dump(mode="json")

            # Write configuration
            toml_content = _dict_to_toml(config_dict)
            async with aiofiles.open(self.config_path, "w", encoding="utf-8") as f:
                await f.write(toml_content)

            # Update last modified time
            self._last_modified = self.config_path.stat().st_mtime

            logger.info(f"Configuration saved to {self.config_path}")

        except Exception as e:
            raise ConfigurationError(
                f"Failed to save configuration to {self.config_path}: {e}",
                error_code="CONFIG_SAVE_FAILED",
                config_path=str(self.config_path),
            ) from e

    def save_config(self, backup: bool = True) -> None:
        """Synchronous wrapper for save_config_async."""
        asyncio.run(self.save_config_async(backup))

    async def get_options_async(
        self, cli_args: Dict[str, Any], profile_name: Optional[str] = None
    ) -> CertificateOptions:
        """
        Merge settings from default, profile, and CLI arguments asynchronously.

        The order of precedence is: CLI > profile > default.

        Args:
            cli_args: Command-line arguments
            profile_name: Profile name to use (overrides instance setting)

        Returns:
            Merged certificate options
        """
        config = await self.load_config_async()

        # Use provided profile name or instance setting
        profile = profile_name or self.profile_name

        # Start with default settings
        merged_dict = config.default.to_dict()

        # Apply profile settings if specified
        if profile:
            if profile in config.profiles:
                profile_settings = config.profiles[profile].to_dict()
                merged_dict.update(profile_settings)
                logger.debug(f"Applied profile '{profile}' settings")
            else:
                logger.warning(f"Profile '{profile}' not found in configuration")
                # List available profiles
                available = list(config.profiles.keys())
                if available:
                    logger.info(f"Available profiles: {', '.join(available)}")

        # CLI arguments override everything
        for key, value in cli_args.items():
            if value is not None:
                # Handle special conversions
                if key == "cert_dir" and isinstance(value, (str, Path)):
                    merged_dict[key] = Path(value)
                elif key == "cert_type" and isinstance(value, str):
                    merged_dict[key] = CertificateType.from_string(value)
                elif key == "key_size" and isinstance(value, (int, str)):
                    merged_dict[key] = KeySize(str(value))
                elif key == "hash_algorithm" and isinstance(value, str):
                    merged_dict[key] = HashAlgorithm(value.lower())
                elif key == "san" and isinstance(value, list):
                    # Rename 'san' to 'san_list' for compatibility
                    merged_dict["san_list"] = value
                else:
                    merged_dict[key] = value

        # Filter out keys not in CertificateOptions and None values
        valid_keys = CertificateOptions.model_fields.keys()
        filtered_dict = {
            k: v for k, v in merged_dict.items() if k in valid_keys and v is not None
        }

        try:
            return CertificateOptions.model_validate(filtered_dict)
        except Exception as e:
            raise ConfigurationError(
                f"Invalid merged configuration: {e}",
                error_code="INVALID_MERGED_CONFIG",
                merged_config=filtered_dict,
            ) from e

    def get_options(
        self, cli_args: Dict[str, Any], profile_name: Optional[str] = None
    ) -> CertificateOptions:
        """Synchronous wrapper for get_options_async."""
        return asyncio.run(self.get_options_async(cli_args, profile_name))

    async def add_profile_async(
        self, name: str, profile_config: Union[ProfileConfig, Dict[str, Any]]
    ) -> None:
        """
        Add or update a configuration profile asynchronously.

        Args:
            name: Profile name
            profile_config: Profile configuration data
        """
        config = await self.load_config_async()

        if isinstance(profile_config, dict):
            profile_config = ProfileConfig.model_validate(profile_config)

        config.profiles[name] = profile_config
        self._config = config

        await self.save_config_async()
        logger.info(f"Profile '{name}' added/updated successfully")

    def add_profile(
        self, name: str, profile_config: Union[ProfileConfig, Dict[str, Any]]
    ) -> None:
        """Synchronous wrapper for add_profile_async."""
        asyncio.run(self.add_profile_async(name, profile_config))

    async def remove_profile_async(self, name: str) -> bool:
        """
        Remove a configuration profile asynchronously.

        Args:
            name: Profile name to remove

        Returns:
            True if profile was removed, False if not found
        """
        config = await self.load_config_async()

        if name in config.profiles:
            del config.profiles[name]
            self._config = config
            await self.save_config_async()
            logger.info(f"Profile '{name}' removed successfully")
            return True
        else:
            logger.warning(f"Profile '{name}' not found")
            return False

    def remove_profile(self, name: str) -> bool:
        """Synchronous wrapper for remove_profile_async."""
        return asyncio.run(self.remove_profile_async(name))

    async def list_profiles_async(self) -> List[str]:
        """List all available profile names asynchronously."""
        config = await self.load_config_async()
        return list(config.profiles.keys())

    def list_profiles(self) -> List[str]:
        """Synchronous wrapper for list_profiles_async."""
        return asyncio.run(self.list_profiles_async())

    def _should_reload(self) -> bool:
        """Check if configuration should be reloaded based on file modification time."""
        if not self.config_path.exists():
            return False

        if self._last_modified is None:
            return True

        current_mtime = self.config_path.stat().st_mtime
        return current_mtime > self._last_modified

    async def _create_backup(self) -> None:
        """Create a backup of the current configuration file."""
        if not self._config:
            return

        import datetime

        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = self.config_path.with_suffix(f".backup_{timestamp}.toml")

        try:
            # Copy current config to backup
            async with aiofiles.open(self.config_path, "rb") as src:
                content = await src.read()

            async with aiofiles.open(backup_path, "wb") as dst:
                await dst.write(content)

            logger.debug(f"Configuration backup created: {backup_path}")

            # Clean up old backups
            await self._cleanup_old_backups()

        except Exception as e:
            logger.warning(f"Failed to create configuration backup: {e}")

    async def _cleanup_old_backups(self) -> None:
        """Clean up old backup files, keeping only the most recent ones."""
        if not self._config:
            return

        backup_pattern = f"{self.config_path.stem}.backup_*.toml"
        backup_dir = self.config_path.parent

        try:
            import glob

            backup_files = list(backup_dir.glob(backup_pattern))
            backup_files.sort(key=lambda p: p.stat().st_mtime, reverse=True)

            # Keep only the most recent backups
            files_to_remove = backup_files[self._config.backup_count :]

            for backup_file in files_to_remove:
                backup_file.unlink()
                logger.debug(f"Removed old backup: {backup_file}")

        except Exception as e:
            logger.warning(f"Failed to cleanup old backups: {e}")


# Backward compatibility alias
ConfigManager = EnhancedConfigManager
