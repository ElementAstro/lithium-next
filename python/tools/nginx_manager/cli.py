#!/usr/bin/env python3
"""
Asynchronous command-line interface for Nginx Manager.
"""

import argparse
import asyncio
import sys
from pathlib import Path
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

        subparsers = parser.add_subparsers(dest="command", help="Commands", required=True)

        # Service management commands
        for action in ["install", "start", "stop", "reload", "restart", "status", "version", "check", "health"]:
            subparsers.add_parser(action, help=f"{action.capitalize()} Nginx.")

        # Backup and Restore
        backup_parser = subparsers.add_parser("backup", help="Backup Nginx configuration.")
        backup_parser.add_argument("--name", help="Custom name for the backup file.")
        subparsers.add_parser("list-backups", help="List available backups.")
        restore_parser = subparsers.add_parser("restore", help="Restore Nginx configuration.")
        restore_parser.add_argument("--backup", help="Path to the backup file to restore.")

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
            parser = vhost_sp.add_parser(action, help=f"{action.capitalize()} a virtual host.")
            parser.add_argument("server_name", help="The server name of the vhost.")

        vhost_sp.add_parser("list", help="List all virtual hosts.")

        return parser

    async def run(self) -> int:
        """
        Parse arguments and execute the requested async command.
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

            if cmd in ["start", "stop", "reload", "restart"]:
                await self.manager.manage_service(cmd)
            elif cmd == "install":
                await self.manager.install_nginx()
            elif cmd == "status":
                await self.manager.get_status()
            elif cmd == "version":
                await self.manager.get_version()
            elif cmd == "check":
                await self.manager.check_config()
            elif cmd == "health":
                await self.manager.health_check()
            elif cmd == "backup":
                await self.manager.backup_config(custom_name=args.name)
            elif cmd == "list-backups":
                for backup in self.manager.list_backups():
                    print(backup)
            elif cmd == "restore":
                await self.manager.restore_config(backup_file=args.backup)
            elif cmd == "vhost":
                await self.handle_vhost_command(args)

            logger.debug("Command executed successfully.")
            return 0
        except NginxError as e:
            logger.error(f"An error occurred: {e}")
            print(f"Error: {e}", file=sys.stderr)
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
    Main entry point for the asynchronous CLI.
    """
    try:
        return asyncio.run(NginxManagerCLI().run())
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.", file=sys.stderr)
        return 130  # Standard exit code for Ctrl+C
    except NginxError as e:
        # Catch exceptions that might be raised during initialization
        print(f"Critical Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())