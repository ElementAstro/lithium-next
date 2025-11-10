#!/usr/bin/env python3
"""
Command-line interface for WiFi Hotspot Manager.
Provides a user-friendly interface for managing WiFi hotspots from the command line.
"""

import sys
import argparse
import asyncio

from loguru import logger
from .models import AuthenticationType, EncryptionType, BandType
from .hotspot_manager import HotspotManager


def setup_logger(verbose: bool = False):
    """Configure loguru logger with appropriate verbosity level."""
    # Remove default logger
    logger.remove()

    # Set log level based on verbosity
    log_level = "DEBUG" if verbose else "INFO"

    # Add a handler with custom format
    logger.add(
        sys.stderr,
        format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>",
        level=log_level,
    )


def main():
    """
    Main entry point for command-line usage.

    Parses command-line arguments and executes the requested action.
    """
    parser = argparse.ArgumentParser(description="Advanced WiFi Hotspot Manager")
    subparsers = parser.add_subparsers(dest="action", help="Action to perform")

    # Start command
    start_parser = subparsers.add_parser("start", help="Start a WiFi hotspot")
    start_parser.add_argument("--name", help="Hotspot name")
    start_parser.add_argument("--password", help="Hotspot password")
    start_parser.add_argument(
        "--authentication",
        choices=[t.value for t in AuthenticationType],
        help="Authentication type",
    )
    start_parser.add_argument(
        "--encryption",
        choices=[t.value for t in EncryptionType],
        help="Encryption type",
    )
    start_parser.add_argument("--channel", type=int, help="Channel number")
    start_parser.add_argument("--interface", help="Network interface")
    start_parser.add_argument(
        "--band", choices=[b.value for b in BandType], help="WiFi band"
    )
    start_parser.add_argument(
        "--hidden", action="store_true", help="Make the hotspot hidden (not broadcast)"
    )
    start_parser.add_argument(
        "--max-clients", type=int, help="Maximum number of clients"
    )

    # Stop command
    subparsers.add_parser("stop", help="Stop the WiFi hotspot")

    # Status command
    subparsers.add_parser("status", help="Show hotspot status")

    # List command
    subparsers.add_parser("list", help="List active connections")

    # Config command
    config_parser = subparsers.add_parser("config", help="Update hotspot configuration")
    config_parser.add_argument("--name", help="Hotspot name")
    config_parser.add_argument("--password", help="Hotspot password")
    config_parser.add_argument(
        "--authentication",
        choices=[t.value for t in AuthenticationType],
        help="Authentication type",
    )
    config_parser.add_argument(
        "--encryption",
        choices=[t.value for t in EncryptionType],
        help="Encryption type",
    )
    config_parser.add_argument("--channel", type=int, help="Channel number")
    config_parser.add_argument("--interface", help="Network interface")
    config_parser.add_argument(
        "--band", choices=[b.value for b in BandType], help="WiFi band"
    )
    config_parser.add_argument(
        "--hidden", action="store_true", help="Make the hotspot hidden (not broadcast)"
    )
    config_parser.add_argument(
        "--max-clients", type=int, help="Maximum number of clients"
    )

    # Restart command
    restart_parser = subparsers.add_parser("restart", help="Restart the WiFi hotspot")
    restart_parser.add_argument("--name", help="Hotspot name")
    restart_parser.add_argument("--password", help="Hotspot password")
    restart_parser.add_argument(
        "--authentication",
        choices=[t.value for t in AuthenticationType],
        help="Authentication type",
    )
    restart_parser.add_argument(
        "--encryption",
        choices=[t.value for t in EncryptionType],
        help="Encryption type",
    )
    restart_parser.add_argument("--channel", type=int, help="Channel number")
    restart_parser.add_argument("--interface", help="Network interface")
    restart_parser.add_argument(
        "--band", choices=[b.value for b in BandType], help="WiFi band"
    )
    restart_parser.add_argument(
        "--hidden", action="store_true", help="Make the hotspot hidden (not broadcast)"
    )
    restart_parser.add_argument(
        "--max-clients", type=int, help="Maximum number of clients"
    )

    # Interfaces command
    subparsers.add_parser("interfaces", help="List available network interfaces")

    # Clients command
    clients_parser = subparsers.add_parser("clients", help="List connected clients")
    clients_parser.add_argument(
        "--monitor", action="store_true", help="Continuously monitor clients"
    )
    clients_parser.add_argument(
        "--interval", type=int, default=5, help="Monitoring interval in seconds"
    )

    # Channels command
    channels_parser = subparsers.add_parser(
        "channels", help="List available WiFi channels"
    )
    channels_parser.add_argument("--interface", help="Network interface to check")

    # Add global verbose flag
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable verbose logging"
    )

    # Parse arguments
    args = parser.parse_args()

    # If no arguments were provided, show help
    if not args.action:
        parser.print_help()
        return 1

    # Configure logger based on verbose flag
    setup_logger(args.verbose)

    # Create the hotspot manager
    manager = HotspotManager()

    # Process commands using pattern matching (Python 3.10+)
    match args.action:
        case "start":
            # Collect parameters for start command
            params = {}
            for param in [
                "name",
                "password",
                "authentication",
                "encryption",
                "channel",
                "interface",
                "band",
                "hidden",
                "max_clients",
            ]:
                if hasattr(args, param) and getattr(args, param) is not None:
                    params[param] = getattr(args, param)

            # Convert string enum values to actual enums
            if "authentication" in params:
                params["authentication"] = AuthenticationType(params["authentication"])
            if "encryption" in params:
                params["encryption"] = EncryptionType(params["encryption"])
            if "band" in params:
                params["band"] = BandType(params["band"])

            success = manager.start(**params)
            return 0 if success else 1

        case "stop":
            success = manager.stop()
            return 0 if success else 1

        case "status":
            manager.status()
            return 0

        case "list":
            manager.list()
            return 0

        case "config":
            # Collect parameters for config command
            params = {}
            for param in [
                "name",
                "password",
                "authentication",
                "encryption",
                "channel",
                "interface",
                "band",
                "hidden",
                "max_clients",
            ]:
                if hasattr(args, param) and getattr(args, param) is not None:
                    params[param] = getattr(args, param)

            # Convert string enum values to actual enums
            if "authentication" in params:
                params["authentication"] = AuthenticationType(params["authentication"])
            if "encryption" in params:
                params["encryption"] = EncryptionType(params["encryption"])
            if "band" in params:
                params["band"] = BandType(params["band"])

            success = manager.set(**params)
            return 0 if success else 1

        case "restart":
            # Collect parameters for restart command
            params = {}
            for param in [
                "name",
                "password",
                "authentication",
                "encryption",
                "channel",
                "interface",
                "band",
                "hidden",
                "max_clients",
            ]:
                if hasattr(args, param) and getattr(args, param) is not None:
                    params[param] = getattr(args, param)

            # Convert string enum values to actual enums
            if "authentication" in params:
                params["authentication"] = AuthenticationType(params["authentication"])
            if "encryption" in params:
                params["encryption"] = EncryptionType(params["encryption"])
            if "band" in params:
                params["band"] = BandType(params["band"])

            success = manager.restart(**params)
            return 0 if success else 1

        case "interfaces":
            interfaces = manager.get_network_interfaces()
            if interfaces:
                print("**Available network interfaces:**")
                for interface in interfaces:
                    print(
                        f"- {interface['name']} ({interface['type']}): {interface['state']}"
                    )
            else:
                print("No network interfaces found")
            return 0

        case "clients":
            if args.monitor:
                # Run asynchronously for monitoring
                try:
                    print("Monitoring clients... Press Ctrl+C to stop")
                    asyncio.run(manager.monitor_clients(interval=args.interval))
                except KeyboardInterrupt:
                    print("\nMonitoring stopped")
            else:
                # Just show current clients
                clients = manager.get_connected_clients()
                if clients:
                    print(f"**{len(clients)} clients connected:**")
                    for client in clients:
                        ip = client.get("ip_address", "Unknown IP")
                        hostname = client.get("hostname", "")
                        if hostname:
                            print(f"- {client['mac_address']} ({ip}) - {hostname}")
                        else:
                            print(f"- {client['mac_address']} ({ip})")
                else:
                    print("No clients connected")
            return 0

        case "channels":
            interface = args.interface or manager.current_config.interface
            channels = manager.get_available_channels(interface)
            if channels:
                print(f"**Available channels for {interface}:**")
                print(", ".join(map(str, channels)))
            else:
                print(f"No channel information available for {interface}")
            return 0

        case _:
            parser.print_help()
            return 1


if __name__ == "__main__":
    sys.exit(main())
