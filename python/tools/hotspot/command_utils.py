#!/usr/bin/env python3
"""
Command execution utilities for WiFi Hotspot Manager.
Provides functions for running shell commands synchronously and asynchronously.
"""

import asyncio
import subprocess
from typing import List
from loguru import logger

from .models import CommandResult


def run_command(cmd: List[str]) -> CommandResult:
    """
    Run a command synchronously and return the result.

    Args:
        cmd: List of command parts to execute

    Returns:
        CommandResult object containing the command output and status
    """
    logger.debug(f"Running command: {' '.join(cmd)}")
    try:
        result = subprocess.run(
            cmd,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        success = result.returncode == 0

        if not success:
            logger.error(f"Command failed: {' '.join(cmd)}")
            logger.error(f"Error: {result.stderr}")

        return CommandResult(
            success=success,
            stdout=result.stdout,
            stderr=result.stderr,
            return_code=result.returncode,
            command=cmd
        )
    except Exception as e:
        logger.exception(f"Exception running command: {e}")
        return CommandResult(
            success=False,
            stderr=str(e),
            command=cmd
        )


async def run_command_async(cmd: List[str]) -> CommandResult:
    """
    Run a command asynchronously and return the result.

    Args:
        cmd: List of command parts to execute

    Returns:
        CommandResult object containing the command output and status
    """
    logger.debug(f"Running command asynchronously: {' '.join(cmd)}")
    try:
        process = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            text=False
        )
        stdout_bytes, stderr_bytes = await process.communicate()

        # Decode bytes to strings
        stdout = stdout_bytes.decode('utf-8') if stdout_bytes else ""
        stderr = stderr_bytes.decode('utf-8') if stderr_bytes else ""

        success = process.returncode == 0

        if not success:
            logger.error(f"Command failed: {' '.join(cmd)}")
            logger.error(f"Error: {stderr}")

        return CommandResult(
            success=success,
            stdout=stdout,
            stderr=stderr,
            return_code=process.returncode if process.returncode is not None else -1,
            command=cmd
        )
    except Exception as e:
        logger.exception(f"Exception running command: {e}")
        return CommandResult(
            success=False,
            stderr=str(e),
            command=cmd
        )
