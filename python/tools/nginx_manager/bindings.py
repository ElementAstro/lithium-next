#!/usr/bin/env python3
"""
PyBind11 bindings for the asynchronous Nginx Manager.
"""

import asyncio
import json
from pathlib import Path
from typing import Any, Awaitable, TypeVar, Coroutine

from loguru import logger

from .manager import (
    NginxManager,
    basic_template,
    php_template,
    proxy_template,
)
from .core import NginxError

T = TypeVar("T")


class NginxManagerBindings:
    """
    Class providing synchronous bindings for the async NginxManager,
    suitable for pybind11 integration.
    """

    def __init__(self):
        """Initialize with a new NginxManager instance and a running event loop."""
        self.manager = NginxManager(use_colors=False)
        self.manager.register_plugin(
            "vhost_templates",
            {"basic": basic_template, "php": php_template, "proxy": proxy_template},
        )
        logger.debug("PyBind11 bindings initialized.")

    def _run_sync(self, coro: Coroutine[Any, Any, T]) -> T:
        """
        Run an awaitable coroutine synchronously.
        This is a blocking call that will run the asyncio event loop until the future is done.
        """
        try:
            return asyncio.run(coro)
        except NginxError as e:
            logger.error(f"An Nginx operation failed: {e}")
            # Re-raise the exception to allow C++ to catch it if needed
            raise
        except Exception as e:
            logger.error(
                f"An unexpected error occurred in async execution: {e}")
            raise

    def is_installed(self) -> bool:
        """Check if Nginx is installed."""
        return self._run_sync(self.manager.is_nginx_installed())

    def install(self) -> bool:
        """Install Nginx if not already installed."""
        self._run_sync(self.manager.install_nginx())
        return True

    def start(self) -> bool:
        """Start Nginx server."""
        self._run_sync(self.manager.manage_service("start"))
        return True

    def stop(self) -> bool:
        """Stop Nginx server."""
        self._run_sync(self.manager.manage_service("stop"))
        return True

    def reload(self) -> bool:
        """Reload Nginx configuration."""
        self._run_sync(self.manager.manage_service("reload"))
        return True

    def restart(self) -> bool:
        """Restart Nginx server."""
        self._run_sync(self.manager.manage_service("restart"))
        return True

    def check_config(self) -> bool:
        """Check Nginx configuration syntax."""
        return self._run_sync(self.manager.check_config())

    def get_status(self) -> bool:
        """Check if Nginx is running."""
        return self._run_sync(self.manager.get_status())

    def get_version(self) -> str:
        """Get Nginx version."""
        return self._run_sync(self.manager.get_version())

    def backup_config(self, custom_name: str = "") -> str:
        """Backup Nginx configuration."""
        backup_path = self._run_sync(
            self.manager.backup_config(custom_name=custom_name or None)
        )
        return str(backup_path)

    def restore_config(self, backup_file: str = "") -> bool:
        """Restore Nginx configuration from backup."""
        self._run_sync(
            self.manager.restore_config(backup_file=backup_file or None)
        )
        return True

    def create_virtual_host(
        self, server_name: str, port: int = 80, root_dir: str = "", template: str = "basic"
    ) -> str:
        """Create a virtual host configuration."""
        config_path = self._run_sync(
            self.manager.manage_virtual_host(
                "create",
                server_name,
                port=port,
                root_dir=root_dir or None,
                template=template,
            )
        )
        return str(config_path)

    def enable_virtual_host(self, server_name: str) -> bool:
        """Enable a virtual host."""
        self._run_sync(self.manager.manage_virtual_host("enable", server_name))
        return True

    def disable_virtual_host(self, server_name: str) -> bool:
        """Disable a virtual host."""
        self._run_sync(self.manager.manage_virtual_host(
            "disable", server_name))
        return True

    def list_virtual_hosts(self) -> str:
        """List all virtual hosts and their status as a JSON string."""
        vhosts = self.manager.list_virtual_hosts()  # This is synchronous
        return json.dumps(vhosts)

    def health_check(self) -> str:
        """Perform a health check and return results as a JSON string."""
        result = self._run_sync(self.manager.health_check())
        return json.dumps(result)
