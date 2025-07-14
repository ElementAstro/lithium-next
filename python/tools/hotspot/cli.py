#!/usr/bin/env python3
"""
Enhanced command-line interface for WiFi Hotspot Manager with modern Python features.

This module provides a comprehensive CLI with advanced argument parsing, structured
logging, rich output formatting, and robust error handling.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Any, AsyncContextManager, AsyncGenerator, Dict, List, Optional, Union

from loguru import logger
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn
from rich.prompt import Confirm

from .hotspot_manager import HotspotManager
from .models import (
    AuthenticationType,
    BandType,
    EncryptionType,
    HotspotConfig,
    ConnectedClient,
    HotspotException,
)


class CLIError(Exception):
    """Base exception for CLI-related errors."""

    pass


class LoggerManager:
    """Enhanced logger management with structured formatting."""

    @staticmethod
    def setup_logger(
        verbose: bool = False,
        quiet: bool = False,
        log_file: Optional[Path] = None,
        json_logs: bool = False,
    ) -> None:
        """Configure loguru with enhanced formatting and multiple outputs."""
        # Remove default logger
        logger.remove()

        if quiet:
            return  # No logging output

        # Determine log level
        level = "DEBUG" if verbose else "INFO"

        # Console format
        if json_logs:
            console_format = (
                "{time:YYYY-MM-DD HH:mm:ss} | "
                "{level: <8} | "
                "{name}:{function}:{line} | "
                "{message} | "
                "{extra}"
            )
        else:
            console_format = (
                "<green>{time:HH:mm:ss}</green> | "
                "<level>{level: <8}</level> | "
                "<cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> | "
                "<level>{message}</level>"
            )

        # Add console handler
        logger.add(
            sys.stderr,
            level=level,
            format=console_format,
            colorize=not json_logs,
            serialize=json_logs,
        )

        # Add file handler if specified
        if log_file:
            log_file.parent.mkdir(parents=True, exist_ok=True)
            logger.add(
                log_file,
                level="DEBUG",
                format=(
                    "{time:YYYY-MM-DD HH:mm:ss.SSS} | "
                    "{level: <8} | "
                    "{name}:{function}:{line} | "
                    "{message} | "
                    "{extra}"
                ),
                rotation="10 MB",
                retention="1 week",
                serialize=True,
            )

        # Configure exception handling
        logger.configure(
            handlers=[
                {
                    "sink": sys.stderr,
                    "level": level,
                    "format": console_format,
                    "colorize": not json_logs,
                    "serialize": json_logs,
                    "catch": True,
                    "backtrace": verbose,
                    "diagnose": verbose,
                }
            ]
        )


class RichOutput:
    """Rich console output manager for enhanced CLI experience."""

    def __init__(self, console: Optional[Console] = None) -> None:
        self.console = console or Console()

    def print_status(self, status: Dict[str, Any]) -> None:
        """Display hotspot status in a formatted table."""
        if not status.get("running"):
            self.console.print(
                Panel("[red]Hotspot is not running[/red]", title="Status")
            )
            return

        table = Table(title="Hotspot Status", show_header=True)
        table.add_column("Property", style="cyan")
        table.add_column("Value", style="green")

        # Add status information
        for key, value in status.items():
            if key == "clients":
                continue  # Handle clients separately

            display_key = key.replace("_", " ").title()
            if isinstance(value, dict):
                display_value = json.dumps(value, indent=2)
            elif isinstance(value, bool):
                display_value = "✓" if value else "✗"
            else:
                display_value = str(value)

            table.add_row(display_key, display_value)

        self.console.print(table)

        # Display connected clients if any
        if status.get("clients"):
            self.print_clients(status["clients"])

    def print_clients(self, clients: List[Dict[str, Any]]) -> None:
        """Display connected clients in a formatted table."""
        if not clients:
            self.console.print("\n[yellow]No clients connected[/yellow]")
            return

        table = Table(title=f"Connected Clients ({len(clients)})", show_header=True)
        table.add_column("MAC Address", style="cyan")
        table.add_column("IP Address", style="green")
        table.add_column("Hostname", style="blue")
        table.add_column("Connected", style="yellow")
        table.add_column("Status", style="magenta")

        for client_data in clients:
            table.add_row(
                client_data.get("mac_address", "N/A"),
                client_data.get("ip_address", "N/A"),
                client_data.get("hostname", "Unknown"),
                client_data.get("connection_duration_str", "N/A"),
                "🟢 Active" if client_data.get("is_active") else "🟡 Idle",
            )

        self.console.print(table)

    def print_interfaces(self, interfaces: List[Dict[str, Any]]) -> None:
        """Display available network interfaces."""
        table = Table(title="Available WiFi Interfaces", show_header=True)
        table.add_column("Interface", style="cyan")
        table.add_column("Type", style="green")
        table.add_column("State", style="yellow")
        table.add_column("Driver", style="blue")

        for iface in interfaces:
            state_color = "green" if iface.get("is_available") else "red"
            table.add_row(
                iface.get("name", "N/A"),
                iface.get("type", "N/A"),
                f"[{state_color}]{iface.get('state', 'N/A')}[/{state_color}]",
                iface.get("driver", "N/A"),
            )

        self.console.print(table)

    @asynccontextmanager
    async def progress_context(
        self, description: str
    ) -> AsyncGenerator[Progress, None]:
        """Context manager for progress indication."""
        with Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            console=self.console,
        ) as progress:
            task = progress.add_task(description, total=None)
            yield progress
            progress.update(task, completed=True)


class EnhancedArgumentParser:
    """Enhanced argument parser with validation and help formatting."""

    def __init__(self) -> None:
        self.parser = self._create_parser()

    def _create_parser(self) -> argparse.ArgumentParser:
        """Create the main argument parser with enhanced configuration."""
        parser = argparse.ArgumentParser(
            prog="wifi-hotspot",
            description="Enhanced WiFi Hotspot Manager with modern Python features",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Examples:
  wifi-hotspot start --name MyHotspot --password mypassword
  wifi-hotspot status --json
  wifi-hotspot clients --monitor --interval 3
  wifi-hotspot config save --file /path/to/config.json
            """,
        )

        # Global options
        parser.add_argument(
            "-v", "--verbose", action="store_true", help="Enable verbose debug output"
        )
        parser.add_argument(
            "-q",
            "--quiet",
            action="store_true",
            help="Suppress all output except errors",
        )
        parser.add_argument(
            "--log-file", type=Path, help="Write logs to specified file"
        )
        parser.add_argument(
            "--json-logs", action="store_true", help="Output logs in JSON format"
        )
        parser.add_argument(
            "--no-color", action="store_true", help="Disable colored output"
        )

        # Create subcommands
        subparsers = parser.add_subparsers(
            dest="command", help="Available commands", metavar="COMMAND"
        )

        self._add_start_parser(subparsers)
        self._add_stop_parser(subparsers)
        self._add_status_parser(subparsers)
        self._add_restart_parser(subparsers)
        self._add_clients_parser(subparsers)
        self._add_config_parser(subparsers)
        self._add_interfaces_parser(subparsers)

        return parser

    def _add_start_parser(self, subparsers: Any) -> None:
        """Add the 'start' subcommand parser."""
        parser = subparsers.add_parser(
            "start",
            help="Start a WiFi hotspot",
            description="Start a WiFi hotspot with specified configuration",
        )

        parser.add_argument(
            "--name", "--ssid", help="SSID (network name) for the hotspot"
        )
        parser.add_argument(
            "--password", help="Password for securing the hotspot (8-63 characters)"
        )
        parser.add_argument(
            "--interface", help="Network interface to use (e.g., wlan0)"
        )
        parser.add_argument(
            "--auth",
            "--authentication",
            choices=[e.value for e in AuthenticationType],
            help="Authentication method",
        )
        parser.add_argument(
            "--enc",
            "--encryption",
            choices=[e.value for e in EncryptionType],
            help="Encryption algorithm",
        )
        parser.add_argument(
            "--band",
            choices=[e.value for e in BandType],
            help="Frequency band (2.4GHz, 5GHz, or dual)",
        )
        parser.add_argument(
            "--channel",
            type=int,
            choices=range(1, 15),
            help="WiFi channel (1-14 for 2.4GHz)",
        )
        parser.add_argument(
            "--max-clients", type=int, help="Maximum number of concurrent clients"
        )
        parser.add_argument(
            "--hidden", action="store_true", help="Hide the network SSID"
        )
        parser.add_argument(
            "--config-file", type=Path, help="Load configuration from JSON file"
        )

    def _add_stop_parser(self, subparsers: Any) -> None:
        """Add the 'stop' subcommand parser."""
        parser = subparsers.add_parser(
            "stop",
            help="Stop the WiFi hotspot",
            description="Stop the currently running hotspot",
        )
        parser.add_argument(
            "--force", action="store_true", help="Force stop without confirmation"
        )

    def _add_status_parser(self, subparsers: Any) -> None:
        """Add the 'status' subcommand parser."""
        parser = subparsers.add_parser(
            "status",
            help="Show hotspot status",
            description="Display current hotspot status and connected clients",
        )
        parser.add_argument(
            "--json", action="store_true", help="Output status in JSON format"
        )
        parser.add_argument(
            "--watch", action="store_true", help="Continuously monitor status"
        )
        parser.add_argument(
            "--interval",
            type=int,
            default=5,
            help="Update interval for watch mode (seconds)",
        )

    def _add_restart_parser(self, subparsers: Any) -> None:
        """Add the 'restart' subcommand parser."""
        parser = subparsers.add_parser(
            "restart",
            help="Restart the hotspot",
            description="Restart the hotspot with optional configuration changes",
        )
        # Inherit start options
        self._add_start_options(parser)

    def _add_clients_parser(self, subparsers: Any) -> None:
        """Add the 'clients' subcommand parser."""
        parser = subparsers.add_parser(
            "clients",
            help="Manage connected clients",
            description="List or monitor connected clients",
        )
        parser.add_argument(
            "--monitor", action="store_true", help="Monitor clients in real-time"
        )
        parser.add_argument(
            "--interval", type=int, default=5, help="Monitoring interval in seconds"
        )
        parser.add_argument("--json", action="store_true", help="Output in JSON format")

    def _add_config_parser(self, subparsers: Any) -> None:
        """Add the 'config' subcommand parser."""
        parser = subparsers.add_parser(
            "config",
            help="Manage hotspot configuration",
            description="Save, load, or validate hotspot configurations",
        )

        config_subparsers = parser.add_subparsers(
            dest="config_action", help="Configuration actions"
        )

        # Save config
        save_parser = config_subparsers.add_parser(
            "save", help="Save current configuration"
        )
        save_parser.add_argument(
            "--file", type=Path, required=True, help="Output file path"
        )

        # Load config
        load_parser = config_subparsers.add_parser(
            "load", help="Load configuration from file"
        )
        load_parser.add_argument(
            "--file", type=Path, required=True, help="Configuration file path"
        )

        # Validate config
        validate_parser = config_subparsers.add_parser(
            "validate", help="Validate configuration file"
        )
        validate_parser.add_argument(
            "file", type=Path, help="Configuration file to validate"
        )

    def _add_interfaces_parser(self, subparsers: Any) -> None:
        """Add the 'interfaces' subcommand parser."""
        parser = subparsers.add_parser(
            "interfaces",
            help="List available network interfaces",
            description="Show available WiFi interfaces that can be used for hotspots",
        )
        parser.add_argument("--json", action="store_true", help="Output in JSON format")

    def _add_start_options(self, parser: argparse.ArgumentParser) -> None:
        """Add common start/restart options to a parser."""
        parser.add_argument("--name", help="New SSID for the hotspot")
        parser.add_argument("--password", help="New password for the hotspot")
        # Add other common options as needed

    def parse_args(self, args: Optional[List[str]] = None) -> argparse.Namespace:
        """Parse command line arguments with validation."""
        parsed_args = self.parser.parse_args(args)

        # Validate argument combinations
        if parsed_args.quiet and parsed_args.verbose:
            self.parser.error("--quiet and --verbose are mutually exclusive")

        return parsed_args


