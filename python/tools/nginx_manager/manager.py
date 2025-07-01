#!/usr/bin/env python3
"""
Main NginxManager class implementation with modern Python features.
"""

import asyncio
import datetime
import platform
import shutil
import subprocess
from functools import lru_cache
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Union, Any, Callable, Awaitable

from loguru import logger

from .core import (
    OperatingSystem,
    NginxError,
    ConfigError,
    InstallationError,
    OperationError,
    NginxPaths,
)
from .utils import OutputColors


class NginxManager:
    """
    Main class for managing Nginx operations with a modern, extensible design.
    """

    def __init__(
        self,
        use_colors: bool = True,
        paths: Optional[NginxPaths] = None,
        runner: Optional[Callable[..., Awaitable[subprocess.CompletedProcess]]] = None,
    ):
        """
        Initialize the NginxManager.
        Args:
            use_colors: Whether to use colored output in terminal.
            paths: Optional NginxPaths object for dependency injection.
            runner: Optional async function to run shell commands.
        """
        self.os = self._detect_os()
        self.paths = paths or self._setup_paths()
        self.use_colors = use_colors and OutputColors.is_color_supported()
        self.run_command = runner or self._run_command
        self.plugins = {}
        logger.debug(f"NginxManager initialized with OS: {self.os.value}")

    def register_plugin(self, name: str, plugin: Any) -> None:
        """Register a new plugin."""
        self.plugins[name] = plugin
        logger.info(f"Plugin '{name}' registered.")

    def _detect_os(self) -> OperatingSystem:
        """Detect the current operating system."""
        system = platform.system().lower()
        try:
            return next(os_type for os_type in OperatingSystem if os_type.value == system)
        except StopIteration:
            return OperatingSystem.UNKNOWN

    def _setup_paths(self) -> NginxPaths:
        """Set up the path configuration based on the detected OS."""
        base_path, binary_path, logs_path = self._get_os_specific_paths()
        return NginxPaths(
            base_path=base_path,
            conf_path=base_path / "nginx.conf",
            binary_path=binary_path,
            backup_path=base_path / "backup",
            sites_available=base_path / "sites-available",
            sites_enabled=base_path / "sites-enabled",
            logs_path=logs_path,
            ssl_path=base_path / "ssl",
        )

    def _get_os_specific_paths(self) -> Tuple[Path, Path, Path]:
        """Return OS-specific paths for Nginx."""
        if self.os == OperatingSystem.LINUX:
            return Path("/etc/nginx"), Path("/usr/sbin/nginx"), Path("/var/log/nginx")
        if self.os == OperatingSystem.WINDOWS:
            base = Path("C:/nginx")
            return base, base / "nginx.exe", base / "logs"
        if self.os == OperatingSystem.MACOS:
            return (
                Path("/usr/local/etc/nginx"),
                Path("/usr/local/bin/nginx"),
                Path("/usr/local/var/log/nginx"),
            )
        logger.warning("Unknown OS, defaulting to Linux paths.")
        return Path("/etc/nginx"), Path("/usr/sbin/nginx"), Path("/var/log/nginx")

    def _print_color(self, message: str, color: str = OutputColors.RESET) -> None:
        """Print a message with color if color output is enabled."""
        print(f"{color}{message}{OutputColors.RESET}" if self.use_colors else message)

    async def _run_command(
        self, cmd: Union[List[str], str], check: bool = True, **kwargs
    ) -> subprocess.CompletedProcess:
        """Run a shell command asynchronously with proper error handling."""
        try:
            logger.debug(f"Running command: {cmd}")
            proc = await asyncio.create_subprocess_shell(
                cmd if isinstance(cmd, str) else " ".join(cmd),
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                **kwargs,
            )
            stdout, stderr = await proc.communicate()
            
            # Fixed: Use the original command as args since proc.args isn't accessible
            args = cmd
            # Fixed: Ensure returncode is not None
            returncode = proc.returncode if proc.returncode is not None else 1
            
            result = subprocess.CompletedProcess(
                args, 
                returncode, 
                stdout.decode(), 
                stderr.decode()
            )
            if check and result.returncode != 0:
                raise OperationError(
                    f"Command '{cmd}' failed: {result.stderr.strip()}"
                )
            return result
        except Exception as e:
            logger.error(f"Command execution failed: {e}")
            raise OperationError(str(e)) from e

    @lru_cache(maxsize=1)
    async def is_nginx_installed(self) -> bool:
        """Check if Nginx is installed."""
        try:
            result = await self.run_command(
                [str(self.paths.binary_path), "-v"], check=False
            )
            return result.returncode == 0
        except FileNotFoundError:
            logger.debug("Nginx binary not found.")
            return False

    async def install_nginx(self) -> None:
        """Install Nginx if not already installed."""
        if await self.is_nginx_installed():
            logger.info("Nginx is already installed.")
            return

        logger.info("Installing Nginx...")
        install_commands = {
            OperatingSystem.LINUX: {
                "debian": "sudo apt-get update && sudo apt-get install -y nginx",
                "redhat": "sudo yum update && sudo yum install -y nginx",
            },
            OperatingSystem.MACOS: "brew update && brew install nginx",
        }

        cmd = None
        if self.os == OperatingSystem.LINUX:
            if Path("/etc/debian_version").exists():
                cmd = install_commands[self.os]["debian"]
            elif Path("/etc/redhat-release").exists():
                cmd = install_commands[self.os]["redhat"]
        elif self.os == OperatingSystem.MACOS:
            cmd = install_commands[self.os]

        if cmd:
            await self.run_command(cmd, shell=True)
            logger.success("Nginx installed successfully.")
        else:
            raise InstallationError(
                "Unsupported OS for automatic installation. Please install manually."
            )

    async def manage_service(self, action: str) -> None:
        """Manage the Nginx service (start, stop, reload, restart)."""
        if not await self.is_nginx_installed():
            raise OperationError("Nginx is not installed.")

        if action in ("start", "restart") and not self.paths.binary_path.exists():
            raise OperationError("Nginx binary not found.")

        cmd_map = {
            "start": [str(self.paths.binary_path)],
            "stop": [str(self.paths.binary_path), "-s", "stop"],
            "reload": [str(self.paths.binary_path), "-s", "reload"],
        }

        if action == "restart":
            await self.manage_service("stop")
            await asyncio.sleep(1)  # Give time for the service to stop
            await self.manage_service("start")
        elif action in cmd_map:
            await self.run_command(cmd_map[action])
        else:
            raise ValueError(f"Invalid service action: {action}")

        self._print_color(f"Nginx has been {action}ed.", OutputColors.GREEN)
        logger.success(f"Nginx {action}ed.")

    async def check_config(self) -> bool:
        """Check the syntax of the Nginx configuration files."""
        if not self.paths.conf_path.exists():
            raise ConfigError("Nginx configuration file not found.")
        try:
            await self.run_command(
                [str(self.paths.binary_path), "-t", "-c", str(self.paths.conf_path)]
            )
            self._print_color("Nginx configuration is valid.", OutputColors.GREEN)
            return True
        except OperationError as e:
            self._print_color(f"Nginx configuration is invalid: {e}", OutputColors.RED)
            return False

    async def get_status(self) -> bool:
        """Check if Nginx is running."""
        cmd = (
            "tasklist | findstr nginx.exe"
            if self.os == OperatingSystem.WINDOWS
            else "pgrep nginx"
        )
        result = await self.run_command(cmd, shell=True, check=False)
        is_running = result.returncode == 0 and result.stdout.strip()
        status_msg = "running" if is_running else "not running"
        color = OutputColors.GREEN if is_running else OutputColors.RED
        self._print_color(f"Nginx is {status_msg}.", color)
        return is_running

    async def get_version(self) -> str:
        """Get the version of Nginx."""
        result = await self.run_command([str(self.paths.binary_path), "-v"])
        version = result.stderr.strip()
        self._print_color(version, OutputColors.CYAN)
        return version

    async def backup_config(self, custom_name: Optional[str] = None) -> Path:
        """Backup Nginx configuration file."""
        self.paths.backup_path.mkdir(parents=True, exist_ok=True)
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_file = self.paths.backup_path / (
            custom_name or f"nginx.conf.{timestamp}.bak"
        )
        shutil.copy2(self.paths.conf_path, backup_file)
        self._print_color(f"Config backed up to {backup_file}", OutputColors.GREEN)
        return backup_file

    def list_backups(self) -> List[Path]:
        """List all available configuration backups."""
        if not self.paths.backup_path.exists():
            return []
        return sorted(
            self.paths.backup_path.glob("*.bak"),
            key=lambda p: p.stat().st_mtime,
            reverse=True,
        )

    async def restore_config(
        self, backup_file: Optional[Union[Path, str]] = None
    ) -> None:
        """Restore Nginx configuration from backup."""
        backups = self.list_backups()
        if not backups:
            raise OperationError("No backups found.")

        to_restore = Path(backup_file) if backup_file else backups[0]
        if not to_restore.exists():
            raise OperationError(f"Backup file {to_restore} not found.")

        await self.backup_config(f"pre-restore-{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.bak")
        shutil.copy2(to_restore, self.paths.conf_path)
        self._print_color(f"Config restored from {to_restore}", OutputColors.GREEN)
        await self.check_config()

    async def manage_virtual_host(
        self, action: str, server_name: str, **kwargs
    ) -> Optional[Path]:
        """Manage virtual hosts (create, enable, disable)."""
        actions = {
            "create": self._create_vhost,
            "enable": self._enable_vhost,
            "disable": self._disable_vhost,
        }
        if action not in actions:
            raise ValueError(f"Invalid virtual host action: {action}")
        return await actions[action](server_name, **kwargs)

    async def _create_vhost(
        self,
        server_name: str,
        port: int = 80,
        root_dir: Optional[str] = None,
        template: str = "basic",
    ) -> Path:
        """Create a new virtual host configuration."""
        self.paths.sites_available.mkdir(parents=True, exist_ok=True)
        root = root_dir or (
            f"C:/www/{server_name}"
            if self.os == OperatingSystem.WINDOWS
            else f"/var/www/{server_name}"
        )
        config_file = self.paths.sites_available / f"{server_name}.conf"

        templates = self.plugins.get("vhost_templates", {})
        if template not in templates:
            raise ConfigError(f"Unknown or unregistered template: {template}")

        config_content = templates[template](
            server_name=server_name, port=port, root_dir=root, paths=self.paths
        )
        config_file.write_text(config_content)
        self._print_color(f"Vhost {server_name} created.", OutputColors.GREEN)
        return config_file

    async def _enable_vhost(self, server_name: str, **_) -> None:
        """Enable a virtual host."""
        source = self.paths.sites_available / f"{server_name}.conf"
        target = self.paths.sites_enabled / f"{server_name}.conf"
        if not source.exists():
            raise ConfigError(f"Vhost config {source} not found.")
        if self.os == OperatingSystem.WINDOWS:
            shutil.copy2(source, target)
        else:
            if target.exists():
                target.unlink()
            target.symlink_to(f"../sites-available/{server_name}.conf")
        self._print_color(f"Vhost {server_name} enabled.", OutputColors.GREEN)
        await self.check_config()

    async def _disable_vhost(self, server_name: str, **_) -> None:
        """Disable a virtual host."""
        target = self.paths.sites_enabled / f"{server_name}.conf"
        if target.exists():
            target.unlink()
            self._print_color(f"Vhost {server_name} disabled.", OutputColors.GREEN)
        else:
            self._print_color(f"Vhost {server_name} already disabled.", OutputColors.YELLOW)

    def list_virtual_hosts(self) -> Dict[str, bool]:
        """List all virtual hosts and their status."""
        self.paths.sites_available.mkdir(exist_ok=True)
        self.paths.sites_enabled.mkdir(exist_ok=True)
        available = {f.stem for f in self.paths.sites_available.glob("*.conf")}
        enabled = {f.stem for f in self.paths.sites_enabled.glob("*.conf")}
        return {host: host in enabled for host in available}

    async def health_check(self) -> Dict[str, Any]:
        """Perform a comprehensive health check."""
        logger.info("Starting Nginx health check...")
        results = {
            "installed": await self.is_nginx_installed(),
            "running": False,
            "config_valid": False,
            "version": None,
            "virtual_hosts": 0,
            "errors": [],
        }
        if results["installed"]:
            try:
                results["version"] = await self.get_version()
                results["running"] = await self.get_status()
                results["config_valid"] = await self.check_config()
                results["virtual_hosts"] = len(self.list_virtual_hosts())
            except OperationError as e:
                results["errors"].append(str(e))
        self._print_color("Health Check Results:", OutputColors.CYAN)
        for key, value in results.items():
            self._print_color(f"  {key.replace('_', ' ').title()}: {value}")
        return results


