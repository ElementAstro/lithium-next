#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Exception hierarchy for build system helpers.
"""


class BuildSystemError(Exception):
    """Base exception class for build system errors."""

    pass


class ConfigurationError(BuildSystemError):
    """Exception raised for errors in the configuration process."""

    pass


class BuildError(BuildSystemError):
    """Exception raised for errors in the build process."""

    pass


class TestError(BuildSystemError):
    """Exception raised for errors in the testing process."""

    pass


class InstallationError(BuildSystemError):
    """Exception raised for errors in the installation process."""

    pass
