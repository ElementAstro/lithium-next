#!/usr/bin/env python3
"""
Configuration management for the Pacman Package Manager
Enhanced with modern Python features and robust error handling.
"""

from __future__ import annotations

import platform
import re
from pathlib import Path
from typing import Any
from dataclasses import dataclass, field
from contextlib import contextmanager
from collections.abc import Generator

from loguru import logger

from .exceptions import ConfigError, create_error_context


@dataclass(frozen=True, slots=True)
class ConfigSection:
    """Represents a configuration section with enhanced validation."""

    name: str
    options: dict[str, str] = field(default_factory=dict)
    enabled: bool = True

    def get_option(self, key: str, default: str | None = None) -> str | None:
        """Get an option value with default support."""
        return self.options.get(key, default)

    def has_option(self, key: str) -> bool:
        """Check if option exists."""
        return key in self.options


@dataclass(slots=True)
class PacmanConfigState:
    """Mutable state for configuration management."""

    options: ConfigSection = field(default_factory=lambda: ConfigSection("options"))
    repositories: dict[str, ConfigSection] = field(default_factory=dict)
    _dirty: bool = field(default=False, init=False)

    def mark_dirty(self) -> None:
        """Mark configuration as modified."""
        self._dirty = True

    def is_dirty(self) -> bool:
        """Check if configuration has been modified."""
        return self._dirty

    def mark_clean(self) -> None:
        """Mark configuration as clean (saved)."""
        self._dirty = False


