#!/usr/bin/env python3
"""
Main NginxManager class implementation with modern Python features.
"""

from __future__ import annotations

import asyncio
import datetime
import platform
import shutil
import subprocess
from contextlib import asynccontextmanager
from functools import lru_cache
from pathlib import Path
from typing import List, Optional, Union, Any, Callable, Awaitable, AsyncGenerator

from loguru import logger

from .core import (
    OperatingSystem,
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
    ) -> None:
        """
        Initialize the NginxManager.
        Args:
            use_colors: Whether to use colored output in terminal.
            paths: Optional NginxPaths object for dependency injection.
            runner: Optional async function to run shell commands.
        """
        self._os = self._detect_os()
        self._paths = paths or self._setup_paths()
        self.use_colors = use_colors and OutputColors.is_color_supported()
        self.run_command = runner or self._run_command
        self.plugins: dict[str, Any] = {}
        logger.debug(f"NginxManager initialized with OS: {self._os!s}")

    @property
    def os(self) -> OperatingSystem:
        """Get the detected operating system."""
        return self._os

    @property
    def paths(self) -> NginxPaths:
        """Get the Nginx paths configuration."""
        return self._paths

    def register_plugin(self, name: str, plugin: Any) -> None:
        """Register a new plugin."""
        self.plugins[name] = plugin
        logger.info(f"Plugin '{name}' registered.")

    def _detect_os(self) -> OperatingSystem:
        """Detect the current operating system."""
        system = platform.system().lower()
        return OperatingSystem.from_platform(system)

    def _setup_paths(self) -> NginxPaths:
        """Set up the path configuration based on the detected OS."""
        base_path, binary_path, logs_path = self._get_os_specific_paths()
        return NginxPaths.from_base_path(base_path, binary_path, logs_path)

    def _get_os_specific_paths(self) -> tuple[Path, Path, Path]:
        """Return OS-specific paths for Nginx."""
        match self._os:
            case OperatingSystem.LINUX:
                return (
                    Path("/etc/nginx"),
                    Path("/usr/sbin/nginx"),
                    Path("/var/log/nginx"),
                )
            case OperatingSystem.WINDOWS:
                base = Path("C:/nginx")
                return base, base / "nginx.exe", base / "logs"
            case OperatingSystem.MACOS:
                return (
                    Path("/usr/local/etc/nginx"),
                    Path("/usr/local/bin/nginx"),
                    Path("/usr/local/var/log/nginx"),
                )
            case _:
                logger.warning("Unknown OS, defaulting to Linux paths.")
                return (
                    Path("/etc/nginx"),
                    Path("/usr/sbin/nginx"),
                    Path("/var/log/nginx"),
                )

    def _print_color(
        self, message: str, color: OutputColors = OutputColors.RESET
    ) -> None:
        """Print a message with color if color output is enabled."""
        if self.use_colors:
            print(color.format_text(message))
        else:
            print(message)

    async def _run_command(
        self, cmd: Union[List[str], str], check: bool = True, **kwargs
    ) -> subprocess.CompletedProcess:
        """Run a shell command asynchronously with proper error handling and context management."""
        command_str = cmd if isinstance(cmd, str) else " ".join(cmd)

        try:
            logger.debug(f"Running command: {command_str}")

            async with self._command_context():
                proc = await asyncio.create_subprocess_shell(
                    command_str,
                    stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.PIPE,
                    **kwargs,
                )
                stdout, stderr = await proc.communicate()

                returncode = proc.returncode or 0
                result = subprocess.CompletedProcess(
                    args=cmd,
                    returncode=returncode,
                    stdout=stdout.decode(errors="replace"),
                    stderr=stderr.decode(errors="replace"),
                )

                if check and result.returncode != 0:
                    error_msg = (
                        result.stderr.strip()
                        or result.stdout.strip()
                        or "Command failed"
                    )
                    raise OperationError(
                        f"Command '{command_str}' failed",
                        error_code=result.returncode,
                        details={"stderr": error_msg},
                    )

                logger.debug(f"Command completed with return code: {result.returncode}")
                return result

        except asyncio.TimeoutError as e:
            logger.error(f"Command '{command_str}' timed out")
            raise OperationError(
                f"Command '{command_str}' timed out", details={"timeout": str(e)}
            ) from e
        except OSError as e:
            logger.error(f"OS error running command '{command_str}': {e}")
            raise OperationError(
                f"OS error: {e}", details={"command": command_str}
            ) from e
        except Exception as e:
            logger.error(f"Unexpected error running command '{command_str}': {e}")
            raise OperationError(
                f"Unexpected error: {e}", details={"command": command_str}
            ) from e

    @asynccontextmanager
    async def _command_context(self) -> AsyncGenerator[None, None]:
        """Context manager for command execution with proper cleanup."""
        try:
            yield
        except Exception:
            # Log any cleanup needed here
            logger.debug("Command execution context cleanup")
            raise

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
        """Install Nginx if not already installed with enhanced error handling."""
        try:
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
            match self._os:
                case OperatingSystem.LINUX:
                    if Path("/etc/debian_version").exists():
                        cmd = install_commands[self._os]["debian"]
                    elif Path("/etc/redhat-release").exists():
                        cmd = install_commands[self._os]["redhat"]
                    else:
                        raise InstallationError(
                            "Unsupported Linux distribution for automatic installation",
                            details={
                                "detected_files": str(
                                    list(Path("/etc").glob("*-release"))
                                )
                            },
                        )
                case OperatingSystem.MACOS:
                    cmd = install_commands[self._os]
                case _:
                    raise InstallationError(
                        "Unsupported OS for automatic installation. Please install manually.",
                        details={"detected_os": str(self._os)},
                    )

            if cmd:
                await self.run_command(cmd, shell=True)
                logger.success("Nginx installed successfully.")
                self._paths.ensure_directories()  # Ensure directories exist after installation

        except (OSError, PermissionError) as e:
            raise InstallationError(
                f"Permission or system error during installation: {e}",
                details={"error_type": type(e).__name__},
            ) from e
        except Exception as e:
            if isinstance(e, (InstallationError, OperationError)):
                raise
            raise InstallationError(f"Unexpected error during installation: {e}") from e

    async def manage_service(self, action: str) -> None:
        """Manage the Nginx service (start, stop, reload, restart) with enhanced error handling."""
        valid_actions = {"start", "stop", "reload", "restart"}
        if action not in valid_actions:
            raise ValueError(
                f"Invalid service action: {action}. Valid actions: {valid_actions}"
            )

        try:
            if not await self.is_nginx_installed():
                raise OperationError(
                    "Nginx is not installed",
                    details={"action": action, "suggestion": "Install Nginx first"},
                )

            if action in ("start", "restart") and not self.paths.binary_path.exists():
                raise OperationError(
                    "Nginx binary not found",
                    details={
                        "binary_path": str(self.paths.binary_path),
                        "action": action,
                    },
                )

            cmd_map = {
                "start": [str(self.paths.binary_path)],
                "stop": [str(self.paths.binary_path), "-s", "stop"],
                "reload": [str(self.paths.binary_path), "-s", "reload"],
            }

            if action == "restart":
                logger.info("Restarting Nginx: stopping first...")
                await self.manage_service("stop")
                await asyncio.sleep(1)  # Give time for the service to stop
                logger.info("Starting Nginx...")
                await self.manage_service("start")
            elif action in cmd_map:
                await self.run_command(cmd_map[action])

            self._print_color(f"Nginx has been {action}ed.", OutputColors.GREEN)
            logger.success(f"Nginx {action}ed successfully.")

        except (OSError, PermissionError) as e:
            raise OperationError(
                f"Permission or system error during {action}: {e}",
                details={"action": action, "error_type": type(e).__name__},
            ) from e
        except Exception as e:
            if isinstance(e, (OperationError, ValueError)):
                raise
            raise OperationError(
                f"Unexpected error during {action}: {e}", details={"action": action}
            ) from e

    async def check_config(self) -> bool:
        """Check the syntax of the Nginx configuration files with enhanced validation."""
        try:
            if not self.paths.conf_path.exists():
                raise ConfigError(
                    "Nginx configuration file not found",
                    details={"config_path": str(self.paths.conf_path)},
                )

            logger.debug(f"Checking configuration at {self.paths.conf_path}")
            await self.run_command(
                [str(self.paths.binary_path), "-t", "-c", str(self.paths.conf_path)]
            )
            self._print_color("Nginx configuration is valid.", OutputColors.GREEN)
            logger.success("Configuration validation passed")
            return True

        except OperationError as e:
            error_details = e.details.get("stderr", str(e))
            self._print_color(
                f"Nginx configuration is invalid: {error_details}", OutputColors.RED
            )
            logger.error(f"Configuration validation failed: {error_details}")
            return False
        except Exception as e:
            logger.error(f"Unexpected error during config check: {e}")
            raise ConfigError(f"Config check failed: {e}") from e

    async def get_status(self) -> bool:
        """Check if Nginx is running with OS-specific commands."""
        try:
            match self._os:
                case OperatingSystem.WINDOWS:
                    cmd = "tasklist | findstr nginx.exe"
                case _:
                    cmd = "pgrep nginx"

            result = await self.run_command(cmd, shell=True, check=False)
            is_running = result.returncode == 0 and result.stdout.strip()
            status_msg = "running" if is_running else "not running"
            color = OutputColors.GREEN if is_running else OutputColors.RED

            self._print_color(f"Nginx is {status_msg}.", color)
            logger.info(f"Nginx status check: {status_msg}")
            return is_running

        except Exception as e:
            logger.error(f"Error checking Nginx status: {e}")
            raise OperationError(f"Status check failed: {e}") from e

    async def get_version(self) -> str:
        """Get the version of Nginx with error handling."""
        try:
            result = await self.run_command([str(self.paths.binary_path), "-v"])
            # Nginx outputs version to stderr by default
            version = result.stderr.strip() or result.stdout.strip()
            if not version:
                raise OperationError("No version information returned")

            self._print_color(version, OutputColors.CYAN)
            logger.info(f"Nginx version: {version}")
            return version

        except Exception as e:
            if isinstance(e, OperationError):
                raise
            raise OperationError(f"Failed to get Nginx version: {e}") from e

    async def backup_config(self, custom_name: Optional[str] = None) -> Path:
        """Backup Nginx configuration file with enhanced error handling."""
        try:
            self.paths.backup_path.mkdir(parents=True, exist_ok=True)

            if not self.paths.conf_path.exists():
                raise ConfigError(
                    "Source configuration file does not exist",
                    details={"config_path": str(self.paths.conf_path)},
                )

            timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            backup_name = custom_name or f"nginx.conf.{timestamp}.bak"

            # Ensure backup name is safe
            safe_backup_name = Path(backup_name).name  # Remove any path components
            backup_file = self.paths.backup_path / safe_backup_name

            # Check if backup already exists
            if backup_file.exists() and not custom_name:
                backup_file = (
                    self.paths.backup_path / f"nginx.conf.{timestamp}_{id(self)}.bak"
                )

            shutil.copy2(self.paths.conf_path, backup_file)
            self._print_color(f"Config backed up to {backup_file}", OutputColors.GREEN)
            logger.success(f"Configuration backed up to {backup_file}")
            return backup_file

        except (OSError, PermissionError) as e:
            raise ConfigError(
                f"Failed to backup configuration: {e}",
                details={
                    "source": str(self.paths.conf_path),
                    "backup_dir": str(self.paths.backup_path),
                },
            ) from e
        except Exception as e:
            if isinstance(e, ConfigError):
                raise
            raise ConfigError(f"Unexpected error during backup: {e}") from e

    def list_backups(self) -> list[Path]:
        """List all available configuration backups sorted by modification time."""
        try:
            if not self.paths.backup_path.exists():
                logger.debug(
                    f"Backup directory does not exist: {self.paths.backup_path}"
                )
                return []

            backups = list(self.paths.backup_path.glob("*.bak"))
            return sorted(backups, key=lambda p: p.stat().st_mtime, reverse=True)

        except (OSError, PermissionError) as e:
            logger.warning(f"Cannot access backup directory: {e}")
            return []
        except Exception as e:
            logger.error(f"Unexpected error listing backups: {e}")
            return []

    async def restore_config(
        self, backup_file: Optional[Union[Path, str]] = None
    ) -> None:
        """Restore Nginx configuration from backup with enhanced validation."""
        to_restore: Path | None = None  # Initialize to_restore
        try:
            backups = self.list_backups()
            if not backups:
                raise OperationError(
                    "No backups found",
                    details={"backup_dir": str(self.paths.backup_path)},
                )

            to_restore = Path(backup_file) if backup_file else backups[0]

            if not to_restore.exists():
                raise OperationError(
                    f"Backup file not found: {to_restore}",
                    details={
                        "backup_file": str(to_restore),
                        "available_backups": [
                            str(b) for b in backups[:5]
                        ],  # Show first 5
                    },
                )

            # Validate backup file before proceeding
            if not to_restore.suffix == ".bak":
                logger.warning(f"Backup file doesn't have .bak extension: {to_restore}")

            # Create a pre-restore backup
            pre_restore_name = (
                f"pre-restore-{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.bak"
            )
            await self.backup_config(pre_restore_name)

            # Perform the restoration
            shutil.copy2(to_restore, self.paths.conf_path)
            self._print_color(f"Config restored from {to_restore}", OutputColors.GREEN)
            logger.success(f"Configuration restored from {to_restore}")

            # Validate the restored configuration
            if not await self.check_config():
                logger.warning("Restored configuration failed validation")

        except (OSError, PermissionError) as e:
            raise OperationError(
                f"Permission error during restore: {e}",
                details={
                    "backup_file": (
                        str(to_restore) if "to_restore" in locals() else "unknown"
                    )
                },
            ) from e
        except Exception as e:
            if isinstance(e, OperationError):
                raise
            raise OperationError(f"Unexpected error during restore: {e}") from e

    async def manage_virtual_host(
        self, action: str, server_name: str, **kwargs
    ) -> Optional[Path]:
        """Manage virtual hosts (create, enable, disable) with enhanced validation."""
        valid_actions = {"create", "enable", "disable"}
        if action not in valid_actions:
            raise ValueError(
                f"Invalid virtual host action: {action}. Valid actions: {valid_actions}"
            )

        # Validate server name
        if not server_name or not isinstance(server_name, str):
            raise ValueError("Server name must be a non-empty string")

        # Basic server name validation (prevent directory traversal)
        if any(char in server_name for char in ["/", "\\", "..", "\0"]):
            raise ValueError(f"Invalid server name: {server_name}")

        try:
            actions_map = {
                "create": self._create_vhost,
                "enable": self._enable_vhost,
                "disable": self._disable_vhost,
            }
            logger.info(f"Managing virtual host '{server_name}': {action}")
            return await actions_map[action](server_name, **kwargs)

        except Exception as e:
            if isinstance(e, (ValueError, ConfigError, OperationError)):
                raise
            raise OperationError(
                f"Failed to {action} virtual host '{server_name}': {e}",
                details={"action": action, "server_name": server_name},
            ) from e

    async def _create_vhost(
        self,
        server_name: str,
        port: int = 80,
        root_dir: Optional[str] = None,
        template: str = "basic",
    ) -> Path:
        """Create a new virtual host configuration with enhanced validation."""
        config_file: Path | None = None  # Initialize config_file
        try:
            # Validate port
            if not (1 <= port <= 65535):
                raise ValueError(
                    f"Invalid port number: {port}. Must be between 1 and 65535."
                )

            self.paths.sites_available.mkdir(parents=True, exist_ok=True)

            # Determine root directory with OS-specific defaults
            if root_dir is None:
                match self._os:
                    case OperatingSystem.WINDOWS:
                        root_dir = f"C:/www/{server_name}"
                    case _:
                        root_dir = f"/var/www/{server_name}"

            config_file = self.paths.sites_available / f"{server_name}.conf"

            # Check if config already exists
            if config_file.exists():
                logger.warning(f"Virtual host config already exists: {config_file}")

            templates = self.plugins.get("vhost_templates", {})
            if template not in templates:
                available_templates = list(templates.keys())
                raise ConfigError(
                    f"Unknown template: {template}",
                    details={
                        "requested_template": template,
                        "available_templates": available_templates,
                    },
                )

            # Generate configuration content
            try:
                config_content = templates[template](
                    server_name=server_name,
                    port=port,
                    root_dir=root_dir,
                    paths=self.paths,
                )
            except Exception as e:
                raise ConfigError(f"Template generation failed: {e}") from e

            # Write configuration file
            config_file.write_text(config_content, encoding="utf-8")

            self._print_color(
                f"Virtual host '{server_name}' created.", OutputColors.GREEN
            )
            logger.success(f"Virtual host created: {config_file}")
            return config_file

        except (OSError, PermissionError) as e:
            raise ConfigError(
                f"Failed to create virtual host configuration: {e}",
                details={
                    "server_name": server_name,
                    "config_path": (
                        str(config_file) if "config_file" in locals() else "unknown"
                    ),
                },
            ) from e

    async def _enable_vhost(self, server_name: str, **_) -> None:
        """Enable a virtual host with cross-platform support."""
        source: Path | None = None  # Initialize source
        target: Path | None = None  # Initialize target
        try:
            source = self.paths.sites_available / f"{server_name}.conf"
            target = self.paths.sites_enabled / f"{server_name}.conf"

            if not source.exists():
                raise ConfigError(
                    f"Virtual host configuration not found: {source}",
                    details={
                        "server_name": server_name,
                        "expected_path": str(source),
                        "available_configs": [
                            f.stem for f in self.paths.sites_available.glob("*.conf")
                        ],
                    },
                )

            self.paths.sites_enabled.mkdir(parents=True, exist_ok=True)

            # Handle different platforms for enabling
            if target.exists():
                logger.info(f"Virtual host '{server_name}' is already enabled")
                return

            match self._os:
                case OperatingSystem.WINDOWS:
                    # On Windows, copy the file
                    shutil.copy2(source, target)
                case _:
                    # On Unix-like systems, create a symbolic link
                    target.symlink_to(f"../sites-available/{server_name}.conf")

            self._print_color(
                f"Virtual host '{server_name}' enabled.", OutputColors.GREEN
            )
            logger.success(f"Virtual host enabled: {server_name}")

            # Validate configuration after enabling
            await self.check_config()

        except (OSError, PermissionError) as e:
            raise ConfigError(
                f"Failed to enable virtual host: {e}",
                details={
                    "server_name": server_name,
                    "source": str(source),
                    "target": str(target),
                },
            ) from e

    async def _disable_vhost(self, server_name: str, **_) -> None:
        """Disable a virtual host with error handling."""
        target: Path | None = None  # Initialize target
        try:
            target = self.paths.sites_enabled / f"{server_name}.conf"

            if target.exists():
                target.unlink()
                self._print_color(
                    f"Virtual host '{server_name}' disabled.", OutputColors.GREEN
                )
                logger.success(f"Virtual host disabled: {server_name}")
            else:
                self._print_color(
                    f"Virtual host '{server_name}' is already disabled.",
                    OutputColors.YELLOW,
                )
                logger.info(f"Virtual host already disabled: {server_name}")

        except (OSError, PermissionError) as e:
            raise ConfigError(
                f"Failed to disable virtual host: {e}",
                details={"server_name": server_name, "target": str(target)},
            ) from e

    def list_virtual_hosts(self) -> dict[str, bool]:
        """List all virtual hosts and their status with error handling."""
        try:
            self.paths.sites_available.mkdir(exist_ok=True)
            self.paths.sites_enabled.mkdir(exist_ok=True)

            available = {f.stem for f in self.paths.sites_available.glob("*.conf")}
            enabled = {f.stem for f in self.paths.sites_enabled.glob("*.conf")}

            result = {host: host in enabled for host in available}
            logger.debug(
                f"Found {len(available)} virtual hosts, {len(enabled)} enabled"
            )
            return result

        except (OSError, PermissionError) as e:
            logger.warning(f"Error listing virtual hosts: {e}")
            return {}
        except Exception as e:
            logger.error(f"Unexpected error listing virtual hosts: {e}")
            return {}

    async def health_check(self) -> dict[str, Any]:
        """Perform a comprehensive health check with detailed error reporting."""
        logger.info("Starting comprehensive Nginx health check...")

        results: dict[str, Any] = {
            "installed": False,
            "running": False,
            "config_valid": False,
            "version": None,
            "virtual_hosts": 0,
            "errors": [],
            "warnings": [],
            "timestamp": datetime.datetime.now().isoformat(),
        }

        # Check installation
        try:
            results["installed"] = await self.is_nginx_installed()
            if not results["installed"]:
                results["errors"].append("Nginx is not installed")
                self._print_health_results(results)
                return results
        except Exception as e:
            results["errors"].append(f"Installation check failed: {e}")

        # If installed, perform additional checks
        if results["installed"]:
            # Version check
            try:
                version_output = await self.get_version()
                results["version"] = version_output
            except Exception as e:
                results["errors"].append(f"Version check failed: {e}")

            # Status check
            try:
                results["running"] = await self.get_status()
            except Exception as e:
                results["errors"].append(f"Status check failed: {e}")

            # Configuration validation
            try:
                results["config_valid"] = await self.check_config()
                if not results["config_valid"]:
                    results["warnings"].append("Configuration validation failed")
            except Exception as e:
                results["errors"].append(f"Config validation failed: {e}")

            # Virtual hosts count
            try:
                vhosts = self.list_virtual_hosts()
                results["virtual_hosts"] = len(vhosts)
                enabled_count = sum(1 for enabled in vhosts.values() if enabled)
                results["virtual_hosts_enabled"] = enabled_count

                if results["virtual_hosts"] > 0:
                    results["virtual_hosts_list"] = list(vhosts.keys())[
                        :10
                    ]  # Limit to first 10

            except Exception as e:
                results["errors"].append(f"Virtual hosts check failed: {e}")

            # Path validation
            try:
                missing_paths = []
                for path_name in ["base_path", "conf_path", "binary_path"]:
                    path = getattr(self.paths, path_name)
                    if not path.exists():
                        missing_paths.append(f"{path_name}: {path}")

                if missing_paths:
                    results["warnings"].extend(missing_paths)

            except Exception as e:
                results["errors"].append(f"Path validation failed: {e}")

        # Overall health assessment
        results["healthy"] = (
            results["installed"]
            and results["config_valid"]
            and len(results["errors"]) == 0
        )

        self._print_health_results(results)
        logger.info(f"Health check completed. Healthy: {results['healthy']}")
        return results

    def _print_health_results(self, results: dict[str, Any]) -> None:
        """Print formatted health check results."""
        self._print_color("\n=== Nginx Health Check Results ===", OutputColors.CYAN)

        status_items = [
            (
                "Installed",
                results["installed"],
                OutputColors.GREEN if results["installed"] else OutputColors.RED,
            ),
            (
                "Running",
                results["running"],
                OutputColors.GREEN if results["running"] else OutputColors.RED,
            ),
            (
                "Config Valid",
                results["config_valid"],
                OutputColors.GREEN if results["config_valid"] else OutputColors.RED,
            ),
        ]

        for label, value, color in status_items:
            self._print_color(f"  {label}: {value}", color)

        if results["version"]:
            self._print_color(f"  Version: {results['version']}", OutputColors.CYAN)

        if results["virtual_hosts"] > 0:
            enabled = results.get("virtual_hosts_enabled", 0)
            self._print_color(
                f"  Virtual Hosts: {results['virtual_hosts']} total, {enabled} enabled",
                OutputColors.BLUE,
            )

        if results["warnings"]:
            self._print_color("  Warnings:", OutputColors.YELLOW)
            for warning in results["warnings"]:
                self._print_color(f"    - {warning}", OutputColors.YELLOW)

        if results["errors"]:
            self._print_color("  Errors:", OutputColors.RED)
            for error in results["errors"]:
                self._print_color(f"    - {error}", OutputColors.RED)

        overall_color = (
            OutputColors.GREEN if results.get("healthy", False) else OutputColors.RED
        )
        overall_status = "HEALTHY" if results.get("healthy", False) else "UNHEALTHY"
        self._print_color(f"\nOverall Status: {overall_status}", overall_color)


