#!/usr/bin/env python3
"""
Robust command execution utilities for the WiFi Hotspot Manager.
"""

import asyncio
from typing import List

from loguru import logger

from .models import CommandResult


async def run_command_async(cmd: List[str]) -> CommandResult:
    """
    Run a command asynchronously with improved error handling and logging.

    Args:
        cmd: A list of command parts to execute.

    Returns:
        A CommandResult object with the execution outcome.
    """
    cmd_str = " ".join(cmd)
    logger.debug(f"Executing command: {cmd_str}")

    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()

        success = proc.returncode == 0
        result = CommandResult(
            success=success,
            stdout=stdout.decode().strip(),
            stderr=stderr.decode().strip(),
            return_code=proc.returncode or -1,
            command=cmd,
        )

        if not success:
            logger.error(f"Command failed with code {result.return_code}: {cmd_str}")
            logger.error(f"Stderr: {result.stderr}")

        return result

    except FileNotFoundError:
        logger.error(f"Command not found: {cmd[0]}. Please ensure it is installed.")
        return CommandResult(success=False, stderr=f"Command not found: {cmd[0]}", command=cmd)
    except Exception as e:
        logger.exception(f"An unexpected error occurred while running '{cmd_str}'.")
        return CommandResult(success=False, stderr=str(e), command=cmd)
