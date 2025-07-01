#!/usr/bin/env python3
"""
Asynchronous command-line interface for the WiFi Hotspot Manager.
"""

import argparse
import asyncio
import sys
from typing import Any, Dict

from loguru import logger

from .hotspot_manager import HotspotManager
from .models import AuthenticationType, BandType, EncryptionType


def setup_logger(verbose: bool) -> None:
    logger.remove()
    level = "DEBUG" if verbose else "INFO"
    logger.add(sys.stderr, level=level, format="<level>{message}</level>")


class HotspotCLI:
    def __init__(self) -> None:
        self.manager = HotspotManager()
        self.parser = self._create_parser()

    def _create_parser(self) -> argparse.ArgumentParser:
        parser = argparse.ArgumentParser(description="WiFi Hotspot Manager")
        parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output.")

        subparsers = parser.add_subparsers(dest="action", required=True)
        self._add_start_parser(subparsers)
        self._add_stop_parser(subparsers)
        self._add_status_parser(subparsers)
        self._add_restart_parser(subparsers)
        self._add_clients_parser(subparsers)

        return parser

    def _add_start_parser(self, subparsers: Any) -> None:
        p = subparsers.add_parser("start", help="Start a hotspot.")
        p.add_argument("--name", help="SSID of the hotspot.")
        p.add_argument("--password", help="Password for the hotspot.")
        p.add_argument("--auth", choices=[e.value for e in AuthenticationType])
        p.add_argument("--enc", choices=[e.value for e in EncryptionType])
        p.add_argument("--band", choices=[e.value for e in BandType])
        p.add_argument("--channel", type=int)
        p.add_argument("--hidden", action="store_true")

    def _add_stop_parser(self, subparsers: Any) -> None:
        subparsers.add_parser("stop", help="Stop the hotspot.")

    def _add_status_parser(self, subparsers: Any) -> None:
        subparsers.add_parser("status", help="Show hotspot status.")

    def _add_restart_parser(self, subparsers: Any) -> None:
        p = subparsers.add_parser("restart", help="Restart the hotspot.")
        p.add_argument("--name", help="New SSID for the hotspot.")
        p.add_argument("--password", help="New password for the hotspot.")

    def _add_clients_parser(self, subparsers: Any) -> None:
        p = subparsers.add_parser("clients", help="List or monitor connected clients.")
        p.add_argument("--monitor", action="store_true", help="Monitor clients in real-time.")
        p.add_argument("--interval", type=int, default=5, help="Monitoring interval.")

    async def run(self) -> int:
        args = self.parser.parse_args()
        setup_logger(args.verbose)

        action_map = {
            "start": self.start_hotspot,
            "stop": self.stop_hotspot,
            "status": self.show_status,
            "restart": self.restart_hotspot,
            "clients": self.handle_clients,
        }

        try:
            await action_map[args.action](args)
            return 0
        except (ValueError, KeyError) as e:
            logger.error(f"Error: {e}")
            return 1

    async def start_hotspot(self, args: argparse.Namespace) -> None:
        params = self._collect_params(args)
        if await self.manager.start(**params):
            logger.info("Hotspot started successfully.")
        else:
            logger.error("Failed to start hotspot.")

    async def stop_hotspot(self, args: argparse.Namespace) -> None:
        if await self.manager.stop():
            logger.info("Hotspot stopped successfully.")
        else:
            logger.error("Failed to stop hotspot.")

    async def show_status(self, args: argparse.Namespace) -> None:
        status = await self.manager.get_status()
        if not status.get("running"):
            print("Hotspot is not running.")
            return
        for key, value in status.items():
            print(f"{key.replace('_', ' ').title()}: {value}")

    async def restart_hotspot(self, args: argparse.Namespace) -> None:
        params = self._collect_params(args)
        if await self.manager.restart(**params):
            logger.info("Hotspot restarted successfully.")
        else:
            logger.error("Failed to restart hotspot.")

    async def handle_clients(self, args: argparse.Namespace) -> None:
        if args.monitor:
            await self.manager.monitor_clients(args.interval)
        else:
            clients = await self.manager.get_connected_clients()
            print(f"Found {len(clients)} clients:")
            for client in clients:
                print(f"- {client.mac_address} (IP: {client.ip_address or 'N/A'})")

    def _collect_params(self, args: argparse.Namespace) -> Dict[str, Any]:
        return {k: v for k, v in vars(args).items() if v is not None and k != "action"}


def main() -> None:
    cli = HotspotCLI()
    try:
        asyncio.run(cli.run())
    except KeyboardInterrupt:
        logger.info("Exiting.")
    except Exception as e:
        logger.critical(f"An unexpected error occurred: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