class HotspotCLI:
    """Enhanced command-line interface for WiFi Hotspot Manager."""

    def __init__(self) -> None:
        self.manager: Optional[HotspotManager] = None
        self.parser = EnhancedArgumentParser()
        self.output = RichOutput()

    async def run(self, args: Optional[List[str]] = None) -> int:
        """Main CLI entry point with comprehensive error handling."""
        try:
            parsed_args = self.parser.parse_args(args)

            # Setup logging
            LoggerManager.setup_logger(
                verbose=parsed_args.verbose,
                quiet=parsed_args.quiet,
                log_file=parsed_args.log_file,
                json_logs=parsed_args.json_logs,
            )

            # Disable colors if requested
            if parsed_args.no_color:
                self.output.console = Console(color_system=None)

            # Initialize manager
            self.manager = HotspotManager()

            # Route to appropriate handler
            command_map = {
                "start": self.handle_start,
                "stop": self.handle_stop,
                "status": self.handle_status,
                "restart": self.handle_restart,
                "clients": self.handle_clients,
                "config": self.handle_config,
                "interfaces": self.handle_interfaces,
            }

            if parsed_args.command not in command_map:
                self.parser.parser.print_help()
                return 1

            logger.debug(f"Executing command: {parsed_args.command}")
            await command_map[parsed_args.command](parsed_args)

            return 0

        except KeyboardInterrupt:
            logger.info("Operation cancelled by user")
            return 130  # Standard exit code for SIGINT
        except HotspotException as e:
            logger.error(f"Hotspot error: {e}")
            if hasattr(e, "error_code") and e.error_code:
                logger.debug(f"Error code: {e.error_code}")
            return 1
        except CLIError as e:
            logger.error(f"CLI error: {e}")
            return 1
        except Exception as e:
            logger.exception(f"Unexpected error: {e}")
            return 1
        finally:
            # Cleanup
            if self.manager:
                try:
                    await self.manager.stop_monitoring()
                    await self.manager.__aexit__(None, None, None)
                except Exception as e:
                    logger.debug(f"Cleanup error: {e}")

    async def handle_start(self, args: argparse.Namespace) -> None:
        """Handle the 'start' command."""
        assert self.manager is not None

        config_updates = self._extract_config_updates(args)

        # Load config from file if specified
        if hasattr(args, "config_file") and args.config_file:
            try:
                config = HotspotConfig.from_file(args.config_file)
                self.manager.current_config = config
                logger.info(f"Loaded configuration from {args.config_file}")
            except Exception as e:
                raise CLIError(f"Failed to load config file: {e}") from e

        async with self.output.progress_context("Starting hotspot..."):
            success = await self.manager.start(**config_updates)

        if success:
            self.output.console.print("[green]✓[/green] Hotspot started successfully")

            # Show status
            status = await self.manager.get_status()
            self.output.print_status(status)
        else:
            raise CLIError("Failed to start hotspot")

    async def handle_stop(self, args: argparse.Namespace) -> None:
        """Handle the 'stop' command."""
        assert self.manager is not None

        # Confirm if not forced
        if not getattr(args, "force", False):
            status = await self.manager.get_status()
            if status.get("running") and status.get("client_count", 0) > 0:
                if not Confirm.ask(
                    f"Hotspot has {status['client_count']} connected clients. Stop anyway?"
                ):
                    logger.info("Stop operation cancelled")
                    return

        async with self.output.progress_context("Stopping hotspot..."):
            success = await self.manager.stop()

        if success:
            self.output.console.print("[green]✓[/green] Hotspot stopped successfully")
        else:
            raise CLIError("Failed to stop hotspot")

    async def handle_status(self, args: argparse.Namespace) -> None:
        """Handle the 'status' command."""
        assert self.manager is not None

        if getattr(args, "watch", False):
            # Watch mode
            try:
                while True:
                    status = await self.manager.get_status()

                    if args.json:
                        print(json.dumps(status, indent=2))
                    else:
                        self.output.console.clear()
                        self.output.print_status(status)

                    await asyncio.sleep(args.interval)
            except KeyboardInterrupt:
                pass
        else:
            # Single status check
            status = await self.manager.get_status()

            if args.json:
                print(json.dumps(status, indent=2))
            else:
                self.output.print_status(status)

    async def handle_restart(self, args: argparse.Namespace) -> None:
        """Handle the 'restart' command."""
        assert self.manager is not None

        config_updates = self._extract_config_updates(args)

        async with self.output.progress_context("Restarting hotspot..."):
            success = await self.manager.restart(**config_updates)

        if success:
            self.output.console.print("[green]✓[/green] Hotspot restarted successfully")

            # Show status
            status = await self.manager.get_status()
            self.output.print_status(status)
        else:
            raise CLIError("Failed to restart hotspot")

    async def handle_clients(self, args: argparse.Namespace) -> None:
        """Handle the 'clients' command."""
        assert self.manager is not None

        if args.monitor:
            # Monitor mode
            try:
                async for clients in self.manager.monitor_clients(args.interval):
                    if args.json:
                        client_data = [client.to_dict() for client in clients]
                        print(json.dumps(client_data, indent=2))
                    else:
                        self.output.console.clear()
                        self.output.print_clients(
                            [client.to_dict() for client in clients]
                        )
            except KeyboardInterrupt:
                pass
        else:
            # Single client list
            clients = await self.manager.get_connected_clients()

            if args.json:
                client_data = [client.to_dict() for client in clients]
                print(json.dumps(client_data, indent=2))
            else:
                self.output.print_clients([client.to_dict() for client in clients])

    async def handle_config(self, args: argparse.Namespace) -> None:
        """Handle the 'config' command."""
        assert self.manager is not None

        if args.config_action == "save":
            await self.manager.save_config()
            if args.file != self.manager.config_file:
                # Copy to specified file
                import shutil

                shutil.copy2(self.manager.config_file, args.file)

            self.output.console.print(
                f"[green]✓[/green] Configuration saved to {args.file}"
            )

        elif args.config_action == "load":
            try:
                config = HotspotConfig.from_file(args.file)
                self.manager.current_config = config
                await self.manager.save_config()

                self.output.console.print(
                    f"[green]✓[/green] Configuration loaded from {args.file}"
                )
            except Exception as e:
                raise CLIError(f"Failed to load configuration: {e}") from e

        elif args.config_action == "validate":
            try:
                config = HotspotConfig.from_file(args.file)
                self.output.console.print(
                    f"[green]✓[/green] Configuration file {args.file} is valid"
                )

                # Show configuration details
                config_table = Table(title="Configuration Details")
                config_table.add_column("Setting", style="cyan")
                config_table.add_column("Value", style="green")

                for key, value in config.to_dict().items():
                    config_table.add_row(key.replace("_", " ").title(), str(value))

                self.output.console.print(config_table)

            except Exception as e:
                raise CLIError(f"Invalid configuration file: {e}") from e

    async def handle_interfaces(self, args: argparse.Namespace) -> None:
        """Handle the 'interfaces' command."""
        assert self.manager is not None

        interfaces = await self.manager.get_available_interfaces()

        if args.json:
            interface_data = [
                {
                    "name": iface.name,
                    "type": iface.type,
                    "state": iface.state,
                    "driver": iface.driver,
                    "is_wifi": iface.is_wifi,
                    "is_available": iface.is_available,
                }
                for iface in interfaces
            ]
            print(json.dumps(interface_data, indent=2))
        else:
            interface_dicts = [
                {
                    "name": iface.name,
                    "type": iface.type,
                    "state": iface.state,
                    "driver": iface.driver,
                    "is_available": iface.is_available,
                }
                for iface in interfaces
            ]
            self.output.print_interfaces(interface_dicts)

    def _extract_config_updates(self, args: argparse.Namespace) -> Dict[str, Any]:
        """Extract configuration updates from parsed arguments."""
        updates = {}

        # Map CLI arguments to config fields
        arg_mapping = {
            "name": "name",
            "password": "password",
            "interface": "interface",
            "auth": "authentication",
            "authentication": "authentication",
            "enc": "encryption",
            "encryption": "encryption",
            "band": "band",
            "channel": "channel",
            "max_clients": "max_clients",
            "hidden": "hidden",
        }

        for arg_name, config_name in arg_mapping.items():
            if hasattr(args, arg_name):
                value = getattr(args, arg_name)
                if value is not None:
                    updates[config_name] = value

        return updates


def main() -> None:
    """Main entry point for the CLI application."""
    cli = HotspotCLI()

    try:
        exit_code = asyncio.run(cli.run())
        sys.exit(exit_code)
    except KeyboardInterrupt:
        logger.info("Operation cancelled")
        sys.exit(130)
    except Exception as e:
        logger.critical(f"Critical error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
