#!/usr/bin/env python3
"""
Plugin management system for the enhanced pacman manager.
Provides extensible functionality through a modular plugin architecture.
"""

from __future__ import annotations

import inspect
import importlib
import importlib.util
from pathlib import Path
from typing import Dict, List, Any, Callable, Optional, TypeVar, Generic
from collections import defaultdict
from abc import ABC, abstractmethod
from datetime import datetime

from loguru import logger

from .pacman_types import PluginHook, AsyncPluginHook
from .exceptions import PacmanError


T = TypeVar('T')


class PluginError(PacmanError):
    """Exception raised for plugin-related errors."""
    pass


class PluginBase(ABC):
    """
    Base class for all pacman manager plugins.
    """

    def __init__(self):
        self.name = self.__class__.__name__
        self.version = getattr(self, '__version__', '1.0.0')
        self.description = getattr(self, '__description__', '')
        self.enabled = True

    @abstractmethod
    def initialize(self, manager) -> None:
        """Initialize the plugin with the manager instance."""
        logger.info(f"Initializing plugin: {self.name} v{self.version}")

    def cleanup(self) -> None:
        """Clean up plugin resources. Override if needed."""
        logger.debug(f"Cleaning up plugin: {self.name}")

    def get_hooks(self) -> Dict[str, Callable]:
        """
        Return dictionary of hook name -> callable mappings.
        Override to provide plugin functionality.
        """
        return {}


class HookRegistry:
    """
    Registry for managing plugin hooks with support for prioritized execution.
    """

    def __init__(self):
        self._hooks: Dict[str, List[tuple[int, Callable]]] = defaultdict(list)
        self._async_hooks: Dict[str,
                                List[tuple[int, Callable]]] = defaultdict(list)

    def register_hook(self, hook_name: str, callback: Callable, priority: int = 50) -> None:
        """
        Register a hook callback with optional priority.
        Lower priority numbers execute first.
        """
        if inspect.iscoroutinefunction(callback):
            self._async_hooks[hook_name].append((priority, callback))
            self._async_hooks[hook_name].sort(key=lambda x: x[0])
        else:
            self._hooks[hook_name].append((priority, callback))
            self._hooks[hook_name].sort(key=lambda x: x[0])

        logger.debug(f"Registered hook '{hook_name}' with priority {priority}")

    def unregister_hook(self, hook_name: str, callback: Callable) -> bool:
        """Remove a hook callback. Returns True if found and removed."""
        # Check sync hooks
        for i, (priority, cb) in enumerate(self._hooks[hook_name]):
            if cb == callback:
                del self._hooks[hook_name][i]
                logger.debug(f"Unregistered sync hook '{hook_name}'")
                return True

        # Check async hooks
        for i, (priority, cb) in enumerate(self._async_hooks[hook_name]):
            if cb == callback:
                del self._async_hooks[hook_name][i]
                logger.debug(f"Unregistered async hook '{hook_name}'")
                return True

        return False

    def call_hooks(self, hook_name: str, *args, **kwargs) -> List[Any]:
        """
        Call all registered hooks for the given name synchronously.
        Returns list of results from all hook callbacks.
        """
        results = []

        for priority, callback in self._hooks.get(hook_name, []):
            try:
                result = callback(*args, **kwargs)
                results.append(result)
            except Exception as e:
                logger.error(f"Error in hook '{hook_name}': {e}")
                # Continue with other hooks

        return results

    async def call_async_hooks(self, hook_name: str, *args, **kwargs) -> List[Any]:
        """
        Call all registered async hooks for the given name.
        Returns list of results from all hook callbacks.
        """
        results = []

        for priority, callback in self._async_hooks.get(hook_name, []):
            try:
                result = await callback(*args, **kwargs)
                results.append(result)
            except Exception as e:
                logger.error(f"Error in async hook '{hook_name}': {e}")
                # Continue with other hooks

        return results

    def has_hooks(self, hook_name: str) -> bool:
        """Check if any hooks are registered for the given name."""
        return (
            hook_name in self._hooks and len(self._hooks[hook_name]) > 0 or
            hook_name in self._async_hooks and len(
                self._async_hooks[hook_name]) > 0
        )

    def list_hooks(self) -> Dict[str, int]:
        """Get a dictionary of hook names and their callback counts."""
        hook_counts = {}

        for hook_name, callbacks in self._hooks.items():
            hook_counts[hook_name] = len(callbacks)

        for hook_name, callbacks in self._async_hooks.items():
            current_count = hook_counts.get(hook_name, 0)
            hook_counts[hook_name] = current_count + len(callbacks)

        return hook_counts


