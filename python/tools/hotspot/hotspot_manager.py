#!/usr/bin/env python3
"""
Enhanced Hotspot Manager with modern Python features and robust error handling.

This module provides a comprehensive, async-first hotspot management system with
extensive error handling, monitoring capabilities, and extensible plugin architecture.
"""

from __future__ import annotations

import asyncio
import json
import re
import shutil
import time
from contextlib import asynccontextmanager
from pathlib import Path
from typing import (
    Any,
    AsyncContextManager,
    AsyncGenerator,
    AsyncIterator,
    Awaitable,
    Callable,
    Dict,
    List,
    Optional,
    Protocol,
    Union,
)

from loguru import logger

from .command_utils import run_command_async
from .models import (
    AuthenticationType,
    ConnectedClient,
    HotspotConfig,
    HotspotException,
    NetworkManagerError,
    InterfaceError,
    ConfigurationError,
    NetworkInterface,
    CommandResult,
)


class HotspotPlugin(Protocol):
    """Protocol for hotspot plugins."""

    async def on_hotspot_start(self, config: HotspotConfig) -> None:
        """Called when hotspot starts."""
        ...

    async def on_hotspot_stop(self) -> None:
        """Called when hotspot stops."""
        ...

    async def on_client_connect(self, client: ConnectedClient) -> None:
        """Called when a client connects."""
        ...

    async def on_client_disconnect(self, client: ConnectedClient) -> None:
        """Called when a client disconnects."""
        ...


