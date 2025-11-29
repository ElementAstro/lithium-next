#!/usr/bin/env python3
"""
PyBind11 adapter for Hotspot Manager.

This module provides simplified interfaces for C++ bindings via pybind11.
It handles platform compatibility and graceful degradation for non-Linux systems.
"""

import sys
import asyncio
from typing import Dict, List, Any

from loguru import logger
from .hotspot_manager import HotspotManager


def _is_platform_supported() -> bool:
    """
    Check if the platform is supported (Linux only).

    Returns:
        True if running on Linux, False otherwise
    """
    return sys.platform.startswith("linux")


class HotspotPyBindAdapter:
    """
    Adapter class to expose Hotspot functionality to C++ via pybind11.

    This class simplifies the interface and handles conversions between Python and C++.
    All methods return dict/list types for pybind11 compatibility and handle exceptions
    internally, returning structured error responses.
    """

    @staticmethod
    def _check_platform() -> Dict[str, Any]:
        """
        Check platform support and return structured response.

        Returns:
            Dict with success flag and platform check result, or error dict if unsupported
        """
        if not _is_platform_supported():
            return {
                "success": False,
                "supported": False,
                "error": "Hotspot functionality is only supported on Linux systems",
            }
        return {"success": True, "supported": True}

    @staticmethod
    def start(
        name: str = "MyHotspot",
        password: str = None,
        interface: str = "wlan0",
        channel: int = 11,
        **kwargs,
    ) -> Dict[str, Any]:
        """
        Start a WiFi hotspot with the specified configuration.

        Args:
            name: SSID name for the hotspot
            password: Password for the hotspot (optional for open networks)
            interface: Network interface to use
            channel: WiFi channel to broadcast on
            **kwargs: Additional configuration parameters

        Returns:
            Dict containing success status, error message (if any), and running state
        """
        platform_check = HotspotPyBindAdapter._check_platform()
        if not platform_check["success"]:
            return platform_check

        logger.info(f"C++ binding: Starting hotspot '{name}' on {interface}")
        try:
            manager = HotspotManager()
            success = manager.start(
                name=name,
                password=password,
                interface=interface,
                channel=channel,
                **kwargs,
            )
            return {
                "success": success,
                "supported": True,
                "message": (
                    f"Hotspot '{name}' started successfully"
                    if success
                    else "Failed to start hotspot"
                ),
                "running": success,
            }
        except Exception as e:
            logger.exception(f"Error in start: {e}")
            return {
                "success": False,
                "supported": True,
                "error": str(e),
                "running": False,
            }

    @staticmethod
    def stop() -> Dict[str, Any]:
        """
        Stop the currently running hotspot.

        Returns:
            Dict containing success status and error message (if any)
        """
        platform_check = HotspotPyBindAdapter._check_platform()
        if not platform_check["success"]:
            return platform_check

        logger.info("C++ binding: Stopping hotspot")
        try:
            manager = HotspotManager()
            success = manager.stop()
            return {
                "success": success,
                "supported": True,
                "message": (
                    "Hotspot stopped successfully"
                    if success
                    else "Failed to stop hotspot"
                ),
                "running": False if success else True,
            }
        except Exception as e:
            logger.exception(f"Error in stop: {e}")
            return {"success": False, "supported": True, "error": str(e)}

    @staticmethod
    def get_status() -> Dict[str, Any]:
        """
        Get the current status of the hotspot.

        Returns:
            Dict containing hotspot status information including running state,
            interface, SSID, connected clients list, uptime, and IP address
        """
        platform_check = HotspotPyBindAdapter._check_platform()
        if not platform_check["success"]:
            return platform_check

        logger.info("C++ binding: Getting hotspot status")
        try:
            manager = HotspotManager()
            status = manager.get_status()
            return {"success": True, "supported": True, "data": status}
        except Exception as e:
            logger.exception(f"Error in get_status: {e}")
            return {
                "success": False,
                "supported": True,
                "error": str(e),
                "data": {
                    "running": False,
                    "interface": None,
                    "connection_name": None,
                    "ssid": None,
                    "clients": [],
                    "uptime": None,
                    "ip_address": None,
                },
            }

    @staticmethod
    def get_clients() -> Dict[str, Any]:
        """
        Get list of clients connected to the hotspot.

        Returns:
            Dict containing success status and list of connected clients
        """
        platform_check = HotspotPyBindAdapter._check_platform()
        if not platform_check["success"]:
            return platform_check

        logger.info("C++ binding: Getting connected clients")
        try:
            manager = HotspotManager()
            clients = manager.get_connected_clients()
            return {
                "success": True,
                "supported": True,
                "clients": clients,
                "count": len(clients),
            }
        except Exception as e:
            logger.exception(f"Error in get_clients: {e}")
            return {
                "success": False,
                "supported": True,
                "error": str(e),
                "clients": [],
                "count": 0,
            }

    @staticmethod
    async def start_async(
        name: str = "MyHotspot",
        password: str = None,
        interface: str = "wlan0",
        channel: int = 11,
        **kwargs,
    ) -> Dict[str, Any]:
        """
        Start a WiFi hotspot asynchronously with the specified configuration.

        Args:
            name: SSID name for the hotspot
            password: Password for the hotspot (optional for open networks)
            interface: Network interface to use
            channel: WiFi channel to broadcast on
            **kwargs: Additional configuration parameters

        Returns:
            Dict containing success status, error message (if any), and running state
        """
        platform_check = HotspotPyBindAdapter._check_platform()
        if not platform_check["success"]:
            return platform_check

        logger.info(f"C++ binding (async): Starting hotspot '{name}' on {interface}")
        try:
            manager = HotspotManager()
            # Run blocking operation in executor to avoid blocking event loop
            loop = asyncio.get_event_loop()
            success = await loop.run_in_executor(
                None,
                lambda: manager.start(
                    name=name,
                    password=password,
                    interface=interface,
                    channel=channel,
                    **kwargs,
                ),
            )
            return {
                "success": success,
                "supported": True,
                "message": (
                    f"Hotspot '{name}' started successfully"
                    if success
                    else "Failed to start hotspot"
                ),
                "running": success,
            }
        except Exception as e:
            logger.exception(f"Error in start_async: {e}")
            return {
                "success": False,
                "supported": True,
                "error": str(e),
                "running": False,
            }

    @staticmethod
    async def stop_async() -> Dict[str, Any]:
        """
        Stop the currently running hotspot asynchronously.

        Returns:
            Dict containing success status and error message (if any)
        """
        platform_check = HotspotPyBindAdapter._check_platform()
        if not platform_check["success"]:
            return platform_check

        logger.info("C++ binding (async): Stopping hotspot")
        try:
            manager = HotspotManager()
            # Run blocking operation in executor to avoid blocking event loop
            loop = asyncio.get_event_loop()
            success = await loop.run_in_executor(None, manager.stop)
            return {
                "success": success,
                "supported": True,
                "message": (
                    "Hotspot stopped successfully"
                    if success
                    else "Failed to stop hotspot"
                ),
                "running": False if success else True,
            }
        except Exception as e:
            logger.exception(f"Error in stop_async: {e}")
            return {"success": False, "supported": True, "error": str(e)}

    @staticmethod
    async def get_status_async() -> Dict[str, Any]:
        """
        Get the current status of the hotspot asynchronously.

        Returns:
            Dict containing hotspot status information including running state,
            interface, SSID, connected clients list, uptime, and IP address
        """
        platform_check = HotspotPyBindAdapter._check_platform()
        if not platform_check["success"]:
            return platform_check

        logger.info("C++ binding (async): Getting hotspot status")
        try:
            manager = HotspotManager()
            # Run blocking operation in executor to avoid blocking event loop
            loop = asyncio.get_event_loop()
            status = await loop.run_in_executor(None, manager.get_status)
            return {"success": True, "supported": True, "data": status}
        except Exception as e:
            logger.exception(f"Error in get_status_async: {e}")
            return {
                "success": False,
                "supported": True,
                "error": str(e),
                "data": {
                    "running": False,
                    "interface": None,
                    "connection_name": None,
                    "ssid": None,
                    "clients": [],
                    "uptime": None,
                    "ip_address": None,
                },
            }

    @staticmethod
    async def get_clients_async() -> Dict[str, Any]:
        """
        Get list of clients connected to the hotspot asynchronously.

        Returns:
            Dict containing success status and list of connected clients
        """
        platform_check = HotspotPyBindAdapter._check_platform()
        if not platform_check["success"]:
            return platform_check

        logger.info("C++ binding (async): Getting connected clients")
        try:
            manager = HotspotManager()
            # Run blocking operation in executor to avoid blocking event loop
            loop = asyncio.get_event_loop()
            clients = await loop.run_in_executor(None, manager.get_connected_clients)
            return {
                "success": True,
                "supported": True,
                "clients": clients,
                "count": len(clients),
            }
        except Exception as e:
            logger.exception(f"Error in get_clients_async: {e}")
            return {
                "success": False,
                "supported": True,
                "error": str(e),
                "clients": [],
                "count": 0,
            }

    @staticmethod
    def get_tool_info() -> Dict[str, Any]:
        """
        Get metadata about the hotspot tool.

        Returns:
            Dict containing tool information and configuration
        """
        from . import get_tool_info as module_get_tool_info

        try:
            return {"success": True, "data": module_get_tool_info()}
        except Exception as e:
            logger.exception(f"Error in get_tool_info: {e}")
            return {"success": False, "error": str(e), "data": {}}