class PacmanConfig:
    """Enhanced class to manage pacman configuration settings with modern Python features."""

    def __init__(self, config_path: Path | str | None = None) -> None:
        """
        Initialize the pacman configuration manager with enhanced path detection.

        Args:
            config_path: Path to the pacman.conf file. If None, uses the default path.

        Raises:
            ConfigError: If configuration file is not found or cannot be read.
        """
        self._setup_system_info()
        self.config_path = self._resolve_config_path(config_path)
        self._state = PacmanConfigState()
        self._validate_config_file()

    def _setup_system_info(self) -> None:
        """Setup system-specific information using modern Python patterns."""
        system = platform.system().lower()

        match system:
            case "windows":
                self.is_windows = True
                self._default_paths = [
                    Path(r"C:\msys64\etc\pacman.conf"),
                    Path(r"C:\msys32\etc\pacman.conf"),
                    Path(r"D:\msys64\etc\pacman.conf"),
                ]
            case "linux" | "darwin":
                self.is_windows = False
                self._default_paths = [Path("/etc/pacman.conf")]
            case _:
                self.is_windows = False
                self._default_paths = [Path("/etc/pacman.conf")]
                logger.warning(f"Unknown system '{system}', using Linux defaults")

    def _resolve_config_path(self, config_path: Path | str | None) -> Path:
        """Resolve configuration path with enhanced error handling."""
        if config_path:
            resolved_path = Path(config_path)
            if resolved_path.exists():
                return resolved_path
            raise ConfigError(
                f"Specified config path does not exist: {resolved_path}",
                config_path=resolved_path,
            )

        # Try default paths
        for path in self._default_paths:
            if path.exists():
                logger.debug(f"Found pacman config at: {path}")
                return path

        # If no config found, provide helpful error
        searched_paths = [str(p) for p in self._default_paths]
        context = create_error_context(searched_paths=searched_paths)

        if self.is_windows:
            raise ConfigError(
                "MSYS2 pacman configuration not found. Please ensure MSYS2 is properly installed.",
                context=context,
                searched_paths=searched_paths,
            )
        else:
            raise ConfigError(
                "Pacman configuration file not found. Please ensure pacman is installed.",
                context=context,
                searched_paths=searched_paths,
            )

    def _validate_config_file(self) -> None:
        """Validate that the config file is readable with proper error handling."""
        try:
            with self.config_path.open("r", encoding="utf-8") as f:
                # Try to read first line to verify readability
                f.readline()
        except (OSError, PermissionError, UnicodeDecodeError) as e:
            context = create_error_context(config_path=self.config_path)
            raise ConfigError(
                f"Cannot read pacman configuration file: {e}",
                config_path=self.config_path,
                context=context,
                original_error=e,
            ) from e

    @contextmanager
    def _file_operation(self, mode: str = "r") -> Generator[Any, None, None]:
        """Context manager for safe file operations with enhanced error handling."""
        try:
            with self.config_path.open(mode, encoding="utf-8") as f:
                yield f
        except (OSError, PermissionError, UnicodeDecodeError) as e:
            operation = "reading" if "r" in mode else "writing"
            context = create_error_context(
                operation=operation, config_path=self.config_path, file_mode=mode
            )
            raise ConfigError(
                f"Failed {operation} config file: {e}",
                config_path=self.config_path,
                context=context,
                original_error=e,
            ) from e

    def _parse_config(self) -> PacmanConfigState:
        """Parse the pacman.conf file with enhanced error handling and caching."""
        if not self._state.is_dirty() and (
            self._state.options.options or self._state.repositories
        ):
            logger.debug("Using cached configuration data")
            return self._state

        logger.debug(f"Parsing configuration from {self.config_path}")

        try:
            new_state = PacmanConfigState()
            current_section = "options"

            with self._file_operation("r") as f:
                for line_num, line in enumerate(f, 1):
                    line = line.strip()

                    # Skip comments and empty lines
                    if not line or line.startswith("#"):
                        continue

                    # Process section headers with validation
                    if line.startswith("[") and line.endswith("]"):
                        current_section = line[1:-1]
                        if not current_section:
                            logger.warning(f"Empty section name at line {line_num}")
                            continue

                        if current_section == "options":
                            # Options section is already initialized
                            pass
                        else:
                            # Repository section
                            new_state.repositories[current_section] = ConfigSection(
                                name=current_section, enabled=True
                            )
                        continue

                    # Process key-value pairs with enhanced parsing
                    if "=" in line:
                        key, value = line.split("=", 1)
                        key = key.strip()
                        value = value.strip()

                        # Remove inline comments
                        if "#" in value:
                            value = value.split("#", 1)[0].strip()

                        # Validate key name
                        if not key:
                            logger.warning(f"Empty key at line {line_num}")
                            continue

                        # Store in appropriate section
                        if current_section == "options":
                            new_state.options.options[key] = value
                        elif current_section in new_state.repositories:
                            new_state.repositories[current_section].options[key] = value
                        else:
                            logger.warning(
                                f"Orphaned option '{key}' at line {line_num}"
                            )

            self._state = new_state
            self._state.mark_clean()

            logger.info(
                f"Loaded configuration with {len(new_state.options.options)} options "
                f"and {len(new_state.repositories)} repositories"
            )

            return self._state

        except Exception as e:
            context = create_error_context(config_path=self.config_path)
            if isinstance(e, ConfigError):
                raise
            raise ConfigError(
                f"Failed to parse configuration file: {e}",
                config_path=self.config_path,
                context=context,
                original_error=e,
            ) from e

    def get_option(self, option: str, default: str | None = None) -> str | None:
        """
        Get the value of a specific option from pacman.conf with enhanced error handling.

        Args:
            option: The option name to retrieve
            default: Default value if option is not found

        Returns:
            The option value or default if not found
        """
        try:
            config = self._parse_config()
            value = config.options.get_option(option, default)

            if value is not None:
                logger.debug(f"Retrieved option '{option}': {value}")
            else:
                logger.debug(f"Option '{option}' not found, using default: {default}")

            return value

        except Exception as e:
            logger.error(f"Failed to get option '{option}': {e}")
            return default

    def set_option(self, option: str, value: str, create_backup: bool = True) -> bool:
        """
        Set or modify an option in pacman.conf with enhanced safety and validation.

        Args:
            option: The option name to set
            value: The value to set
            create_backup: Whether to create a backup before modification

        Returns:
            True if successful, False otherwise
        """
        if not option or not isinstance(option, str):
            raise ConfigError(
                "Option name must be a non-empty string", invalid_option=option
            )

        if not isinstance(value, str):
            raise ConfigError(
                f"Option value must be a string, got {type(value).__name__}",
                invalid_option=option,
                invalid_value=value,
            )

        try:
            # Create backup if requested
            if create_backup:
                self._create_backup()

            # Read current content
            with self._file_operation("r") as f:
                lines = f.readlines()

            # Pattern to match the option (with or without comment)
            option_pattern = re.compile(
                rf"^#?\s*{re.escape(option)}\s*=.*$", re.MULTILINE
            )
            option_found = False
            new_line = f"{option} = {value}\n"

            # Search and replace existing option
            for i, line in enumerate(lines):
                if option_pattern.match(line):
                    lines[i] = new_line
                    option_found = True
                    logger.debug(f"Updated existing option '{option}' at line {i + 1}")
                    break

            # If option not found, add it to the [options] section
            if not option_found:
                option_added = False
                for i, line in enumerate(lines):
                    if line.strip() == "[options]":
                        # Insert after [options] line
                        lines.insert(i + 1, new_line)
                        option_added = True
                        logger.debug(
                            f"Added new option '{option}' after [options] section"
                        )
                        break

                if not option_added:
                    # If no [options] section found, add it
                    lines.extend(["\n[options]\n", new_line])
                    logger.debug(f"Created [options] section and added '{option}'")

            # Write back to file
            with self._file_operation("w") as f:
                f.writelines(lines)

            # Mark state as dirty so it gets re-parsed
            self._state.mark_dirty()

            logger.success(f"Successfully set option '{option}' = '{value}'")
            return True

        except ConfigError:
            raise
        except Exception as e:
            context = create_error_context(
                option=option, value=value, config_path=self.config_path
            )
            raise ConfigError(
                f"Failed to set option '{option}': {e}",
                config_path=self.config_path,
                invalid_option=option,
                context=context,
                original_error=e,
            ) from e

    def get_enabled_repos(self) -> list[str]:
        """
        Get a list of enabled repositories with enhanced error handling.

        Returns:
            List of enabled repository names
        """
        try:
            config = self._parse_config()
            enabled_repos = [
                name for name, repo in config.repositories.items() if repo.enabled
            ]

            logger.debug(f"Found {len(enabled_repos)} enabled repositories")
            return enabled_repos

        except Exception as e:
            logger.error(f"Failed to get enabled repositories: {e}")
            return []

    def enable_repo(self, repo: str, create_backup: bool = True) -> bool:
        """
        Enable a repository in pacman.conf with enhanced safety.

        Args:
            repo: The repository name to enable
            create_backup: Whether to create a backup before modification

        Returns:
            True if successful, False otherwise
        """
        if not repo or not isinstance(repo, str):
            raise ConfigError(
                "Repository name must be a non-empty string", config_section=repo
            )

        try:
            if create_backup:
                self._create_backup()

            # Read current content
            with self._file_operation("r") as f:
                content = f.read()

            # Look for commented repository section
            section_pattern = re.compile(rf"^#\s*\[{re.escape(repo)}\]", re.MULTILINE)

            if section_pattern.search(content):
                # Uncomment the section
                content = section_pattern.sub(f"[{repo}]", content)

                # Write back to file
                with self._file_operation("w") as f:
                    f.write(content)

                # Mark state as dirty
                self._state.mark_dirty()

                logger.success(f"Successfully enabled repository: {repo}")
                return True
            else:
                logger.warning(f"Repository '{repo}' not found in configuration")
                return False

        except ConfigError:
            raise
        except Exception as e:
            context = create_error_context(
                repository=repo, config_path=self.config_path
            )
            raise ConfigError(
                f"Failed to enable repository '{repo}': {e}",
                config_path=self.config_path,
                config_section=repo,
                context=context,
                original_error=e,
            ) from e

    def _create_backup(self) -> Path:
        """Create a backup of the configuration file with timestamp."""
        from datetime import datetime

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = self.config_path.with_suffix(f".{timestamp}.backup")

        try:
            import shutil

            shutil.copy2(self.config_path, backup_path)
            logger.info(f"Created configuration backup: {backup_path}")
            return backup_path

        except Exception as e:
            logger.warning(f"Failed to create backup: {e}")
            raise ConfigError(
                f"Failed to create configuration backup: {e}",
                config_path=self.config_path,
                original_error=e,
            ) from e

    @property
    def repository_count(self) -> int:
        """Get the number of configured repositories."""
        config = self._parse_config()
        return len(config.repositories)

    @property
    def enabled_repository_count(self) -> int:
        """Get the number of enabled repositories."""
        return len(self.get_enabled_repos())

    def get_config_summary(self) -> dict[str, Any]:
        """Get a summary of the current configuration."""
        try:
            config = self._parse_config()
            return {
                "config_path": str(self.config_path),
                "total_options": len(config.options.options),
                "total_repositories": len(config.repositories),
                "enabled_repositories": len(self.get_enabled_repos()),
                "is_windows": self.is_windows,
                "is_dirty": config.is_dirty(),
            }
        except Exception as e:
            logger.error(f"Failed to generate config summary: {e}")
            return {"error": str(e)}

    def validate_configuration(self) -> list[str]:
        """Validate the configuration and return any issues found."""
        issues: list[str] = []

        try:
            config = self._parse_config()

            # Check for common required options
            required_options = ["Architecture", "SigLevel"]
            for option in required_options:
                if not config.options.has_option(option):
                    issues.append(f"Missing required option: {option}")

            # Check for enabled repositories
            if not self.get_enabled_repos():
                issues.append("No enabled repositories found")

            # Check for valid architecture
            arch = config.options.get_option("Architecture")
            if arch and arch not in [
                "auto",
                "i686",
                "x86_64",
                "armv6h",
                "armv7h",
                "aarch64",
            ]:
                issues.append(f"Unknown architecture: {arch}")

        except Exception as e:
            issues.append(f"Configuration parsing error: {e}")

        return issues
