#!/usr/bin/env python3
"""
Asynchronous command-line interface for Nginx Manager.
"""

from __future__ import annotations

import argparse
import asyncio
import sys
from pathlib import Path
from typing import NoReturn

from loguru import logger

from .manager import (
    NginxManager,
    basic_template,
    php_template,
    proxy_template,
)
from .core import NginxError


class NginxManagerCLI:
    """
    Asynchronous command-line interface for the NginxManager.
    """

    def __init__(self):
        """Initialize the CLI with a NginxManager instance and register plugins."""
        self.manager = NginxManager()
        self.manager.register_plugin(
            "vhost_templates",
            {"basic": basic_template, "php": php_template, "proxy": proxy_template},
        )
        logger.debug("Async CLI interface initialized with default plugins.")

    def setup_parser(self) -> argparse.ArgumentParser:
        """
        Set up the argument parser.
        """
        parser = argparse.ArgumentParser(
            description="Nginx Manager - An async tool for managing Nginx.",
            formatter_class=argparse.RawDescriptionHelpFormatter,
        )
        parser.add_argument(
            "-v", "--verbose", action="store_true", help="Enable verbose output."
        )

        subparsers = parser.add_subparsers(
            dest="command", help="Commands", required=True
        )

        # Service management commands
        for action in [
            "install",
            "start",
            "stop",
            "reload",
            "restart",
            "status",
            "version",
            "check",
            "health",
        ]:
            subparsers.add_parser(action, help=f"{action.capitalize()} Nginx.")

        # Backup and Restore
        backup_parser = subparsers.add_parser(
            "backup", help="Backup Nginx configuration."
        )
        backup_parser.add_argument("--name", help="Custom name for the backup file.")
        subparsers.add_parser("list-backups", help="List available backups.")
        restore_parser = subparsers.add_parser(
            "restore", help="Restore Nginx configuration."
        )
        restore_parser.add_argument(
            "--backup", help="Path to the backup file to restore."
        )

        # Virtual Host Management
        vhost_parser = subparsers.add_parser("vhost", help="Manage virtual hosts.")
        vhost_sp = vhost_parser.add_subparsers(dest="vhost_command", required=True)

        vhost_create = vhost_sp.add_parser("create", help="Create a virtual host.")
        vhost_create.add_argument("server_name", help="Server name (e.g., example.com)")
        vhost_create.add_argument("--port", type=int, default=80, help="Port number.")
        vhost_create.add_argument("--root", help="Document root directory.")
        vhost_create.add_argument(
            "--template",
            default="basic",
            choices=self.manager.plugins.get("vhost_templates", {}).keys(),
            help="Template to use for the vhost.",
        )

        for action in ["enable", "disable"]:
            parser = vhost_sp.add_parser(
                action, help=f"{action.capitalize()} a virtual host."
            )
            parser.add_argument("server_name", help="The server name of the vhost.")

        vhost_sp.add_parser("list", help="List all virtual hosts.")

        return parser

    async def run(self) -> int:
        """
        Parse arguments and execute the requested async command with enhanced error handling.
        """
        parser = self.setup_parser()
        args = parser.parse_args()

        if args.verbose:
            logger.remove()
            logger.add(sys.stderr, level="DEBUG")
            logger.debug("Verbose logging enabled.")

        try:
            logger.debug(f"Executing command: {args.command}")
            cmd = args.command

            match cmd:
                case "start" | "stop" | "reload" | "restart":
                    await self.manager.manage_service(cmd)
                case "install":
                    await self.manager.install_nginx()
                case "status":
                    await self.manager.get_status()
                case "version":
                    await self.manager.get_version()
                case "check":
                    await self.manager.check_config()
                case "health":
                    await self.manager.health_check()
                case "backup":
                    await self.manager.backup_config(custom_name=args.name)
                case "list-backups":
                    backups = self.manager.list_backups()
                    if backups:
                        print("Available backups:")
                        for backup in backups:
                            print(f"  - {backup.name} ({backup.stat().st_mtime})")
                    else:
                        print("No backups found.")
                case "restore":
                    await self.manager.restore_config(backup_file=args.backup)
                case "vhost":
                    await self.handle_vhost_command(args)
                case _:
                    logger.error(f"Unknown command: {cmd}")
                    return 1

            logger.debug("Command executed successfully.")
            return 0

        except NginxError as e:
            logger.error(f"Nginx operation failed: {e}")
            print(f"Error: {e}", file=sys.stderr)
            return 1
        except KeyboardInterrupt:
            logger.info("Operation cancelled by user")
            print("\nOperation cancelled by user.", file=sys.stderr)
            return 130
        except Exception as e:
            logger.error(f"Unexpected error: {e}")
            print(f"Unexpected error: {e}", file=sys.stderr)
            return 1

    async def handle_vhost_command(self, args: argparse.Namespace) -> None:
        """
        Handle virtual host subcommands.
        """
        cmd = args.vhost_command
        if cmd == "list":
            vhosts = self.manager.list_virtual_hosts()
            for host, enabled in vhosts.items():
                status = "enabled" if enabled else "disabled"
                print(f"- {host}: {status}")
        else:
            await self.manager.manage_virtual_host(
                cmd,
                args.server_name,
                port=getattr(args, "port", 80),
                root_dir=getattr(args, "root", None),
                template=getattr(args, "template", "basic"),
            )


def main() -> int:
    """
    Main entry point for the asynchronous CLI with enhanced error handling.
    """
    try:
        return asyncio.run(NginxManagerCLI().run())
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.", file=sys.stderr)
        return 130  # Standard exit code for Ctrl+C
    except NginxError as e:
        # Catch exceptions that might be raised during initialization
        print(f"Critical Error: {e}", file=sys.stderr)
        logger.error(f"Critical initialization error: {e}")
        return 1
    except Exception as e:
        print(f"Unexpected critical error: {e}", file=sys.stderr)
        logger.error(f"Unexpected critical error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
