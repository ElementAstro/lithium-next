#!/usr/bin/env python3
"""
Nginx Manager - A comprehensive tool for managing Nginx web server

This module provides functionality for managing Nginx installations, including:
- Installation and service management
- Configuration handling and validation
- Virtual host management
- SSL certificate management
- Log analysis and monitoring

The module supports both command-line usage and embedding via pybind11.
"""

import argparse
import datetime
import logging
import os
import platform
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Union, Any, Callable
import json
from functools import lru_cache


class OperatingSystem(Enum):
    """Enum representing supported operating systems."""
    LINUX = "linux"
    WINDOWS = "windows"
    MACOS = "darwin"
    UNKNOWN = "unknown"


class NginxError(Exception):
    """Base exception class for all Nginx-related errors."""
    pass


class ConfigError(NginxError):
    """Exception raised for Nginx configuration errors."""
    pass


class InstallationError(NginxError):
    """Exception raised for Nginx installation errors."""
    pass


class OperationError(NginxError):
    """Exception raised for failed Nginx operations."""
    pass


@dataclass
class NginxPaths:
    """Class holding paths related to Nginx installation."""
    base_path: Path
    conf_path: Path
    binary_path: Path
    backup_path: Path
    sites_available: Path
    sites_enabled: Path
    logs_path: Path
    ssl_path: Path


class OutputColors:
    """ANSI color codes for terminal output."""
    GREEN = '\033[0;32m'
    RED = '\033[0;31m'
    YELLOW = '\033[0;33m'
    BLUE = '\033[0;34m'
    MAGENTA = '\033[0;35m'
    CYAN = '\033[0;36m'
    RESET = '\033[0m'

    @staticmethod
    def is_color_supported() -> bool:
        """Check if the current terminal supports colors."""
        return platform.system() != "Windows" or "TERM" in os.environ