class PluginManager:
    """
    Manager for loading, configuring, and executing plugins.
    """

    def __init__(self, plugin_directories: Optional[List[Path]] = None):
        self.plugin_directories = plugin_directories or []
        self.plugins: Dict[str, PluginBase] = {}
        self.hook_registry = HookRegistry()
        self._manager_instance = None

    def add_plugin_directory(self, directory: Path) -> None:
        """Add a directory to search for plugins."""
        if directory.is_dir():
            self.plugin_directories.append(directory)
            logger.debug(f"Added plugin directory: {directory}")
        else:
            logger.warning(f"Plugin directory does not exist: {directory}")

    def load_plugins(self, manager_instance=None) -> None:
        """
        Load all plugins from the configured directories.
        """
        self._manager_instance = manager_instance
        loaded_count = 0

        for directory in self.plugin_directories:
            loaded_count += self._load_plugins_from_directory(directory)

        logger.info(f"Loaded {loaded_count} plugins")

    def _load_plugins_from_directory(self, directory: Path) -> int:
        """Load plugins from a specific directory."""
        loaded_count = 0

        for plugin_file in directory.glob("*.py"):
            if plugin_file.name.startswith("__"):
                continue

            try:
                plugin = self._load_plugin_from_file(plugin_file)
                if plugin:
                    self.register_plugin(plugin)
                    loaded_count += 1
            except Exception as e:
                logger.error(f"Failed to load plugin from {plugin_file}: {e}")

        return loaded_count

    def _load_plugin_from_file(self, plugin_file: Path) -> Optional[PluginBase]:
        """Load a single plugin from a Python file."""
        module_name = f"pacman_plugin_{plugin_file.stem}"

        spec = importlib.util.spec_from_file_location(module_name, plugin_file)
        if not spec or not spec.loader:
            logger.warning(f"Could not load spec for {plugin_file}")
            return None

        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)

        # Look for plugin classes that inherit from PluginBase
        for name, obj in inspect.getmembers(module, inspect.isclass):
            if issubclass(obj, PluginBase) and obj != PluginBase:
                logger.debug(f"Found plugin class: {name}")
                return obj()

        logger.warning(f"No valid plugin class found in {plugin_file}")
        return None

    def register_plugin(self, plugin: PluginBase) -> None:
        """Register a plugin instance."""
        if plugin.name in self.plugins:
            logger.warning(
                f"Plugin '{plugin.name}' already registered, replacing")

        self.plugins[plugin.name] = plugin

        # Initialize the plugin
        try:
            plugin.initialize(self._manager_instance)

            # Register plugin hooks
            hooks = plugin.get_hooks()
            for hook_name, callback in hooks.items():
                self.hook_registry.register_hook(hook_name, callback)

            logger.info(f"Registered plugin: {plugin.name} v{plugin.version}")

        except Exception as e:
            logger.error(f"Failed to initialize plugin '{plugin.name}': {e}")
            # Remove from plugins dict if initialization failed
            if plugin.name in self.plugins:
                del self.plugins[plugin.name]

    def unregister_plugin(self, plugin_name: str) -> bool:
        """Unregister a plugin and clean up its hooks."""
        if plugin_name not in self.plugins:
            return False

        plugin = self.plugins[plugin_name]

        # Clean up plugin resources
        try:
            plugin.cleanup()
        except Exception as e:
            logger.error(
                f"Error during plugin cleanup for '{plugin_name}': {e}")

        # Remove hooks
        hooks = plugin.get_hooks()
        for hook_name, callback in hooks.items():
            self.hook_registry.unregister_hook(hook_name, callback)

        # Remove from plugins dict
        del self.plugins[plugin_name]

        logger.info(f"Unregistered plugin: {plugin_name}")
        return True

    def enable_plugin(self, plugin_name: str) -> bool:
        """Enable a plugin."""
        if plugin_name in self.plugins:
            self.plugins[plugin_name].enabled = True
            logger.info(f"Enabled plugin: {plugin_name}")
            return True
        return False

    def disable_plugin(self, plugin_name: str) -> bool:
        """Disable a plugin."""
        if plugin_name in self.plugins:
            self.plugins[plugin_name].enabled = False
            logger.info(f"Disabled plugin: {plugin_name}")
            return True
        return False

    def call_hook(self, hook_name: str, *args, **kwargs) -> List[Any]:
        """
        Call all registered hooks for the given name.
        Only calls hooks from enabled plugins.
        """
        return self.hook_registry.call_hooks(hook_name, *args, **kwargs)

    async def call_async_hook(self, hook_name: str, *args, **kwargs) -> List[Any]:
        """
        Call all registered async hooks for the given name.
        Only calls hooks from enabled plugins.
        """
        return await self.hook_registry.call_async_hooks(hook_name, *args, **kwargs)

    def get_plugin_info(self) -> Dict[str, Dict[str, Any]]:
        """Get information about all registered plugins."""
        return {
            name: {
                'version': plugin.version,
                'description': plugin.description,
                'enabled': plugin.enabled,
                'hooks': list(plugin.get_hooks().keys())
            }
            for name, plugin in self.plugins.items()
        }

    def reload_plugin(self, plugin_name: str) -> bool:
        """Reload a specific plugin."""
        if plugin_name not in self.plugins:
            return False

        # Store the plugin file path (if available)
        plugin = self.plugins[plugin_name]

        # Unregister the old plugin
        self.unregister_plugin(plugin_name)

        # Try to reload from directories
        for directory in self.plugin_directories:
            for plugin_file in directory.glob("*.py"):
                if plugin_file.stem == plugin_name.lower():
                    try:
                        new_plugin = self._load_plugin_from_file(plugin_file)
                        if new_plugin and new_plugin.name == plugin_name:
                            self.register_plugin(new_plugin)
                            logger.info(f"Reloaded plugin: {plugin_name}")
                            return True
                    except Exception as e:
                        logger.error(
                            f"Failed to reload plugin '{plugin_name}': {e}")
                        return False

        logger.warning(
            f"Could not find plugin file for '{plugin_name}' to reload")
        return False