# Default virtual host templates
def basic_template(**kwargs) -> str:
    return f"""server {{
    listen {kwargs['port']};
    server_name {kwargs['server_name']};
    root {kwargs['root_dir']};
    location / {{
        index index.html;
        try_files $uri $uri/ =404;
    }}
    access_log {kwargs['paths'].logs_path}/{kwargs['server_name']}.access.log;
    error_log {kwargs['paths'].logs_path}/{kwargs['server_name']}.error.log;
}}"""


def php_template(**kwargs) -> str:
    return f"""server {{
    listen {kwargs['port']};
    server_name {kwargs['server_name']};
    root {kwargs['root_dir']};
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
    access_log {kwargs['paths'].logs_path}/{kwargs['server_name']}.access.log;
    error_log {kwargs['paths'].logs_path}/{kwargs['server_name']}.error.log;
}}"""


def proxy_template(**kwargs) -> str:
    return f"""server {{
    listen {kwargs['port']};
    server_name {kwargs['server_name']};
    location / {{
        proxy_pass http://localhost:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }}
    access_log {kwargs['paths'].logs_path}/{kwargs['server_name']}.access.log;
    error_log {kwargs['paths'].logs_path}/{kwargs['server_name']}.error.log;
}}"""


async def main():
    """Example usage of the NginxManager."""
    manager = NginxManager()
    manager.register_plugin(
        "vhost_templates",
        {"basic": basic_template, "php": php_template, "proxy": proxy_template},
    )
    await manager.health_check()


if __name__ == "__main__":
    asyncio.run(main())

