#!/usr/bin/env python3
"""
Logging configuration for Nginx Manager.
"""

import sys
from loguru import logger


def setup_logging(log_level: str = "INFO") -> None:
    """
    Set up logging configuration using loguru.

    Args:
        log_level: The logging level (DEBUG, INFO, WARNING, ERROR, CRITICAL)
    """
    # Remove default logger
    logger.remove()

    # Add stderr logger with appropriate format and level
    logger.add(
        sys.stderr,
        format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
        level=log_level,
        colorize=True
    )

    # Optional: Add a file logger for persistent logs
    logger.add(
        "nginx_manager.log",
        rotation="10 MB",
        retention="1 week",
        level=log_level,
        format="{time:YYYY-MM-DD HH:mm:ss} | {level: <8} | {name}:{function}:{line} - {message}"
    )

    logger.info("Logging initialized")