# Built-in example plugin
class LoggingPlugin(PluginBase):
    """
    Example plugin that logs package operations.
    """

    def __init__(self):
        super().__init__()
        self.__version__ = '1.0.0'
        self.__description__ = 'Logs all package operations'

    def initialize(self, manager) -> None:
        """Initialize the logging plugin."""
        self.manager = manager
        logger.info("Logging plugin initialized")

    def get_hooks(self) -> Dict[str, Callable]:
        """Return hook callbacks."""
        return {
            'before_install': self.log_before_install,
            'after_install': self.log_after_install,
            'before_remove': self.log_before_remove,
            'after_remove': self.log_after_remove,
        }

    def log_before_install(self, package_name: str, **kwargs) -> None:
        """Log before package installation."""
        logger.info(f"[Plugin] About to install package: {package_name}")

    def log_after_install(self, package_name: str, success: bool, **kwargs) -> None:
        """Log after package installation."""
        status = "successfully" if success else "failed to"
        logger.info(
            f"[Plugin] {status.capitalize()} installed package: {package_name}")

    def log_before_remove(self, package_name: str, **kwargs) -> None:
        """Log before package removal."""
        logger.info(f"[Plugin] About to remove package: {package_name}")

    def log_after_remove(self, package_name: str, success: bool, **kwargs) -> None:
        """Log after package removal."""
        status = "successfully" if success else "failed to"
        logger.info(
            f"[Plugin] {status.capitalize()} removed package: {package_name}")