class HotspotManager:
    """
    Enhanced WiFi hotspot manager with modern async architecture and robust error handling.

    Features:
    - Async-first design for better performance
    - Comprehensive error handling with custom exceptions
    - Plugin architecture for extensibility
    - Monitoring and metrics collection
    - Configuration validation and management
    - Resource cleanup with context managers
    """

    def __init__(
        self,
        config: Optional[HotspotConfig] = None,
        config_dir: Optional[Path] = None,
        runner: Optional[Callable[..., Awaitable[CommandResult]]] = None,
    ) -> None:
        """
        Initialize the hotspot manager.

        Args:
            config: Initial hotspot configuration
            config_dir: Directory for storing configuration files
            runner: Custom command runner (for testing/mocking)
        """
        self.config_dir = config_dir or Path.home() / ".config" / "hotspot-manager"
        self.config_file = self.config_dir / "config.json"
        self.run_command = runner or run_command_async
        self.plugins: Dict[str, HotspotPlugin] = {}
        self._monitoring_task: Optional[asyncio.Task[None]] = None
        self._client_cache: Dict[str, ConnectedClient] = {}

        # Check NetworkManager availability
        if not self._is_network_manager_available():
            logger.warning(
                "NetworkManager (nmcli) is not available. "
                "Some features may not work correctly."
            )

        # Load or use provided configuration
        self.current_config = config or self._load_config() or HotspotConfig()

        logger.debug(
            "HotspotManager initialized",
            extra={
                "config": self.current_config.to_dict(),
                "config_dir": str(self.config_dir),
                "nmcli_available": self._is_network_manager_available(),
            },
        )

    def _is_network_manager_available(self) -> bool:
        """Check if NetworkManager is available on the system."""
        return shutil.which("nmcli") is not None

    def _parse_detail(self, output: str, key: str) -> Optional[str]:
        """Parse a specific field from nmcli output."""
        pattern = rf"^{re.escape(key)}:\s*(.*)$"
        match = re.search(pattern, output, re.MULTILINE)
        return match.group(1).strip() if match else None

    async def _ensure_network_manager(self) -> None:
        """Ensure NetworkManager is available and responsive."""
        if not self._is_network_manager_available():
            raise NetworkManagerError(
                "NetworkManager (nmcli) is not available", error_code="NM_NOT_FOUND"
            )

        # Test NetworkManager responsiveness
        try:
            result = await asyncio.wait_for(
                self.run_command(["nmcli", "--version"]), timeout=5.0
            )
            if not result.success:
                raise NetworkManagerError(
                    "NetworkManager is not responding",
                    error_code="NM_NOT_RESPONDING",
                    command_result=result.to_dict(),
                )
        except asyncio.TimeoutError:
            raise NetworkManagerError(
                "NetworkManager command timed out", error_code="NM_TIMEOUT"
            ) from None

    async def get_available_interfaces(self) -> List[NetworkInterface]:
        """Get list of available network interfaces."""
        await self._ensure_network_manager()

        result = await self.run_command(["nmcli", "device", "status"])
        if not result.success:
            raise NetworkManagerError(
                "Failed to get interface list",
                error_code="NM_INTERFACE_LIST_FAILED",
                command_result=result.to_dict(),
            )

        interfaces = []
        for line in result.stdout.splitlines()[1:]:  # Skip header
            parts = line.split()
            if len(parts) >= 3:
                interfaces.append(
                    NetworkInterface(
                        name=parts[0],
                        type=parts[1],
                        state=parts[2],
                        driver=parts[3] if len(parts) > 3 else None,
                    )
                )

        return [iface for iface in interfaces if iface.is_wifi]

    async def validate_interface(self, interface: str) -> bool:
        """Validate that an interface exists and can be used for hotspots."""
        interfaces = await self.get_available_interfaces()
        target_interface = next(
            (iface for iface in interfaces if iface.name == interface), None
        )

        if not target_interface:
            raise InterfaceError(
                f"Interface '{interface}' not found",
                error_code="INTERFACE_NOT_FOUND",
                interface=interface,
            )

        if not target_interface.is_wifi:
            raise InterfaceError(
                f"Interface '{interface}' is not a WiFi interface",
                error_code="INTERFACE_NOT_WIFI",
                interface=interface,
                interface_type=target_interface.type,
            )

        return True

    async def get_connected_clients(
        self, interface: Optional[str] = None
    ) -> List[ConnectedClient]:
        """
        Get list of connected clients with enhanced error handling.

        Args:
            interface: Network interface to check (auto-detect if None)

        Returns:
            List of connected clients
        """
        try:
            if not interface:
                status = await self.get_status()
                if not status.get("running"):
                    return []
                interface = status.get("interface")

            if not interface:
                logger.debug("No interface specified and hotspot not running")
                return []

            # Get station information using iw
            iw_result = await self.run_command(
                ["iw", "dev", interface, "station", "dump"]
            )

            if not iw_result.success:
                logger.debug(
                    f"Failed to get station dump for {interface}: {iw_result.stderr}"
                )
                return []

            # Parse MAC addresses from station dump
            mac_addresses = re.findall(r"Station (\S+)", iw_result.stdout)
            clients = []

            # Get ARP table for IP addresses
            arp_result = await self.run_command(["arp", "-n"])
            mac_to_ip = {}

            if arp_result.success:
                for line in arp_result.stdout.splitlines():
                    match = re.search(r"(\S+)\s+ether\s+(\S+)", line)
                    if match:
                        ip, mac = match.groups()
                        mac_to_ip[mac.lower()] = ip

            # Create ConnectedClient objects
            current_time = time.time()
            for mac in mac_addresses:
                mac_lower = mac.lower()

                # Get cached client info or create new
                if mac_lower in self._client_cache:
                    client = self._client_cache[mac_lower]
                    # Update IP if available
                    if mac_lower in mac_to_ip:
                        client = ConnectedClient(
                            mac_address=client.mac_address,
                            ip_address=mac_to_ip[mac_lower],
                            hostname=client.hostname,
                            connected_since=client.connected_since,
                            data_transferred=client.data_transferred,
                            signal_strength=client.signal_strength,
                        )
                else:
                    client = ConnectedClient(
                        mac_address=mac,
                        ip_address=mac_to_ip.get(mac_lower),
                        connected_since=current_time,
                    )
                    self._client_cache[mac_lower] = client

                clients.append(client)

            # Clean up disconnected clients from cache
            current_macs = {mac.lower() for mac in mac_addresses}
            for mac in list(self._client_cache.keys()):
                if mac not in current_macs:
                    del self._client_cache[mac]

            return clients

        except Exception as e:
            if isinstance(e, HotspotException):
                raise
            raise HotspotException(
                f"Failed to get connected clients: {e}",
                error_code="CLIENT_LIST_FAILED",
                interface=interface,
            ) from e

    async def register_plugin(self, name: str, plugin: HotspotPlugin) -> None:
        """Register a hotspot plugin."""
        if not hasattr(plugin, "on_hotspot_start"):
            raise ValueError(f"Plugin {name} does not implement HotspotPlugin protocol")

        self.plugins[name] = plugin
        logger.info(f"Plugin '{name}' registered successfully")

    async def unregister_plugin(self, name: str) -> bool:
        """Unregister a hotspot plugin."""
        if name in self.plugins:
            del self.plugins[name]
            logger.info(f"Plugin '{name}' unregistered")
            return True
        return False

    async def _notify_plugins(self, event: str, *args: Any) -> None:
        """Notify all registered plugins of an event."""
        for name, plugin in self.plugins.items():
            try:
                method = getattr(plugin, f"on_{event}", None)
                if method:
                    await method(*args)
            except Exception as e:
                logger.error(f"Plugin '{name}' error on {event}: {e}")

    def _load_config(self) -> Optional[HotspotConfig]:
        """Load configuration from file with error handling."""
        if not self.config_file.exists():
            return None

        try:
            with self.config_file.open("r", encoding="utf-8") as f:
                data = json.load(f)
            config = HotspotConfig.from_dict(data)
            logger.debug(f"Configuration loaded from {self.config_file}")
            return config
        except (json.JSONDecodeError, ValueError) as e:
            logger.error(f"Failed to load configuration: {e}")
            return None

    async def save_config(self) -> None:
        """Save current configuration to file."""
        try:
            self.config_dir.mkdir(parents=True, exist_ok=True)
            with self.config_file.open("w", encoding="utf-8") as f:
                json.dump(self.current_config.to_dict(), f, indent=2)
            logger.info(f"Configuration saved to {self.config_file}")
        except OSError as e:
            raise ConfigurationError(
                f"Failed to save configuration: {e}",
                error_code="CONFIG_SAVE_FAILED",
                config_file=str(self.config_file),
            ) from e

    async def update_config(self, **kwargs: Any) -> None:
        """Update current configuration with new values."""
        try:
            # Create new config with updates
            current_dict = self.current_config.to_dict()
            current_dict.update(kwargs)

            # Validate new configuration
            new_config = HotspotConfig.from_dict(current_dict)
            self.current_config = new_config

            await self.save_config()
            logger.debug("Configuration updated", extra={"updates": kwargs})

        except ValueError as e:
            raise ConfigurationError(
                f"Invalid configuration update: {e}",
                error_code="CONFIG_INVALID",
                updates=kwargs,
            ) from e

    async def start(self, **kwargs: Any) -> bool:
        """
        Start the hotspot with enhanced error handling and validation.

        Args:
            **kwargs: Configuration overrides

        Returns:
            True if hotspot started successfully
        """
        try:
            await self._ensure_network_manager()

            # Update configuration if provided
            if kwargs:
                await self.update_config(**kwargs)

            cfg = self.current_config

            # Validate configuration
            if cfg.authentication.requires_password and not cfg.password:
                raise ConfigurationError(
                    "Password is required for secured networks",
                    error_code="PASSWORD_REQUIRED",
                    authentication=cfg.authentication.value,
                )

            # Validate interface
            await self.validate_interface(cfg.interface)

            # Build NetworkManager command
            cmd = [
                "nmcli",
                "dev",
                "wifi",
                "hotspot",
                "ifname",
                cfg.interface,
                "ssid",
                cfg.name,
            ]

            if cfg.password:
                cmd.extend(["password", cfg.password])

            # Execute hotspot creation
            result = await self.run_command(cmd)

            if not result.success:
                raise NetworkManagerError(
                    f"Failed to start hotspot: {result.stderr}",
                    error_code="HOTSPOT_START_FAILED",
                    command_result=result.to_dict(),
                )

            # Apply advanced configuration
            await self._apply_advanced_config(cfg)

            # Notify plugins
            await self._notify_plugins("hotspot_start", cfg)

            logger.success(f"Hotspot '{cfg.name}' started successfully")
            return True

        except HotspotException:
            raise
        except Exception as e:
            raise HotspotException(
                f"Unexpected error starting hotspot: {e}",
                error_code="HOTSPOT_START_UNEXPECTED",
            ) from e

    async def _apply_advanced_config(self, cfg: HotspotConfig) -> None:
        """Apply advanced hotspot configuration."""
        base_cmd = ["nmcli", "connection", "modify", "Hotspot"]

        commands = [
            [*base_cmd, "802-11-wireless-security.key-mgmt", cfg.authentication.value],
            [*base_cmd, "802-11-wireless-security.pairwise", cfg.encryption.value],
            [*base_cmd, "802-11-wireless.band", cfg.band.value],
            [*base_cmd, "802-11-wireless.channel", str(cfg.channel)],
            [*base_cmd, "802-11-wireless.hidden", "yes" if cfg.hidden else "no"],
        ]

        for cmd in commands:
            result = await self.run_command(cmd)
            if not result.success:
                logger.warning(
                    f"Failed to apply config: {' '.join(cmd)}: {result.stderr}"
                )

    async def stop(self) -> bool:
        """Stop the hotspot with error handling."""
        try:
            await self._ensure_network_manager()

            result = await self.run_command(["nmcli", "connection", "down", "Hotspot"])

            if result.success:
                await self._notify_plugins("hotspot_stop")
                logger.success("Hotspot stopped successfully")

                # Clear client cache
                self._client_cache.clear()

                return True
            else:
                logger.warning(f"Failed to stop hotspot: {result.stderr}")
                return False

        except NetworkManagerError:
            raise
        except Exception as e:
            raise HotspotException(
                f"Unexpected error stopping hotspot: {e}",
                error_code="HOTSPOT_STOP_UNEXPECTED",
            ) from e

    async def get_status(self) -> Dict[str, Any]:
        """Get comprehensive hotspot status information."""
        try:
            await self._ensure_network_manager()

            dev_status = await self.run_command(["nmcli", "dev", "status"])
            if not dev_status.success or "Hotspot" not in dev_status.stdout:
                return {"running": False}

            # Find interface running hotspot
            interface = None
            for line in dev_status.stdout.splitlines():
                if "Hotspot" in line:
                    interface = line.split()[0]
                    break

            if not interface:
                return {"running": False}

            # Get detailed connection information
            details = await self.run_command(["nmcli", "con", "show", "Hotspot"])

            # Get connected clients
            clients = await self.get_connected_clients(interface)

            return {
                "running": True,
                "interface": interface,
                "ssid": self._parse_detail(details.stdout, "802-11-wireless.ssid"),
                "ip_address": self._parse_detail(details.stdout, "IP4.ADDRESS"),
                "clients": [client.to_dict() for client in clients],
                "client_count": len(clients),
                "config": self.current_config.to_dict(),
            }

        except HotspotException:
            raise
        except Exception as e:
            raise HotspotException(
                f"Failed to get hotspot status: {e}", error_code="STATUS_FAILED"
            ) from e

    async def restart(self, **kwargs: Any) -> bool:
        """Restart the hotspot with optional configuration updates."""
        logger.info("Restarting hotspot...")

        await self.stop()
        await asyncio.sleep(1)  # Brief pause to ensure clean shutdown

        return await self.start(**kwargs)

    @asynccontextmanager
    async def managed_hotspot(
        self, **config_overrides: Any
    ) -> AsyncGenerator[HotspotManager, None]:
        """
        Context manager for automatic hotspot lifecycle management.

        Usage:
            async with manager.managed_hotspot(name="TempHotspot") as hotspot:
                # Hotspot is automatically started
                status = await hotspot.get_status()
                # Hotspot is automatically stopped when exiting context
        """
        try:
            success = await self.start(**config_overrides)
            if not success:
                raise HotspotException(
                    "Failed to start managed hotspot", error_code="MANAGED_START_FAILED"
                )

            yield self

        finally:
            try:
                await self.stop()
            except Exception as e:
                logger.error(f"Error stopping managed hotspot: {e}")

    async def monitor_clients(
        self,
        interval: int = 5,
        callback: Optional[Callable[[List[ConnectedClient]], Awaitable[None]]] = None,
    ) -> AsyncIterator[List[ConnectedClient]]:
        """
        Monitor connected clients with async generator pattern.

        Args:
            interval: Monitoring interval in seconds
            callback: Optional async callback for client updates

        Yields:
            List of currently connected clients
        """
        seen_clients: Dict[str, ConnectedClient] = {}

        try:
            while True:
                current_clients = await self.get_connected_clients()
                current_macs = {client.mac_address for client in current_clients}

                # Detect new and disconnected clients
                new_clients = [
                    client
                    for client in current_clients
                    if client.mac_address not in seen_clients
                ]

                disconnected_macs = set(seen_clients.keys()) - current_macs
                disconnected_clients = [seen_clients[mac] for mac in disconnected_macs]

                # Log and notify changes
                if new_clients:
                    for client in new_clients:
                        logger.info(f"Client connected: {client.mac_address}")
                        await self._notify_plugins("client_connect", client)

                if disconnected_clients:
                    for client in disconnected_clients:
                        logger.info(f"Client disconnected: {client.mac_address}")
                        await self._notify_plugins("client_disconnect", client)

                # Update seen clients
                seen_clients = {
                    client.mac_address: client for client in current_clients
                }

                # Call callback if provided
                if callback:
                    await callback(current_clients)

                yield current_clients

                await asyncio.sleep(interval)

        except asyncio.CancelledError:
            logger.debug("Client monitoring cancelled")
            raise
        except Exception as e:
            logger.error(f"Error in client monitoring: {e}")
            raise

    async def start_monitoring(self, interval: int = 5) -> None:
        """Start background client monitoring task."""
        if self._monitoring_task and not self._monitoring_task.done():
            logger.warning("Monitoring task already running")
            return

        async def monitor_task() -> None:
            async for clients in self.monitor_clients(interval):
                pass  # Monitoring happens in the async generator

        self._monitoring_task = asyncio.create_task(monitor_task())
        logger.info("Background client monitoring started")

    async def stop_monitoring(self) -> None:
        """Stop background client monitoring task."""
        if self._monitoring_task and not self._monitoring_task.done():
            self._monitoring_task.cancel()
            try:
                await self._monitoring_task
            except asyncio.CancelledError:
                pass
            logger.info("Background client monitoring stopped")

    async def __aenter__(self) -> HotspotManager:
        """Async context manager entry."""
        return self

    async def __aexit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """Async context manager exit with cleanup."""
        await self.stop_monitoring()
        # Note: We don't automatically stop the hotspot here as it might be intentional
