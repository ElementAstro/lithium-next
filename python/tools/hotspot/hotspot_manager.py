#!/usr/bin/env python3
"""
Optimized Hotspot Manager with modern Python features.
"""

import asyncio
import json
import re
import shutil
from pathlib import Path
from typing import Any, Awaitable, Callable, Dict, List, Optional

from loguru import logger

from .command_utils import run_command_async
from .models import (
    AuthenticationType,
    ConnectedClient,
    HotspotConfig,
)


class HotspotManager:
    """
    Manages WiFi hotspots using NetworkManager with an extensible, async-first design.
    """

    def __init__(
        self,
        config: Optional[HotspotConfig] = None,
        config_dir: Optional[Path] = None,
        runner: Optional[Callable[..., Awaitable[Any]]] = None,
    ):
        self.config_dir = config_dir or Path.home() / ".config" / "hotspot-manager"
        self.config_file = self.config_dir / "config.json"
        self.run_command = runner or run_command_async
        self.plugins: Dict[str, Callable[..., Any]] = {}

        if not self._is_network_manager_available():
            logger.warning("NetworkManager (nmcli) is not available.")

        self.current_config = config or self._load_config() or HotspotConfig()
        logger.debug(f"Initialized with config: {self.current_config}")

    def _is_network_manager_available(self) -> bool:
        return shutil.which("nmcli") is not None

    def _parse_detail(self, output: str, key: str) -> Optional[str]:
        match = re.search(rf"^{key}:\s*(.*)$", output, re.MULTILINE)  # Fixed: raw string for regex
        return match.group(1).strip() if match else None

    async def get_connected_clients(
        self, interface: Optional[str] = None
    ) -> List[ConnectedClient]:
        if not interface:
            status = await self.get_status()
            if not status.get("running"):
                return []
            interface = status["interface"]

        if not interface:  # Added null check
            return []

        iw_result = await self.run_command(
            ["iw", "dev", interface, "station", "dump"]  # Fixed: interface is now guaranteed non-None
        )
        if not iw_result.success:
            return []

        clients = [
            ConnectedClient(mac_address=mac)
            for mac in re.findall(r"Station (\S+)", iw_result.stdout)
        ]

        arp_result = await self.run_command(["arp", "-n"])
        if arp_result.success:
            mac_to_ip = {
                mac: ip
                for ip, mac in re.findall(r"(\S+)\s+ether\s+(\S+)", arp_result.stdout)
            }
            for client in clients:
                client.ip_address = mac_to_ip.get(client.mac_address)

        return clients

    async def register_plugin(self, name: str, plugin: Callable[..., Any]) -> None:
        self.plugins[name] = plugin
        logger.info(f"Plugin '{name}' registered.")

    def _load_config(self) -> Optional[HotspotConfig]:
        if self.config_file.exists():
            try:
                with self.config_file.open('r') as f:
                    return HotspotConfig.from_dict(json.load(f))
            except (json.JSONDecodeError, KeyError) as e:
                logger.error(f"Failed to load configuration: {e}")
        return None

    async def save_config(self) -> None:
        self.config_dir.mkdir(parents=True, exist_ok=True)
        with self.config_file.open('w') as f:
            json.dump(self.current_config.to_dict(), f, indent=2)
        logger.info(f"Configuration saved to {self.config_file}")

    async def update_config(self, **kwargs: Any) -> None:
        for key, value in kwargs.items():
            if hasattr(self.current_config, key):
                setattr(self.current_config, key, value)
        await self.save_config()

    async def start(self, **kwargs: Any) -> bool:
        await self.update_config(**kwargs)
        cfg = self.current_config

        if cfg.authentication != AuthenticationType.NONE and (
            not cfg.password or len(cfg.password) < 8
        ):
            raise ValueError("A password of at least 8 characters is required.")

        cmd = ["nmcli", "dev", "wifi", "hotspot", "ifname", cfg.interface, "ssid", cfg.name]
        if cfg.password:
            cmd.extend(["password", cfg.password])

        result = await self.run_command(cmd)
        if not result.success:
            return False

        await self._apply_advanced_config(cfg)
        logger.info(f"Hotspot '{cfg.name}' is now running.")
        return True

    async def _apply_advanced_config(self, cfg: HotspotConfig) -> None:
        base_cmd = ["nmcli", "connection", "modify", "Hotspot"]
        commands = [
            [*base_cmd, "802-11-wireless-security.key-mgmt", cfg.authentication.value],
            [*base_cmd, "802-11-wireless-security.pairwise", cfg.encryption.value],
            [*base_cmd, "802-11-wireless.band", cfg.band.value],
            [*base_cmd, "802-11-wireless.channel", str(cfg.channel)],
            [*base_cmd, "802-11-wireless.hidden", "yes" if cfg.hidden else "no"],
        ]
        for cmd in commands:
            await self.run_command(cmd)

    async def stop(self) -> bool:
        result = await self.run_command(["nmcli", "connection", "down", "Hotspot"])
        if result.success:
            logger.info("Hotspot has been stopped.")
        return result.success

    async def get_status(self) -> Dict[str, Any]:
        dev_status = await self.run_command(["nmcli", "dev", "status"])
        if not dev_status.success or "Hotspot" not in dev_status.stdout:
            return {"running": False}

        interface = next(
            (p.split()[0] for p in dev_status.stdout.splitlines() if "Hotspot" in p),
            None,
        )
        if not interface:
            return {"running": False}

        details = await self.run_command(["nmcli", "con", "show", "Hotspot"])
        return {
            "running": True,
            "interface": interface,
            "ssid": self._parse_detail(details.stdout, "802-11-wireless.ssid"),
            "ip_address": self._parse_detail(details.stdout, "IP4.ADDRESS"),
            "clients": await self.get_connected_clients(interface),
        }

    async def restart(self, **kwargs: Any) -> bool:
        await self.stop()
        await asyncio.sleep(1)
        return await self.start(**kwargs)

    async def monitor_clients(
        self, interval: int = 5, callback: Optional[Callable[..., None]] = None
    ) -> None:
        seen_clients = set()
        while True:
            clients = await self.get_connected_clients()
            current_macs = {c.mac_address for c in clients}

            new = current_macs - seen_clients
            gone = seen_clients - current_macs

            if new:
                logger.info(f"New clients: {new}")
            if gone:
                logger.info(f"Clients left: {gone}")

            if callback:
                callback(clients)
            else:
                print(f"Clients: {[c.mac_address for c in clients]}")

            seen_clients = current_macs
            await asyncio.sleep(interval)
