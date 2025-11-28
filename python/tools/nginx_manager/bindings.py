#!/usr/bin/env python3
"""
PyBind11 bindings for Nginx Manager.

This module provides two binding approaches:
1. NginxPyBindAdapter - Static methods for C++ pybind11 integration (recommended)
2. NginxManagerBindings - Instance-based bindings for backward compatibility
"""

import json
import platform
from pathlib import Path
from typing import Dict, List, Any
from loguru import logger

from .manager import NginxManager
from .core import OperatingSystem


class NginxPyBindAdapter:
    """
    PyBind11 adapter class for C++ integration.

    Provides static methods for C++ binding via pybind11. Returns dict/list types
    for pybind11 compatibility and includes graceful degradation for unsupported
    platforms (Windows).
    """

    @staticmethod
    def is_platform_supported() -> bool:
        """
        Check if the current platform is supported.

        Returns:
            True for Linux/macOS, False for Windows
        """
        system = platform.system().lower()
        return system != 'windows'

    @staticmethod
    def _handle_unsupported_platform() -> Dict[str, Any]:
        """
        Return a structured error response for unsupported platforms.

        Returns:
            Dictionary with error information
        """
        return {
            "success": False,
            "error": "Nginx management is primarily supported on Linux and macOS",
            "platform": platform.system(),
            "message": "Windows support is limited. Please install Nginx manually."
        }

    @staticmethod
    def is_installed() -> Dict[str, bool]:
        """
        Check if Nginx is installed.

        Returns:
            Dictionary with 'installed' and 'success' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            return {
                "installed": manager.is_nginx_installed(),
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: Error checking installation: {str(e)}")
            return {
                "installed": False,
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def install() -> Dict[str, Any]:
        """
        Install Nginx.

        Returns:
            Dictionary with 'success' and optional 'error' keys
        """
        if not NginxPyBindAdapter.is_platform_supported():
            return NginxPyBindAdapter._handle_unsupported_platform()

        try:
            manager = NginxManager(use_colors=False)
            manager.install_nginx()
            logger.info("C++ binding: Nginx installed successfully")
            return {"success": True}
        except Exception as e:
            logger.error(f"C++ binding: Installation failed: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def start() -> Dict[str, Any]:
        """
        Start Nginx server.

        Returns:
            Dictionary with 'success' and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            manager.start_nginx()
            logger.info("C++ binding: Nginx started successfully")
            return {"success": True}
        except Exception as e:
            logger.error(f"C++ binding: Start failed: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def stop() -> Dict[str, Any]:
        """
        Stop Nginx server.

        Returns:
            Dictionary with 'success' and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            manager.stop_nginx()
            logger.info("C++ binding: Nginx stopped successfully")
            return {"success": True}
        except Exception as e:
            logger.error(f"C++ binding: Stop failed: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def reload() -> Dict[str, Any]:
        """
        Reload Nginx configuration.

        Returns:
            Dictionary with 'success' and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            manager.reload_nginx()
            logger.info("C++ binding: Nginx configuration reloaded")
            return {"success": True}
        except Exception as e:
            logger.error(f"C++ binding: Reload failed: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def restart() -> Dict[str, Any]:
        """
        Restart Nginx server.

        Returns:
            Dictionary with 'success' and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            manager.restart_nginx()
            logger.info("C++ binding: Nginx restarted successfully")
            return {"success": True}
        except Exception as e:
            logger.error(f"C++ binding: Restart failed: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def check_config() -> Dict[str, Any]:
        """
        Check Nginx configuration syntax.

        Returns:
            Dictionary with 'valid' boolean and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            valid = manager.check_config()
            logger.info(f"C++ binding: Config check result: {valid}")
            return {
                "valid": valid,
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: Config check failed: {str(e)}")
            return {
                "valid": False,
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def get_status() -> Dict[str, Any]:
        """
        Check if Nginx is running.

        Returns:
            Dictionary with 'running' boolean and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            running = manager.get_status()
            logger.info(f"C++ binding: Nginx status: {running}")
            return {
                "running": running,
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: Status check failed: {str(e)}")
            return {
                "running": False,
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def get_version() -> Dict[str, Any]:
        """
        Get Nginx version.

        Returns:
            Dictionary with 'version' string and 'success' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            version = manager.get_version()
            logger.info(f"C++ binding: Nginx version: {version}")
            return {
                "version": version,
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: Failed to get version: {str(e)}")
            return {
                "version": "",
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def backup_config(custom_name: str = "") -> Dict[str, Any]:
        """
        Backup Nginx configuration.

        Args:
            custom_name: Optional custom name for the backup file

        Returns:
            Dictionary with 'path' string and 'success' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            backup_path = manager.backup_config(
                custom_name=custom_name if custom_name else None
            )
            logger.info(f"C++ binding: Configuration backed up to {backup_path}")
            return {
                "path": str(backup_path),
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: Backup failed: {str(e)}")
            return {
                "path": "",
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def restore_config(backup_file: str = "") -> Dict[str, Any]:
        """
        Restore Nginx configuration from backup.

        Args:
            backup_file: Path to the backup file to restore

        Returns:
            Dictionary with 'success' boolean and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            manager.restore_config(
                backup_file=backup_file if backup_file else None
            )
            logger.info(f"C++ binding: Configuration restored from {backup_file}")
            return {"success": True}
        except Exception as e:
            logger.error(f"C++ binding: Restore failed: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def list_backups() -> Dict[str, Any]:
        """
        List all available configuration backups.

        Returns:
            Dictionary with 'backups' list and 'success' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            backups = manager.list_backups()
            backup_list = [str(b) for b in backups]
            logger.info(f"C++ binding: Found {len(backup_list)} backup(s)")
            return {
                "backups": backup_list,
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: Failed to list backups: {str(e)}")
            return {
                "backups": [],
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def create_virtual_host(server_name: str, port: int = 80,
                           root_dir: str = "", template: str = "basic") -> Dict[str, Any]:
        """
        Create a virtual host configuration.

        Args:
            server_name: Server name (domain)
            port: Port number
            root_dir: Document root directory
            template: Template to use ('basic', 'php', 'proxy')

        Returns:
            Dictionary with 'path' string and 'success' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            config_path = manager.create_virtual_host(
                server_name=server_name,
                port=port,
                root_dir=root_dir if root_dir else None,
                template=template
            )
            logger.info(f"C++ binding: Virtual host created at {config_path}")
            return {
                "path": str(config_path),
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: Virtual host creation failed: {str(e)}")
            return {
                "path": "",
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def enable_virtual_host(server_name: str) -> Dict[str, Any]:
        """
        Enable a virtual host.

        Args:
            server_name: Name of the server configuration

        Returns:
            Dictionary with 'success' boolean and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            manager.enable_virtual_host(server_name)
            logger.info(f"C++ binding: Virtual host {server_name} enabled")
            return {"success": True}
        except Exception as e:
            logger.error(f"C++ binding: Failed to enable virtual host: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def disable_virtual_host(server_name: str) -> Dict[str, Any]:
        """
        Disable a virtual host.

        Args:
            server_name: Name of the server configuration

        Returns:
            Dictionary with 'success' boolean and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            manager.disable_virtual_host(server_name)
            logger.info(f"C++ binding: Virtual host {server_name} disabled")
            return {"success": True}
        except Exception as e:
            logger.error(f"C++ binding: Failed to disable virtual host: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def list_virtual_hosts() -> Dict[str, Any]:
        """
        List all virtual hosts and their status.

        Returns:
            Dictionary with 'virtual_hosts' dict and 'success' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            vhosts = manager.list_virtual_hosts()
            logger.info(f"C++ binding: Found {len(vhosts)} virtual host(s)")
            return {
                "virtual_hosts": vhosts,
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: Failed to list virtual hosts: {str(e)}")
            return {
                "virtual_hosts": {},
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def analyze_logs(domain: str = "", lines: int = 100, filter_pattern: str = "") -> Dict[str, Any]:
        """
        Analyze Nginx access logs.

        Args:
            domain: Specific domain to analyze logs for
            lines: Number of lines to analyze
            filter_pattern: Regex pattern to filter log entries

        Returns:
            Dictionary with 'entries' list and 'success' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            entries = manager.analyze_logs(
                domain=domain if domain else None,
                lines=lines,
                filter_pattern=filter_pattern if filter_pattern else None
            )
            logger.info(f"C++ binding: Analyzed {len(entries)} log entries")
            return {
                "entries": entries,
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: Failed to analyze logs: {str(e)}")
            return {
                "entries": [],
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def generate_ssl_cert(domain: str, email: str = "", use_letsencrypt: bool = True) -> Dict[str, Any]:
        """
        Generate SSL certificates for a domain.

        Args:
            domain: Domain name
            email: Email address for Let's Encrypt
            use_letsencrypt: Whether to use Let's Encrypt (if False, uses self-signed)

        Returns:
            Dictionary with 'cert_path', 'key_path' strings and 'success' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            cert_path, key_path = manager.generate_ssl_cert(
                domain=domain,
                email=email if email else None,
                use_letsencrypt=use_letsencrypt
            )
            logger.info(f"C++ binding: SSL cert generated for {domain}")
            return {
                "cert_path": str(cert_path),
                "key_path": str(key_path),
                "success": True
            }
        except Exception as e:
            logger.error(f"C++ binding: SSL cert generation failed: {str(e)}")
            return {
                "cert_path": "",
                "key_path": "",
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def configure_ssl(domain: str, cert_path: str, key_path: str) -> Dict[str, Any]:
        """
        Configure SSL for a virtual host.

        Args:
            domain: Domain name
            cert_path: Path to the certificate file
            key_path: Path to the key file

        Returns:
            Dictionary with 'success' boolean and optional 'error' keys
        """
        try:
            manager = NginxManager(use_colors=False)
            manager.configure_ssl(
                domain=domain,
                cert_path=Path(cert_path),
                key_path=Path(key_path)
            )
            logger.info(f"C++ binding: SSL configured for {domain}")
            return {"success": True}
        except Exception as e:
            logger.error(f"C++ binding: SSL configuration failed: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def health_check() -> Dict[str, Any]:
        """
        Perform a comprehensive health check.

        Returns:
            Dictionary with health check results
        """
        try:
            manager = NginxManager(use_colors=False)
            result = manager.health_check()
            logger.info("C++ binding: Health check completed")
            return {
                "success": True,
                "health": result
            }
        except Exception as e:
            logger.error(f"C++ binding: Health check failed: {str(e)}")
            return {
                "success": False,
                "health": {},
                "error": str(e)
            }


class NginxManagerBindings:
    """
    Class providing bindings for pybind11 integration.

    This class wraps NginxManager functionality to be called from C++ via pybind11.
    Provides instance-based interface for backward compatibility.
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

    def create_virtual_host(self, server_name: str, port: int = 80,
                            root_dir: str = "", template: str = 'basic') -> str:
        """Create a virtual host configuration."""
        try:
            config_path = self.manager.create_virtual_host(
                server_name=server_name,
                port=port,
                root_dir=root_dir if root_dir else None,
                template=template
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
            return "{\"error\": \"Health check failed\"}"
