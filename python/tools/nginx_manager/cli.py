#!/usr/bin/env python3
"""
Command-line interface for Nginx Manager.
"""

import argparse
from pathlib import Path
import sys
from loguru import logger

from .manager import NginxManager


class NginxManagerCLI:
    """
    Command-line interface for the NginxManager.

    This class provides a command-line interface to interact with
    the NginxManager class using argparse.
    """

    def __init__(self):
        """Initialize the CLI with a NginxManager instance."""
        self.manager = NginxManager()
        logger.debug("CLI interface initialized")

    def setup_parser(self) -> argparse.ArgumentParser:
        """
        Set up the argument parser.

        Returns:
            Configured argument parser
        """
        parser = argparse.ArgumentParser(
            description="Nginx Manager - A tool for managing Nginx web server",
            formatter_class=argparse.RawDescriptionHelpFormatter
        )

        subparsers = parser.add_subparsers(dest="command", help="Commands")

        # Basic commands
        subparsers.add_parser("install", help="Install Nginx")
        subparsers.add_parser("start", help="Start Nginx")
        subparsers.add_parser("stop", help="Stop Nginx")
        subparsers.add_parser("reload", help="Reload Nginx configuration")
        subparsers.add_parser("restart", help="Restart Nginx")
        subparsers.add_parser("check", help="Check Nginx configuration syntax")
        subparsers.add_parser("status", help="Show Nginx status")
        subparsers.add_parser("version", help="Show Nginx version")

        # Backup commands
        backup_parser = subparsers.add_parser(
            "backup", help="Backup Nginx configuration")
        backup_parser.add_argument(
            "--name", help="Custom name for the backup file")

        subparsers.add_parser(
            "list-backups", help="List available configuration backups")

        restore_parser = subparsers.add_parser(
            "restore", help="Restore Nginx configuration")
        restore_parser.add_argument(
            "--backup", help="Path to the backup file to restore")

        # Virtual host commands
        vhost_parser = subparsers.add_parser(
            "vhost", help="Virtual host management")
        vhost_subparsers = vhost_parser.add_subparsers(dest="vhost_command")

        create_vhost_parser = vhost_subparsers.add_parser(
            "create", help="Create a virtual host")
        create_vhost_parser.add_argument("server_name", help="Server name")
        create_vhost_parser.add_argument(
            "--port", type=int, default=80, help="Port number")
        create_vhost_parser.add_argument(
            "--root", help="Document root directory")
        create_vhost_parser.add_argument("--template", default="basic",
                                         choices=["basic", "php", "proxy"],
                                         help="Template to use")

        enable_vhost_parser = vhost_subparsers.add_parser(
            "enable", help="Enable a virtual host")
        enable_vhost_parser.add_argument("server_name", help="Server name")

        disable_vhost_parser = vhost_subparsers.add_parser(
            "disable", help="Disable a virtual host")
        disable_vhost_parser.add_argument("server_name", help="Server name")

        vhost_subparsers.add_parser("list", help="List virtual hosts")

        # SSL commands
        ssl_parser = subparsers.add_parser("ssl", help="SSL management")
        ssl_subparsers = ssl_parser.add_subparsers(dest="ssl_command")

        generate_ssl_parser = ssl_subparsers.add_parser(
            "generate", help="Generate SSL certificate")
        generate_ssl_parser.add_argument("domain", help="Domain name")
        generate_ssl_parser.add_argument(
            "--email", help="Email address for Let's Encrypt")
        generate_ssl_parser.add_argument("--self-signed", action="store_true",
                                         help="Generate self-signed certificate")

        configure_ssl_parser = ssl_subparsers.add_parser(
            "configure", help="Configure SSL for a domain")
        configure_ssl_parser.add_argument("domain", help="Domain name")
        configure_ssl_parser.add_argument(
            "--cert", required=True, help="Path to certificate file")
        configure_ssl_parser.add_argument(
            "--key", required=True, help="Path to key file")

        # Log analysis
        logs_parser = subparsers.add_parser("logs", help="Log analysis")
        logs_parser.add_argument("--domain", help="Domain to analyze logs for")
        logs_parser.add_argument("--lines", type=int, default=100,
                                 help="Number of lines to analyze")
        logs_parser.add_argument(
            "--filter", help="Filter pattern for log entries")

        # Health check
        subparsers.add_parser("health", help="Perform a health check")

        # Add verbose option to all commands
        parser.add_argument("--verbose", "-v", action="store_true",
                            help="Enable verbose output")

        return parser

    def run(self) -> int:
        """
        Parse arguments and execute the requested command.

        Returns:
            Exit code (0 for success, non-zero for failure)
        """
        parser = self.setup_parser()
        args = parser.parse_args()

        # Set verbose logging if requested
        if getattr(args, "verbose", False):
            logger.remove()
            logger.add(sys.stderr, level="DEBUG")
            logger.debug("Verbose logging enabled")

        if not args.command:
            parser.print_help()
            return 1

        try:
            logger.debug(f"Executing command: {args.command}")
            match args.command:
                case "install":
                    self.manager.install_nginx()

                case "start":
                    self.manager.start_nginx()

                case "stop":
                    self.manager.stop_nginx()

                case "reload":
                    self.manager.reload_nginx()

                case "restart":
                    self.manager.restart_nginx()

                case "check":
                    self.manager.check_config()

                case "status":
                    self.manager.get_status()

                case "version":
                    self.manager.get_version()

                case "backup":
                    self.manager.backup_config(custom_name=args.name)

                case "list-backups":
                    self.manager.list_backups()

                case "restore":
                    self.manager.restore_config(backup_file=args.backup)

                case "vhost":
                    if not args.vhost_command:
                        parser.error("No virtual host command specified")
                        return 1

                    match args.vhost_command:
                        case "create":
                            self.manager.create_virtual_host(
                                server_name=args.server_name,
                                port=args.port,
                                root_dir=args.root,
                                template=args.template
                            )

                        case "enable":
                            self.manager.enable_virtual_host(args.server_name)

                        case "disable":
                            self.manager.disable_virtual_host(args.server_name)

                        case "list":
                            self.manager.list_virtual_hosts()

                case "ssl":
                    if not args.ssl_command:
                        parser.error("No SSL command specified")
                        return 1

                    match args.ssl_command:
                        case "generate":
                            self.manager.generate_ssl_cert(
                                domain=args.domain,
                                email=args.email,
                                use_letsencrypt=not args.self_signed
                            )

                        case "configure":
                            self.manager.configure_ssl(
                                domain=args.domain,
                                cert_path=Path(args.cert),
                                key_path=Path(args.key)
                            )

                case "logs":
                    self.manager.analyze_logs(
                        domain=args.domain,
                        lines=args.lines,
                        filter_pattern=args.filter
                    )

                case "health":
                    self.manager.health_check()

            logger.debug("Command executed successfully")
            return 0

        except Exception as e:
            logger.exception(f"Error executing command: {str(e)}")
            print(f"Error: {str(e)}")
            return 1
