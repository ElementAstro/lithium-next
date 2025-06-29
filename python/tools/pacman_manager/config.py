#!/usr/bin/env python3
"""
Configuration management for the Pacman Package Manager
"""

import platform
import re
from pathlib import Path
from typing import Dict, Any, Optional, List

from loguru import logger

from .exceptions import ConfigError


class PacmanConfig:
    """Class to manage pacman configuration settings"""

    def __init__(self, config_path: Optional[Path] = None):
        """
        Initialize the pacman configuration manager.

        Args:
            config_path: Path to the pacman.conf file. If None, uses the default path.
        """
        self.is_windows = platform.system().lower() == "windows"

        if config_path:
            self.config_path = config_path
        elif self.is_windows:
            # Default MSYS2 pacman config path
            self.config_path = Path(r"C:\msys64\etc\pacman.conf")
            if not self.config_path.exists():
                self.config_path = Path(r"C:\msys32\etc\pacman.conf")
        else:
            # Default Linux pacman config path
            self.config_path = Path("/etc/pacman.conf")

        if not self.config_path.exists():
            raise ConfigError(
                f"Pacman configuration file not found at {self.config_path}"
            )

        # Cache for config settings to avoid repeated parsing
        self._cache: Dict[str, Any] = {}

    def _parse_config(self) -> Dict[str, Any]:
        """Parse the pacman.conf file and return a dictionary of settings"""
        if self._cache:
            return self._cache

        config: Dict[str, Any] = {"repos": {}, "options": {}}
        current_section = "options"

        with open(self.config_path, "r") as f:
            for line in f:
                line = line.strip()

                # Skip comments and empty lines
                if not line or line.startswith("#"):
                    continue

                # Check for section headers
                if line.startswith("[") and line.endswith("]"):
                    current_section = line[1:-1]
                    if current_section != "options":
                        config["repos"][current_section] = {"enabled": True}
                    continue

                # Parse key-value pairs
                if "=" in line:
                    key, value = line.split("=", 1)
                    key = key.strip()
                    value = value.strip()

                    # Remove inline comments
                    if "#" in value:
                        value = value.split("#", 1)[0].strip()

                    if current_section == "options":
                        config["options"][key] = value
                    else:
                        config["repos"][current_section][key] = value

        self._cache = config
        return config

    def get_option(self, option: str) -> Optional[str]:
        """
        Get the value of a specific option from pacman.conf.

        Args:
            option: The option name to retrieve

        Returns:
            The option value or None if not found
        """
        config = self._parse_config()
        return config.get("options", {}).get(option)

    def set_option(self, option: str, value: str) -> bool:
        """
        Set or modify an option in pacman.conf.

        Args:
            option: The option name to set
            value: The value to set

        Returns:
            True if successful, False otherwise
        """
        # Read the current config
        with open(self.config_path, "r") as f:
            lines = f.readlines()

        option_pattern = re.compile(rf"^#?\s*{re.escape(option)}\s*=.*")
        option_found = False

        for i, line in enumerate(lines):
            if option_pattern.match(line):
                lines[i] = f"{option} = {value}\n"
                option_found = True
                break

        if not option_found:
            # Add to the [options] section
            options_index = -1
            for i, line in enumerate(lines):
                if line.strip() == "[options]":
                    options_index = i
                    break

            if options_index >= 0:
                lines.insert(options_index + 1, f"{option} = {value}\n")
            else:
                lines.append(f"\n[options]\n{option} = {value}\n")

        # Write back to file (requires sudo typically)
        try:
            with open(self.config_path, "w") as f:
                f.writelines(lines)
            self._cache = {}  # Clear cache
            return True
        except (PermissionError, OSError):
            logger.error(
                f"Failed to write to {self.config_path}. Do you have sufficient permissions?"
            )
            return False

    def get_enabled_repos(self) -> List[str]:
        """
        Get a list of enabled repositories.

        Returns:
            List of enabled repository names
        """
        config = self._parse_config()
        return [
            repo
            for repo, details in config.get("repos", {}).items()
            if details.get("enabled", False)
        ]

    def enable_repo(self, repo: str) -> bool:
        """
        Enable a repository in pacman.conf.

        Args:
            repo: The repository name to enable

        Returns:
            True if successful, False otherwise
        """
        # Read the current config
        with open(self.config_path, "r") as f:
            content = f.read()

        # Look for the repository section commented out
        section_pattern = re.compile(rf"#\s*$${re.escape(repo)}$$")
        if section_pattern.search(content):
            # Uncomment the section
            content = section_pattern.sub(f"[{repo}]", content)

            # Write back to file
            try:
                with open(self.config_path, "w") as f:
                    f.write(content)
                self._cache = {}  # Clear cache
                return True
            except (PermissionError, OSError):
                logger.error(
                    f"Failed to write to {self.config_path}. Do you have sufficient permissions?"
                )
                return False
        else:
            logger.warning(f"Repository {repo} not found in config")
            return False