class NginxManager:
    """
    Main class for managing Nginx operations.

    This class provides methods to install, configure, and manage Nginx web server.
    It supports operations on different operating systems and can be used both from
    command line or as an imported library.
    """

    def __init__(self, use_colors: bool = True, log_level: int = logging.INFO):
        """
        Initialize the NginxManager.

        Args:
            use_colors: Whether to use colored output in terminal
            log_level: Logging level (from the logging module)
        """
        self._setup_logging(log_level)
        self.os = self._detect_os()
        self.paths = self._setup_paths()
        self.use_colors = use_colors and OutputColors.is_color_supported()

    def _setup_logging(self, log_level: int) -> None:
        """
        Set up the logging configuration.

        Args:
            log_level: The logging level to use
        """
        self.logger = logging.getLogger("nginx_manager")
        handler = logging.StreamHandler()
        formatter = logging.Formatter(
            '%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        handler.setFormatter(formatter)
        self.logger.addHandler(handler)
        self.logger.setLevel(log_level)

    def _detect_os(self) -> OperatingSystem:
        """
        Detect the current operating system.

        Returns:
            The detected operating system enum value
        """
        system = platform.system().lower()
        try:
            return next(os_type for os_type in OperatingSystem
                        if os_type.value == system)
        except StopIteration:
            return OperatingSystem.UNKNOWN

    def _setup_paths(self) -> NginxPaths:
        """
        Set up the path configuration based on the detected OS.

        Returns:
            Object containing all relevant Nginx paths
        """
        match self.os:
            case OperatingSystem.LINUX:
                base_path = Path("/etc/nginx")
                binary_path = Path("/usr/sbin/nginx")
                logs_path = Path("/var/log/nginx")

            case OperatingSystem.WINDOWS:
                base_path = Path("C:/nginx")
                binary_path = base_path / "nginx.exe"
                logs_path = base_path / "logs"

            case OperatingSystem.MACOS:
                base_path = Path("/usr/local/etc/nginx")
                binary_path = Path("/usr/local/bin/nginx")
                logs_path = Path("/usr/local/var/log/nginx")

            case _:
                # Default to Linux paths if OS is unknown
                self.logger.warning(
                    "Unknown OS detected, defaulting to Linux paths")
                base_path = Path("/etc/nginx")
                binary_path = Path("/usr/sbin/nginx")
                logs_path = Path("/var/log/nginx")

        conf_path = base_path / "nginx.conf"
        backup_path = base_path / "backup"
        sites_available = base_path / "sites-available"
        sites_enabled = base_path / "sites-enabled"
        ssl_path = base_path / "ssl"

        return NginxPaths(
            base_path=base_path,
            conf_path=conf_path,
            binary_path=binary_path,
            backup_path=backup_path,
            sites_available=sites_available,
            sites_enabled=sites_enabled,
            logs_path=logs_path,
            ssl_path=ssl_path
        )

    def _print_color(self, message: str, color: str = OutputColors.RESET) -> None:
        """
        Print a message with color if color output is enabled.

        Args:
            message: The message to print
            color: The ANSI color code to use
        """
        if self.use_colors:
            print(f"{color}{message}{OutputColors.RESET}")
        else:
            print(message)

    def _run_command(self, cmd: Union[List[str], str], check: bool = True, **kwargs) -> subprocess.CompletedProcess:
        """
        Run a shell command with proper error handling.

        Args:
            cmd: Command to run (list or string)
            check: Whether to raise an exception if the command fails
            **kwargs: Additional arguments to pass to subprocess.run

        Returns:
            The result of the command

        Raises:
            OperationError: If the command fails and check is True
        """
        try:
            self.logger.debug(f"Running command: {cmd}")
            return subprocess.run(
                cmd,
                check=check,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,  # Python 3.7+ text parameter instead of universal_newlines
                **kwargs
            )
        except subprocess.CalledProcessError as e:
            error_msg = f"Command '{cmd}' failed with error: {e.stderr.strip() if e.stderr else str(e)}"
            self.logger.error(error_msg)
            if check:
                raise OperationError(error_msg) from e
            return subprocess.CompletedProcess(e.cmd, e.returncode, e.stdout, e.stderr)

    @lru_cache(maxsize=1)
    def is_nginx_installed(self) -> bool:
        """
        Check if Nginx is installed.

        Returns:
            True if Nginx is installed, False otherwise
        """
        try:
            result = self._run_command(
                [str(self.paths.binary_path), "-v"], check=False)
            return result.returncode == 0
        except FileNotFoundError:
            return False

    def install_nginx(self) -> None:
        """
        Install Nginx if not already installed.

        Raises:
            InstallationError: If installation fails or platform is unsupported
        """
        if self.is_nginx_installed():
            self.logger.info("Nginx is already installed")
            return

        self.logger.info("Installing Nginx...")

        try:
            match self.os:
                case OperatingSystem.LINUX:
                    # Check Linux distribution
                    if Path("/etc/debian_version").exists():
                        self._run_command(
                            "sudo apt-get update && sudo apt-get install nginx -y", shell=True)
                    elif Path("/etc/redhat-release").exists():
                        self._run_command(
                            "sudo yum update && sudo yum install nginx -y", shell=True)
                    else:
                        raise InstallationError(
                            "Unsupported Linux distribution. Please install Nginx manually.")

                case OperatingSystem.WINDOWS:
                    self._print_color(
                        "Windows automatic installation not supported. Please install manually.", OutputColors.YELLOW)
                    raise InstallationError(
                        "Automatic installation on Windows is not supported.")

                case OperatingSystem.MACOS:
                    self._run_command(
                        "brew update && brew install nginx", shell=True)

                case _:
                    raise InstallationError(
                        "Unsupported platform. Please install Nginx manually.")

            self.logger.info("Nginx installed successfully")

        except Exception as e:
            raise InstallationError(
                f"Failed to install Nginx: {str(e)}") from e

    def start_nginx(self) -> None:
        """
        Start the Nginx server.

        Raises:
            OperationError: If Nginx fails to start
        """
        if not self.paths.binary_path.exists():
            raise OperationError("Nginx binary not found")

        self._run_command([str(self.paths.binary_path)])
        self._print_color("Nginx has been started", OutputColors.GREEN)
        self.logger.info("Nginx started")

    def stop_nginx(self) -> None:
        """
        Stop the Nginx server.

        Raises:
            OperationError: If Nginx fails to stop
        """
        if not self.paths.binary_path.exists():
            raise OperationError("Nginx binary not found")

        self._run_command([str(self.paths.binary_path), '-s', 'stop'])
        self._print_color("Nginx has been stopped", OutputColors.GREEN)
        self.logger.info("Nginx stopped")

    def reload_nginx(self) -> None:
        """
        Reload the Nginx configuration.

        Raises:
            OperationError: If Nginx fails to reload
        """
        if not self.paths.binary_path.exists():
            raise OperationError("Nginx binary not found")

        self._run_command([str(self.paths.binary_path), '-s', 'reload'])
        self._print_color(
            "Nginx configuration has been reloaded", OutputColors.GREEN)
        self.logger.info("Nginx configuration reloaded")

    def restart_nginx(self) -> None:
        """
        Restart the Nginx server.

        Raises:
            OperationError: If Nginx fails to restart
        """
        self.stop_nginx()
        self.start_nginx()
        self._print_color("Nginx has been restarted", OutputColors.GREEN)
        self.logger.info("Nginx restarted")

    def check_config(self) -> bool:
        """
        Check the syntax of the Nginx configuration files.

        Returns:
            True if the configuration is valid, False otherwise
        """
        if not self.paths.conf_path.exists():
            raise ConfigError("Nginx configuration file not found")

        try:
            self._run_command([str(self.paths.binary_path),
                              '-t', '-c', str(self.paths.conf_path)])
            self._print_color(
                "Nginx configuration syntax is correct", OutputColors.GREEN)
            self.logger.info("Nginx configuration syntax is correct")
            return True
        except OperationError:
            self._print_color(
                "Nginx configuration syntax is incorrect", OutputColors.RED)
            self.logger.error("Nginx configuration syntax is incorrect")
            return False

    def get_status(self) -> bool:
        """
        Check if Nginx is running.

        Returns:
            True if Nginx is running, False otherwise
        """
        try:
            match self.os:
                case OperatingSystem.WINDOWS:
                    result = self._run_command(
                        'tasklist | findstr nginx.exe', shell=True, check=False)
                case _:
                    result = self._run_command(
                        'pgrep nginx', shell=True, check=False)

            is_running = result.returncode == 0 and result.stdout.strip() != ""

            if is_running:
                self._print_color("Nginx is running", OutputColors.GREEN)
            else:
                self._print_color("Nginx is not running", OutputColors.RED)

            return is_running

        except Exception as e:
            self.logger.error(f"Error checking Nginx status: {str(e)}")
            return False

    def get_version(self) -> str:
        """
        Get the version of Nginx.

        Returns:
            The Nginx version string

        Raises:
            OperationError: If the version cannot be retrieved
        """
        result = self._run_command([str(self.paths.binary_path), '-v'])
        version_output = result.stderr.strip()
        self._print_color(version_output, OutputColors.CYAN)
        self.logger.info(f"Nginx version: {version_output}")
        return version_output

    def backup_config(self, custom_name: Optional[str] = None) -> Path:
        """
        Backup Nginx configuration file.

        Args:
            custom_name: Optional custom name for the backup file

        Returns:
            Path to the created backup file

        Raises:
            OperationError: If the backup cannot be created
        """
        # Create backup directory if it doesn't exist
        self.paths.backup_path.mkdir(parents=True, exist_ok=True)

        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_name = custom_name or f"nginx.conf.{timestamp}.bak"
        backup_file = self.paths.backup_path / backup_name

        try:
            shutil.copy2(self.paths.conf_path, backup_file)
            self._print_color(
                f"Nginx configuration file has been backed up to {backup_file}", OutputColors.GREEN)
            self.logger.info(f"Configuration backed up to {backup_file}")
            return backup_file
        except Exception as e:
            raise OperationError(
                f"Failed to backup configuration: {str(e)}") from e

    def list_backups(self) -> List[Path]:
        """
        List all available configuration backups.

        Returns:
            List of backup file paths
        """
        if not self.paths.backup_path.exists():
            return []

        backups = sorted(list(self.paths.backup_path.glob("nginx.conf.*.bak")),
                         key=lambda p: p.stat().st_mtime,
                         reverse=True)

        if backups:
            self._print_color(
                "Available configuration backups:", OutputColors.CYAN)
            for i, backup in enumerate(backups, 1):
                backup_time = datetime.datetime.fromtimestamp(
                    backup.stat().st_mtime)
                self._print_color(
                    f"{i}. {backup.name} - {backup_time.strftime('%Y-%m-%d %H:%M:%S')}", OutputColors.CYAN)
        else:
            self._print_color(
                "No configuration backups found", OutputColors.YELLOW)

        return backups

    def restore_config(self, backup_file: Optional[Union[Path, str]] = None) -> None:
        """
        Restore Nginx configuration from backup.

        Args:
            backup_file: Path to the backup file to restore (if None, uses latest backup)

        Raises:
            OperationError: If the restoration fails
        """
        if backup_file is None:
            backups = self.list_backups()
            if not backups:
                raise OperationError("No backup files found")
            backup_file = backups[0]  # Take the most recent backup

        if isinstance(backup_file, str):
            backup_file = Path(backup_file)

        if not backup_file.exists():
            raise OperationError(f"Backup file {backup_file} not found")

        try:
            # Make a backup of current config before restoring
            current_backup = self.backup_config(
                custom_name=f"pre_restore.{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.bak")

            # Restore the backup
            shutil.copy2(backup_file, self.paths.conf_path)
            self._print_color(
                f"Nginx configuration has been restored from {backup_file}", OutputColors.GREEN)
            self.logger.info(f"Configuration restored from {backup_file}")

            # Check if the restored config is valid
            self.check_config()

        except Exception as e:
            raise OperationError(
                f"Failed to restore configuration: {str(e)}") from e

    def create_virtual_host(self, server_name: str, port: int = 80,
                            root_dir: Optional[str] = None, template: str = 'basic') -> Path:
        """
        Create a new virtual host configuration.

        Args:
            server_name: Server name (e.g., example.com)
            port: Port number (default: 80)
            root_dir: Document root directory
            template: Template to use ('basic', 'php', 'proxy')

        Returns:
            Path to the created configuration file

        Raises:
            ConfigError: If creation fails
        """
        # Create sites-available and sites-enabled if they don't exist
        self.paths.sites_available.mkdir(parents=True, exist_ok=True)
        self.paths.sites_enabled.mkdir(parents=True, exist_ok=True)

        # Determine root directory if not specified
        if not root_dir:
            match self.os:
                case OperatingSystem.WINDOWS:
                    root_dir = f"C:/www/{server_name}"
                case _:
                    root_dir = f"/var/www/{server_name}"

        # Create config file path
        config_file = self.paths.sites_available / f"{server_name}.conf"

        # Templates for different virtual host configurations
        templates = {
            'basic': f"""server {{
    listen {port};
    server_name {server_name};
    root {root_dir};
    
    location / {{
        index index.html;
        try_files $uri $uri/ =404;
    }}
    
    access_log {self.paths.logs_path}/{server_name}.access.log;
    error_log {self.paths.logs_path}/{server_name}.error.log;
}}
""",
            'php': f"""server {{
    listen {port};
    server_name {server_name};
    root {root_dir};
    
    index index.php index.html;
    
    location / {{
        try_files $uri $uri/ /index.php$is_args$args;
    }}
    
    location ~ \\.php$ {{
        fastcgi_pass unix:/var/run/php/php-fpm.sock;
        fastcgi_index index.php;
        fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
        include fastcgi_params;
    }}
    
    access_log {self.paths.logs_path}/{server_name}.access.log;
    error_log {self.paths.logs_path}/{server_name}.error.log;
}}
""",
            'proxy': f"""server {{
    listen {port};
    server_name {server_name};
    
    location / {{
        proxy_pass http://localhost:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }}
    
    access_log {self.paths.logs_path}/{server_name}.access.log;
    error_log {self.paths.logs_path}/{server_name}.error.log;
}}
"""
        }

        if template not in templates:
            raise ConfigError(f"Unknown template: {template}")

        try:
            # Write the configuration file
            with open(config_file, 'w') as f:
                f.write(templates[template])

            self._print_color(
                f"Virtual host configuration created at {config_file}", OutputColors.GREEN)
            self.logger.info(f"Virtual host {server_name} created")
            return config_file

        except Exception as e:
            raise ConfigError(
                f"Failed to create virtual host: {str(e)}") from e

    def enable_virtual_host(self, server_name: str) -> None:
        """
        Enable a virtual host by creating a symlink in sites-enabled.

        Args:
            server_name: Name of the server configuration

        Raises:
            ConfigError: If the operation fails
        """
        source = self.paths.sites_available / f"{server_name}.conf"
        target = self.paths.sites_enabled / f"{server_name}.conf"

        if not source.exists():
            raise ConfigError(f"Virtual host configuration {source} not found")

        try:
            # Handle different OS symlink capabilities
            match self.os:
                case OperatingSystem.WINDOWS:
                    shutil.copy2(source, target)
                case _:
                    # Create symlink (remove if it already exists)
                    if target.exists():
                        target.unlink()
                    target.symlink_to(
                        Path(f"../sites-available/{server_name}.conf"))

            self._print_color(
                f"Virtual host {server_name} has been enabled", OutputColors.GREEN)
            self.logger.info(f"Virtual host {server_name} enabled")

            # Check config after enabling
            self.check_config()

        except Exception as e:
            raise ConfigError(
                f"Failed to enable virtual host: {str(e)}") from e

    def disable_virtual_host(self, server_name: str) -> None:
        """
        Disable a virtual host by removing the symlink from sites-enabled.

        Args:
            server_name: Name of the server configuration

        Raises:
            ConfigError: If the operation fails
        """
        target = self.paths.sites_enabled / f"{server_name}.conf"

        if not target.exists():
            self._print_color(
                f"Virtual host {server_name} is already disabled", OutputColors.YELLOW)
            return

        try:
            target.unlink()
            self._print_color(
                f"Virtual host {server_name} has been disabled", OutputColors.GREEN)
            self.logger.info(f"Virtual host {server_name} disabled")

        except Exception as e:
            raise ConfigError(
                f"Failed to disable virtual host: {str(e)}") from e

    def list_virtual_hosts(self) -> Dict[str, bool]:
        """
        List all virtual hosts and their status (enabled/disabled).

        Returns:
            Dictionary of virtual hosts with their status
        """
        result = {}

        # Create directories if they don't exist
        self.paths.sites_available.mkdir(parents=True, exist_ok=True)
        self.paths.sites_enabled.mkdir(parents=True, exist_ok=True)

        available_hosts = [
            f.stem for f in self.paths.sites_available.glob("*.conf")]
        enabled_hosts = [
            f.stem for f in self.paths.sites_enabled.glob("*.conf")]

        for host in available_hosts:
            result[host] = host in enabled_hosts

        if result:
            self._print_color("Virtual hosts:", OutputColors.CYAN)
            for host, enabled in result.items():
                status = "enabled" if enabled else "disabled"
                color = OutputColors.GREEN if enabled else OutputColors.YELLOW
                self._print_color(f"  {host} - {status}", color)
        else:
            self._print_color("No virtual hosts found", OutputColors.YELLOW)

        return result

    def analyze_logs(self, domain: Optional[str] = None,
                     lines: int = 100,
                     filter_pattern: Optional[str] = None) -> List[Dict[str, str]]:
        """
        Analyze Nginx access logs.

        Args:
            domain: Specific domain to analyze logs for
            lines: Number of lines to analyze
            filter_pattern: Regex pattern to filter log entries

        Returns:
            Parsed log entries as a list of dictionaries
        """
        log_path = None

        if domain:
            log_path = self.paths.logs_path / f"{domain}.access.log"
            if not log_path.exists():
                self._print_color(
                    f"No access log found for {domain}", OutputColors.YELLOW)
                return []
        else:
            log_path = self.paths.logs_path / "access.log"
            if not log_path.exists():
                self._print_color(
                    "No global access log found", OutputColors.YELLOW)
                return []

        try:
            # Tail the log file using appropriate OS command
            match self.os:
                case OperatingSystem.WINDOWS:
                    cmd = f"powershell -command \"Get-Content -Tail {lines} '{log_path}'\""
                case _:
                    cmd = f"tail -n {lines} {log_path}"

            result = self._run_command(cmd, shell=True)
            log_lines = result.stdout.strip().split("\n")

            # Parse log entries
            parsed_entries = []

            # Common Nginx log pattern (can be adjusted based on log format)
            log_pattern = r'(\S+) - (\S+) \[(.*?)\] "(.*?)" (\d+) (\d+) "(.*?)" "(.*?)"'

            for line in log_lines:
                if not line.strip():
                    continue

                if filter_pattern and not re.search(filter_pattern, line):
                    continue

                match = re.match(log_pattern, line)
                if match:
                    ip, user, timestamp, request, status, size, referer, user_agent = match.groups()

                    parsed_entries.append({
                        "ip": ip,
                        "user": user,
                        "timestamp": timestamp,
                        "request": request,
                        "status": status,
                        "size": size,
                        "referer": referer,
                        "user_agent": user_agent
                    })
                else:
                    # For lines that don't match the pattern, store them as raw entries
                    parsed_entries.append({"raw": line})

            # Display summary
            if parsed_entries:
                # Count status codes
                status_counts = {}
                for entry in parsed_entries:
                    if "status" in entry:
                        status = entry["status"]
                        status_counts[status] = status_counts.get(
                            status, 0) + 1

                self._print_color("Log Analysis Summary:", OutputColors.CYAN)
                self._print_color(
                    f"  Total entries: {len(parsed_entries)}", OutputColors.CYAN)

                if status_counts:
                    self._print_color(
                        "  Status code breakdown:", OutputColors.CYAN)

                    for status, count in sorted(status_counts.items()):
                        if status.startswith("2"):
                            color = OutputColors.GREEN
                        elif status.startswith("3"):
                            color = OutputColors.CYAN
                        elif status.startswith("4"):
                            color = OutputColors.YELLOW
                        else:
                            color = OutputColors.RED

                        self._print_color(f"    {status}: {count}", color)
            else:
                self._print_color("No log entries found", OutputColors.YELLOW)

            return parsed_entries

        except Exception as e:
            self.logger.error(f"Failed to analyze logs: {str(e)}")
            return []

    def generate_ssl_cert(self, domain: str, email: Optional[str] = None,
                          use_letsencrypt: bool = True) -> Tuple[Path, Path]:
        """
        Generate SSL certificates for a domain.

        Args:
            domain: Domain name
            email: Email address for Let's Encrypt
            use_letsencrypt: Whether to use Let's Encrypt (if False, uses self-signed)

        Returns:
            Tuple containing paths to certificate and key files

        Raises:
            OperationError: If certificate generation fails
        """
        # Create SSL directory if it doesn't exist
        self.paths.ssl_path.mkdir(parents=True, exist_ok=True)

        cert_path = self.paths.ssl_path / f"{domain}.crt"
        key_path = self.paths.ssl_path / f"{domain}.key"

        try:
            if use_letsencrypt:
                if not email:
                    raise OperationError("Email is required for Let's Encrypt")

                # Use certbot to generate certificates
                cmd = [
                    "certbot", "certonly", "--webroot",
                    "-w", "/var/www/html",
                    "-d", domain,
                    "--email", email,
                    "--agree-tos", "--non-interactive"
                ]

                self._run_command(cmd)

                # Link Let's Encrypt certificates to our location
                letsencrypt_cert = Path(
                    f"/etc/letsencrypt/live/{domain}/fullchain.pem")
                letsencrypt_key = Path(
                    f"/etc/letsencrypt/live/{domain}/privkey.pem")

                if letsencrypt_cert.exists() and letsencrypt_key.exists():
                    if cert_path.exists():
                        cert_path.unlink()
                    if key_path.exists():
                        key_path.unlink()

                    cert_path.symlink_to(letsencrypt_cert)
                    key_path.symlink_to(letsencrypt_key)
                else:
                    raise OperationError(
                        "Let's Encrypt certificates not found")
            else:
                # Generate self-signed certificate
                cmd = [
                    "openssl", "req", "-x509", "-nodes",
                    "-days", "365", "-newkey", "rsa:2048",
                    "-keyout", str(key_path),
                    "-out", str(cert_path),
                    "-subj", f"/CN={domain}"
                ]

                self._run_command(cmd)

            self._print_color(
                f"SSL certificate for {domain} generated successfully", OutputColors.GREEN)
            self.logger.info(f"SSL certificate generated for {domain}")
            return cert_path, key_path

        except Exception as e:
            raise OperationError(
                f"Failed to generate SSL certificate: {str(e)}") from e

    def configure_ssl(self, domain: str, cert_path: Path, key_path: Path) -> None:
        """
        Configure SSL for a virtual host.

        Args:
            domain: Domain name
            cert_path: Path to the certificate file
            key_path: Path to the key file

        Raises:
            ConfigError: If SSL configuration fails
        """
        config_path = self.paths.sites_available / f"{domain}.conf"

        if not config_path.exists():
            raise ConfigError(
                f"Virtual host configuration for {domain} not found")

        try:
            # Read the existing configuration
            with open(config_path, 'r') as f:
                config = f.read()

            # Check if SSL is already configured
            if "listen 443 ssl" in config:
                self._print_color(
                    f"SSL is already configured for {domain}", OutputColors.YELLOW)
                return

            # Modify the configuration to add SSL
            ssl_config = f"""
server {{
    listen 443 ssl;
    server_name {domain};
    
    ssl_certificate {cert_path};
    ssl_certificate_key {key_path};
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;
    
    # Rest of configuration copied from HTTP server block
"""

            # Extract the contents inside the existing server block
            match = re.search(r'server\s*{(.*?)}', config, re.DOTALL)
            if match:
                server_block_content = match.group(1)

                # Remove the listen directive from the copied content
                server_block_content = re.sub(
                    r'\s*listen\s+\d+;', '', server_block_content)

                # Complete the SSL server block
                ssl_config += server_block_content + "\n}"

                # Add HTTP to HTTPS redirection
                redirect_config = f"""
server {{
    listen 80;
    server_name {domain};
    return 301 https://$host$request_uri;
}}
"""

                # Replace the original configuration
                new_config = redirect_config + "\n" + ssl_config

                with open(config_path, 'w') as f:
                    f.write(new_config)

                self._print_color(
                    f"SSL configured for {domain}", OutputColors.GREEN)
                self.logger.info(f"SSL configured for {domain}")

                # Check if configuration is valid
                self.check_config()
            else:
                raise ConfigError(
                    f"Could not parse server block in {config_path}")

        except Exception as e:
            raise ConfigError(f"Failed to configure SSL: {str(e)}") from e

    def health_check(self) -> Dict[str, Any]:
        """
        Perform a comprehensive health check on Nginx installation.

        Returns:
            Dictionary containing health check results
        """
        results = {
            "nginx_installed": False,
            "nginx_running": False,
            "config_valid": False,
            "version": None,
            "virtual_hosts": 0,
            "errors": []
        }

        try:
            # Check if Nginx is installed
            results["nginx_installed"] = self.is_nginx_installed()

            if results["nginx_installed"]:
                # Get Nginx version
                try:
                    version_output = self.get_version()
                    version_match = re.search(
                        r'nginx/(\d+\.\d+\.\d+)', version_output)
                    if version_match:
                        results["version"] = version_match.group(1)
                except Exception as e:
                    results["errors"].append(
                        f"Failed to get version: {str(e)}")

                # Check if Nginx is running
                try:
                    results["nginx_running"] = self.get_status()
                except Exception as e:
                    results["errors"].append(
                        f"Failed to check status: {str(e)}")

                # Check if configuration is valid
                try:
                    results["config_valid"] = self.check_config()
                except Exception as e:
                    results["errors"].append(
                        f"Failed to check config: {str(e)}")

                # Count virtual hosts
                try:
                    virtual_hosts = list(
                        self.paths.sites_available.glob("*.conf"))
                    results["virtual_hosts"] = len(virtual_hosts)
                except Exception as e:
                    results["errors"].append(
                        f"Failed to count virtual hosts: {str(e)}")

                # Check disk space for logs
                try:
                    if self.paths.logs_path.exists():
                        if self.os != OperatingSystem.WINDOWS:
                            df_result = self._run_command(
                                f"df -h {self.paths.logs_path}", shell=True)
                            results["disk_space"] = df_result.stdout.strip()
                except Exception as e:
                    results["errors"].append(
                        f"Failed to check disk space: {str(e)}")

            # Display results
            self._print_color("Nginx Health Check Results:", OutputColors.CYAN)
            self._print_color(f"  Installed: {results['nginx_installed']}",
                              OutputColors.GREEN if results["nginx_installed"] else OutputColors.RED)

            if results["nginx_installed"]:
                self._print_color(f"  Running: {results['nginx_running']}",
                                  OutputColors.GREEN if results["nginx_running"] else OutputColors.RED)
                self._print_color(f"  Configuration Valid: {results['config_valid']}",
                                  OutputColors.GREEN if results["config_valid"] else OutputColors.RED)
                self._print_color(
                    f"  Version: {results['version']}", OutputColors.CYAN)
                self._print_color(
                    f"  Virtual Hosts: {results['virtual_hosts']}", OutputColors.CYAN)

                if "disk_space" in results:
                    self._print_color(
                        f"  Disk Space:\n{results['disk_space']}", OutputColors.CYAN)

            if results["errors"]:
                self._print_color("  Errors:", OutputColors.RED)
                for error in results["errors"]:
                    self._print_color(f"    - {error}", OutputColors.RED)

            return results

        except Exception as e:
            self.logger.error(f"Health check failed: {str(e)}")
            results["errors"].append(f"Health check failed: {str(e)}")
            return results


class NginxManagerCLI:
    """
    Command-line interface for the NginxManager.

    This class provides a command-line interface to interact with
    the NginxManager class using argparse.
    """

    def __init__(self):
        """Initialize the CLI with a NginxManager instance."""
        self.manager = NginxManager()

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

        return parser

    def run(self) -> int:
        """
        Parse arguments and execute the requested command.

        Returns:
            Exit code (0 for success, non-zero for failure)
        """
        parser = self.setup_parser()
        args = parser.parse_args()

        if not args.command:
            parser.print_help()
            return 1

        try:
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

            return 0

        except Exception as e:
            print(f"Error: {str(e)}")
            return 1


class NginxManagerBindings:
    """
    Class providing bindings for pybind11 integration.

    This class wraps NginxManager functionality to be called from C++ via pybind11.
    """

    def __init__(self):
        """Initialize with a new NginxManager instance."""
        self.manager = NginxManager(use_colors=False)

    def is_installed(self) -> bool:
        """Check if Nginx is installed."""
        return self.manager.is_nginx_installed()

    def install(self) -> bool:
        """Install Nginx if not already installed."""
        try:
            self.manager.install_nginx()
            return True
        except Exception:
            return False

    def start(self) -> bool:
        """Start Nginx server."""
        try:
            self.manager.start_nginx()
            return True
        except Exception:
            return False

    def stop(self) -> bool:
        """Stop Nginx server."""
        try:
            self.manager.stop_nginx()
            return True
        except Exception:
            return False

    def reload(self) -> bool:
        """Reload Nginx configuration."""
        try:
            self.manager.reload_nginx()
            return True
        except Exception:
            return False

    def restart(self) -> bool:
        """Restart Nginx server."""
        try:
            self.manager.restart_nginx()
            return True
        except Exception:
            return False

    def check_config(self) -> bool:
        """Check Nginx configuration syntax."""
        try:
            return self.manager.check_config()
        except Exception:
            return False

    def get_status(self) -> bool:
        """Check if Nginx is running."""
        return self.manager.get_status()

    def get_version(self) -> str:
        """Get Nginx version."""
        try:
            return self.manager.get_version()
        except Exception:
            return ""

    def backup_config(self, custom_name: str = "") -> str:
        """Backup Nginx configuration."""
        try:
            backup_path = self.manager.backup_config(
                custom_name=custom_name if custom_name else None
            )
            return str(backup_path)
        except Exception:
            return ""

    def restore_config(self, backup_file: str = "") -> bool:
        """Restore Nginx configuration from backup."""
        try:
            self.manager.restore_config(
                backup_file=backup_file if backup_file else None
            )
            return True
        except Exception:
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
        except Exception:
            return ""

    def enable_virtual_host(self, server_name: str) -> bool:
        """Enable a virtual host."""
        try:
            self.manager.enable_virtual_host(server_name)
            return True
        except Exception:
            return False

    def disable_virtual_host(self, server_name: str) -> bool:
        """Disable a virtual host."""
        try:
            self.manager.disable_virtual_host(server_name)
            return True
        except Exception:
            return False

    def list_virtual_hosts(self) -> str:
        """List all virtual hosts and their status."""
        try:
            vhosts = self.manager.list_virtual_hosts()
            return json.dumps(vhosts)
        except Exception:
            return "{}"

    def health_check(self) -> str:
        """Perform a health check."""
        try:
            result = self.manager.health_check()
            return json.dumps(result)
        except Exception:
            return "{\"error\": \"Health check failed\"}"


def main() -> int:
    """
    Main entry point for the command-line application.

    Returns:
        Exit code (0 for success, non-zero for failure)
    """
    cli = NginxManagerCLI()
    return cli.run()


if __name__ == "__main__":
    sys.exit(main())
else:
    # When imported as a module, expose classes for direct Python usage
    # or pybind11 integration
    __all__ = ['NginxManager', 'NginxManagerBindings', 'NginxError',
               'ConfigError', 'InstallationError', 'OperationError']
