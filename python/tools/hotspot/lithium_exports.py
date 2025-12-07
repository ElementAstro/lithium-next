"""
Lithium export declarations for hotspot module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from lithium_bridge import expose_command, expose_controller

from .hotspot_manager import HotspotManager
from .models import BandType


@expose_controller(
    endpoint="/api/hotspot/status",
    method="GET",
    description="Get WiFi hotspot status",
    tags=["hotspot", "wifi"],
    version="1.0.0",
)
def get_hotspot_status() -> dict:
    """
    Get the current status of the WiFi hotspot.

    Returns:
        Dictionary with hotspot status information
    """
    try:
        manager = HotspotManager()
        status = manager.get_status()
        return {
            "active": status.get("active", False),
            "ssid": status.get("ssid"),
            "interface": status.get("interface"),
            "success": True,
        }
    except Exception as e:
        return {
            "active": False,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/hotspot/start",
    method="POST",
    description="Start WiFi hotspot",
    tags=["hotspot", "wifi"],
    version="1.0.0",
)
def start_hotspot(
    ssid: str,
    password: str,
    interface: str = None,
    band: str = "2.4GHz",
    channel: int = None,
) -> dict:
    """
    Start a WiFi hotspot.

    Args:
        ssid: Network name
        password: Network password
        interface: Network interface to use
        band: Frequency band (2.4GHz or 5GHz)
        channel: WiFi channel

    Returns:
        Dictionary with operation result
    """
    try:
        manager = HotspotManager()
        band_type = BandType.BAND_2_4GHZ if "2.4" in band else BandType.BAND_5GHZ
        result = manager.start(
            ssid=ssid,
            password=password,
            interface=interface,
            band=band_type,
            channel=channel,
        )
        return {
            "action": "start",
            "ssid": ssid,
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "action": "start",
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/hotspot/stop",
    method="POST",
    description="Stop WiFi hotspot",
    tags=["hotspot", "wifi"],
    version="1.0.0",
)
def stop_hotspot() -> dict:
    """
    Stop the WiFi hotspot.

    Returns:
        Dictionary with operation result
    """
    try:
        manager = HotspotManager()
        result = manager.stop()
        return {
            "action": "stop",
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "action": "stop",
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/hotspot/clients",
    method="GET",
    description="Get connected clients",
    tags=["hotspot", "wifi"],
    version="1.0.0",
)
def get_connected_clients() -> dict:
    """
    Get list of connected clients.

    Returns:
        Dictionary with connected clients
    """
    try:
        manager = HotspotManager()
        clients = manager.get_connected_clients()
        return {
            "clients": [
                {
                    "mac": c.mac_address,
                    "ip": c.ip_address,
                    "hostname": c.hostname,
                }
                for c in clients
            ],
            "count": len(clients),
            "success": True,
        }
    except Exception as e:
        return {
            "clients": [],
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/hotspot/interfaces",
    method="GET",
    description="Get available network interfaces",
    tags=["hotspot", "wifi"],
    version="1.0.0",
)
def get_interfaces() -> dict:
    """
    Get available network interfaces for hotspot.

    Returns:
        Dictionary with interface list
    """
    try:
        manager = HotspotManager()
        interfaces = manager.get_network_interfaces()
        return {
            "interfaces": interfaces,
            "count": len(interfaces),
            "success": True,
        }
    except Exception as e:
        return {
            "interfaces": [],
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/hotspot/channels",
    method="GET",
    description="Get available WiFi channels",
    tags=["hotspot", "wifi"],
    version="1.0.0",
)
def get_channels(interface: str = None, band: str = "2.4GHz") -> dict:
    """
    Get available WiFi channels.

    Args:
        interface: Network interface
        band: Frequency band

    Returns:
        Dictionary with channel list
    """
    try:
        manager = HotspotManager()
        band_type = BandType.BAND_2_4GHZ if "2.4" in band else BandType.BAND_5GHZ
        channels = manager.get_available_channels(interface=interface, band=band_type)
        return {
            "channels": channels,
            "band": band,
            "success": True,
        }
    except Exception as e:
        return {
            "channels": [],
            "success": False,
            "error": str(e),
        }


@expose_command(
    command_id="hotspot.status",
    description="Get hotspot status (command)",
    priority=5,
    timeout_ms=10000,
    tags=["hotspot"],
)
def cmd_hotspot_status() -> dict:
    """Get hotspot status via command dispatcher."""
    return get_hotspot_status()


@expose_command(
    command_id="hotspot.start",
    description="Start hotspot (command)",
    priority=5,
    timeout_ms=30000,
    tags=["hotspot"],
)
def cmd_hotspot_start(ssid: str, password: str) -> dict:
    """Start hotspot via command dispatcher."""
    return start_hotspot(ssid, password)


@expose_command(
    command_id="hotspot.stop",
    description="Stop hotspot (command)",
    priority=5,
    timeout_ms=15000,
    tags=["hotspot"],
)
def cmd_hotspot_stop() -> dict:
    """Stop hotspot via command dispatcher."""
    return stop_hotspot()
