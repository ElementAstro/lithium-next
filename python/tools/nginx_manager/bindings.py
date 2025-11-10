#!/usr/bin/env python3
"""
PyBind11 bindings for Nginx Manager.
"""

import json
from pathlib import Path
from loguru import logger

from .manager import NginxManager


class NginxManagerBindings:
    """
    Class providing bindings for pybind11 integration.

    This class wraps NginxManager functionality to be called from C++ via pybind11.
    """

    def __init__(self):
        """Initialize with a new NginxManager instance."""
        self.manager = NginxManager(use_colors=False)
        logger.debug("PyBind11 bindings initialized")

    def is_installed(self) -> bool:
        """Check if Nginx is installed."""
        return self.manager.is_nginx_installed()

    def install(self) -> bool:
        """Install Nginx if not already installed."""
        try:
            self.manager.install_nginx()
            return True
        except Exception as e:
            logger.error(f"Installation failed: {str(e)}")
            return False

    def start(self) -> bool:
        """Start Nginx server."""
        try:
            self.manager.start_nginx()
            return True
        except Exception as e:
            logger.error(f"Start failed: {str(e)}")
            return False

    def stop(self) -> bool:
        """Stop Nginx server."""
        try:
            self.manager.stop_nginx()
            return True
        except Exception as e:
            logger.error(f"Stop failed: {str(e)}")
            return False

    def reload(self) -> bool:
        """Reload Nginx configuration."""
        try:
            self.manager.reload_nginx()
            return True
        except Exception as e:
            logger.error(f"Reload failed: {str(e)}")
            return False

    def restart(self) -> bool:
        """Restart Nginx server."""
        try:
            self.manager.restart_nginx()
            return True
        except Exception as e:
            logger.error(f"Restart failed: {str(e)}")
            return False

    def check_config(self) -> bool:
        """Check Nginx configuration syntax."""
        try:
            return self.manager.check_config()
        except Exception as e:
            logger.error(f"Config check failed: {str(e)}")
            return False

    def get_status(self) -> bool:
        """Check if Nginx is running."""
        return self.manager.get_status()

    def get_version(self) -> str:
        """Get Nginx version."""
        try:
            return self.manager.get_version()
        except Exception as e:
            logger.error(f"Failed to get version: {str(e)}")
            return ""

    def backup_config(self, custom_name: str = "") -> str:
        """Backup Nginx configuration."""
        try:
            backup_path = self.manager.backup_config(
                custom_name=custom_name if custom_name else None
            )
            return str(backup_path)
        except Exception as e:
            logger.error(f"Backup failed: {str(e)}")
            return ""

    def restore_config(self, backup_file: str = "") -> bool:
        """Restore Nginx configuration from backup."""
        try:
            self.manager.restore_config(
                backup_file=backup_file if backup_file else None
            )
            return True
        except Exception as e:
            logger.error(f"Restore failed: {str(e)}")
            return False

    def create_virtual_host(
        self,
        server_name: str,
        port: int = 80,
        root_dir: str = "",
        template: str = "basic",
    ) -> str:
        """Create a virtual host configuration."""
        try:
            config_path = self.manager.create_virtual_host(
                server_name=server_name,
                port=port,
                root_dir=root_dir if root_dir else None,
                template=template,
            )
            return str(config_path)
        except Exception as e:
            logger.error(f"Virtual host creation failed: {str(e)}")
            return ""

    def enable_virtual_host(self, server_name: str) -> bool:
        """Enable a virtual host."""
        try:
            self.manager.enable_virtual_host(server_name)
            return True
        except Exception as e:
            logger.error(f"Failed to enable virtual host: {str(e)}")
            return False

    def disable_virtual_host(self, server_name: str) -> bool:
        """Disable a virtual host."""
        try:
            self.manager.disable_virtual_host(server_name)
            return True
        except Exception as e:
            logger.error(f"Failed to disable virtual host: {str(e)}")
            return False

    def list_virtual_hosts(self) -> str:
        """List all virtual hosts and their status."""
        try:
            vhosts = self.manager.list_virtual_hosts()
            return json.dumps(vhosts)
        except Exception as e:
            logger.error(f"Failed to list virtual hosts: {str(e)}")
            return "{}"

    def health_check(self) -> str:
        """Perform a health check."""
        try:
            result = self.manager.health_check()
            return json.dumps(result)
        except Exception as e:
            logger.error(f"Health check failed: {str(e)}")
            return '{"error": "Health check failed"}'