# Modern virtual host templates with enhanced features
def basic_template(**kwargs) -> str:
    """Basic Nginx virtual host template with security headers."""
    server_name = kwargs["server_name"]
    port = kwargs["port"]
    root_dir = kwargs["root_dir"]
    logs_path = kwargs["paths"].logs_path

    return f"""server {{
    listen {port};
    server_name {server_name};
    root {root_dir};
    index index.html index.htm;

    # Security headers
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;

    location / {{
        try_files $uri $uri/ =404;
    }}

    # Deny access to hidden files
    location ~ /\\. {{
        deny all;
        access_log off;
        log_not_found off;
    }}

    # Logging
    access_log {logs_path}/{server_name}.access.log;
    error_log {logs_path}/{server_name}.error.log;
}}"""


def php_template(**kwargs) -> str:
    """PHP-enabled Nginx virtual host template with modern PHP-FPM configuration."""
    server_name = kwargs["server_name"]
    port = kwargs["port"]
    root_dir = kwargs["root_dir"]
    logs_path = kwargs["paths"].logs_path

    return f"""server {{
    listen {port};
    server_name {server_name};
    root {root_dir};
    index index.php index.html index.htm;

    # Security headers
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;

    location / {{
        try_files $uri $uri/ /index.php$is_args$args;
    }}

    # PHP processing
    location ~ \\.php$ {{
        try_files $uri =404;
        fastcgi_split_path_info ^(.+\\.php)(/.+)$;
        fastcgi_pass unix:/var/run/php/php-fpm.sock;
        fastcgi_index index.php;
        fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
        include fastcgi_params;

        # PHP security
        fastcgi_param PHP_VALUE "expose_php=0";
        fastcgi_hide_header X-Powered-By;
    }}

    # Deny access to hidden files and PHP files in uploads
    location ~ /\\. {{
        deny all;
        access_log off;
        log_not_found off;
    }}

    location ~* /uploads/.*\\.php$ {{
        deny all;
    }}

    # Static file caching
    location ~* \\.(jpg|jpeg|gif|png|css|js|ico|xml)$ {{
        expires 5d;
        add_header Cache-Control "public, immutable";
    }}

    # Logging
    access_log {logs_path}/{server_name}.access.log;
    error_log {logs_path}/{server_name}.error.log;
}}"""


def proxy_template(**kwargs) -> str:
    """Reverse proxy Nginx virtual host template with modern proxy settings."""
    server_name = kwargs["server_name"]
    port = kwargs["port"]
    logs_path = kwargs["paths"].logs_path
    upstream_host = kwargs.get("upstream_host", "localhost")
    upstream_port = kwargs.get("upstream_port", 8000)

    return f"""upstream {server_name}_backend {{
    server {upstream_host}:{upstream_port};
    keepalive 32;
}}

server {{
    listen {port};
    server_name {server_name};

    # Security headers
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;

    # Increase client body size for file uploads
    client_max_body_size 100M;

    location / {{
        proxy_pass http://{server_name}_backend;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Timeout settings
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;

        # Buffering
        proxy_buffering on;
        proxy_buffer_size 128k;
        proxy_buffers 4 256k;
        proxy_busy_buffers_size 256k;

        # Cache bypass for websockets
        proxy_cache_bypass $http_upgrade;
    }}

    # Health check endpoint
    location /nginx-health {{
        access_log off;
        return 200 "healthy\\n";
        add_header Content-Type text/plain;
    }}

    # Logging
    access_log {logs_path}/{server_name}.access.log;
    error_log {logs_path}/{server_name}.error.log;
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