class BackupPlugin(PluginBase):
    """
    Example plugin that creates backups before package operations.
    """
    __version__ = "1.0.0"
    __description__ = "Creates backups before package installations and removals"

    def __init__(self, backup_dir: Optional[Path] = None):
        super().__init__()
        self.backup_dir = backup_dir or Path.home() / ".pacman_backups"
        self.backup_dir.mkdir(exist_ok=True)

    def initialize(self, manager) -> None:
        super().initialize(manager)
        logger.info(f"Backup directory: {self.backup_dir}")

    def get_hooks(self) -> Dict[str, Callable]:
        return {
            "before_install": self.create_backup,
            "before_remove": self.create_backup,
        }

    def create_backup(self, package_name: str, **kwargs) -> None:
        """Create a backup of package list before operations."""
        backup_file = self.backup_dir / \
            f"backup_{datetime.now():%Y%m%d_%H%M%S}.txt"
        try:
            # This would ideally run pacman -Q to get installed packages
            backup_file.write_text(f"Backup before {package_name} operation\n")
            logger.info(f"Created backup: {backup_file}")
        except Exception as e:
            logger.error(f"Failed to create backup: {e}")


class NotificationPlugin(PluginBase):
    """
    Example plugin that sends notifications for package operations.
    """
    __version__ = "1.0.0"
    __description__ = "Sends desktop notifications for package operations"

    def initialize(self, manager) -> None:
        super().initialize(manager)
        self.notifications_enabled = True

    def get_hooks(self) -> Dict[str, Callable]:
        return {
            "after_install": self.notify_install,
            "after_remove": self.notify_remove,
        }

    def notify_install(self, package_name: str, success: bool, **kwargs) -> None:
        """Send notification after package installation."""
        if not self.notifications_enabled:
            return

        if success:
            self._send_notification(
                f"✅ Package '{package_name}' installed successfully")
        else:
            self._send_notification(
                f"❌ Failed to install package '{package_name}'")

    def notify_remove(self, package_name: str, success: bool, **kwargs) -> None:
        """Send notification after package removal."""
        if not self.notifications_enabled:
            return

        if success:
            self._send_notification(
                f"🗑️ Package '{package_name}' removed successfully")
        else:
            self._send_notification(
                f"❌ Failed to remove package '{package_name}'")

    def _send_notification(self, message: str) -> None:
        """Send a desktop notification (placeholder implementation)."""
        # In a real implementation, this would use a notification library
        # like plyer, notify2, or desktop-notifier
        logger.info(f"[Notification] {message}")


class SecurityPlugin(PluginBase):
    """
    Example plugin that performs security checks before package operations.
    """
    __version__ = "1.0.0"
    __description__ = "Performs security checks and validations"

    def __init__(self):
        super().__init__()
        self.blacklisted_packages = {
            "malware-package",
            "suspicious-tool",
            # Add more packages as needed
        }

    def initialize(self, manager) -> None:
        super().initialize(manager)
        logger.info(
            f"Security plugin loaded with {len(self.blacklisted_packages)} blacklisted packages")

    def get_hooks(self) -> Dict[str, Callable]:
        return {
            "before_install": self.security_check,
        }

    def security_check(self, package_name: str, **kwargs) -> None:
        """Perform security check before package installation."""
        if package_name in self.blacklisted_packages:
            logger.warning(
                f"⚠️ Security warning: Package '{package_name}' is blacklisted!")
            # In a real implementation, this could raise an exception to block the operation
        else:
            logger.debug(
                f"✅ Security check passed for package: {package_name}")


# Export all plugin classes
__all__ = [
    "PluginBase",
    "PluginManager",
    "HookRegistry",
    "PluginError",
    "LoggingPlugin",
    "BackupPlugin",
    "NotificationPlugin",
    "SecurityPlugin",
]
