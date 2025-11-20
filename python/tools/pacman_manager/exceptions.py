#!/usr/bin/env python3
"""
Exception types for the Pacman Package Manager
"""


class PacmanError(Exception):
    """Base exception for all pacman-related errors"""
    pass


class CommandError(PacmanError):
    """Exception raised when a command execution fails"""

    def __init__(self, message: str, return_code: int, stderr: str):
        self.return_code = return_code
        self.stderr = stderr
        super().__init__(f"{message} (Return code: {return_code}): {stderr}")


class PackageNotFoundError(PacmanError):
    """Exception raised when a package is not found"""
    pass


class ConfigError(PacmanError):
    """Exception raised when there's a configuration error"""
    pass
