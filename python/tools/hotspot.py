#!/usr/bin/env python3
"""
WiFi Hotspot Manager

A comprehensive utility for managing WiFi hotspots on Linux systems using NetworkManager.
Supports both command-line usage and programmatic API calls through Python or C++ (via pybind11).

Features:
- Create and manage WiFi hotspots with various authentication options
- Monitor connected clients
- Save and load hotspot configurations
- Extensive error handling and logging
"""

import subprocess
import argparse
import logging
import json
import asyncio
import sys
from pathlib import Path
from dataclasses import dataclass, asdict, field
from typing import Optional, List, Dict, Any, Callable, Union, Tuple
from enum import Enum
import time
import shutil
import re
import ipaddress


class AuthenticationType(Enum):
    """
    Authentication types supported for WiFi hotspots.

    Each type represents a different security protocol that can be used
    to secure the hotspot connection.
    """
    WPA_PSK = "wpa-psk"    # WPA Personal
    WPA2_PSK = "wpa2-psk"  # WPA2 Personal
    WPA3_SAE = "wpa3-sae"  # WPA3 Personal with SAE
    NONE = "none"          # Open network (no authentication)


class EncryptionType(Enum):
    """
    Encryption algorithms for securing WiFi traffic.

    These encryption methods are used to protect data transmitted over
    the wireless network.
    """
    AES = "aes"    # Advanced Encryption Standard
    TKIP = "tkip"  # Temporal Key Integrity Protocol
    CCMP = "ccmp"  # Counter Mode with CBC-MAC Protocol (AES-based)


class BandType(Enum):
    """
    WiFi frequency bands that can be used for the hotspot.

    Different bands offer different ranges and speeds.
    """
    G_ONLY = "bg"   # 2.4 GHz band
    A_ONLY = "a"    # 5 GHz band
    DUAL = "any"    # Both bands


@dataclass
class HotspotConfig:
    """
    Configuration parameters for a WiFi hotspot.

    This class stores all settings needed to create and manage a WiFi hotspot,
    with reasonable defaults for common scenarios.
    """
    name: str = "MyHotspot"
    password: Optional[str] = None
    authentication: AuthenticationType = AuthenticationType.WPA_PSK
    encryption: EncryptionType = EncryptionType.AES
    channel: int = 11
    max_clients: int = 10
    interface: str = "wlan0"
    band: BandType = BandType.G_ONLY
    hidden: bool = False

    def to_dict(self) -> Dict[str, Any]:
        """Convert configuration to a dictionary for serialization."""
        result = asdict(self)
        # Convert enum objects to their string values
        result["authentication"] = self.authentication.value
        result["encryption"] = self.encryption.value
        result["band"] = self.band.value
        return result

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "HotspotConfig":
        """Create a configuration object from a dictionary."""
        # Convert string values to enum objects
        if "authentication" in data:
            data["authentication"] = AuthenticationType(data["authentication"])
        if "encryption" in data:
            data["encryption"] = EncryptionType(data["encryption"])
        if "band" in data:
            data["band"] = BandType(data["band"])
        return cls(**data)


@dataclass
class CommandResult:
    """
    Result of a command execution.

    This class standardizes command execution returns with fields for stdout,
    stderr, success status, and the original command executed.
    """
    success: bool
    stdout: str = ""
    stderr: str = ""
    return_code: int = 0
    command: List[str] = field(default_factory=list)

    @property
    def output(self) -> str:
        """Get combined output (stdout + stderr)."""
        return f"{self.stdout}\n{self.stderr}".strip()


@dataclass
class ConnectedClient:
    """Information about a client connected to the hotspot."""
    mac_address: str
    ip_address: Optional[str] = None
    hostname: Optional[str] = None
    connected_since: Optional[float] = None

    @property
    def connection_duration(self) -> float:
        """Calculate how long the client has been connected in seconds."""
        if self.connected_since is None:
            return 0
        return time.time() - self.connected_since


class HotspotManager:
    """
    Manages WiFi hotspots using NetworkManager.

    This class provides a comprehensive interface to create, modify, and monitor
    WiFi hotspots through NetworkManager's command-line tools.
    """

    def __init__(self, config_dir: Optional[Path] = None, log_level: int = logging.INFO):
        """
        Initialize the HotspotManager.

        Args:
            config_dir: Directory to store configuration files. If None, uses ~/.config/hotspot-manager
            log_level: Logging level from the logging module
        """
        # Set up logging
        self.logger = logging.getLogger("HotspotManager")
        self.logger.setLevel(log_level)

        # Add console handler if not already configured
        if not self.logger.handlers:
            handler = logging.StreamHandler()
            formatter = logging.Formatter(
                '%(asctime)s - %(name)s - %(levelname)s - %(message)s')
            handler.setFormatter(formatter)
            self.logger.addHandler(handler)

        # Set up configuration directory
        if config_dir is None:
            self.config_dir = Path.home() / ".config" / "hotspot-manager"
        else:
            self.config_dir = Path(config_dir)

        self.config_dir.mkdir(parents=True, exist_ok=True)
        self.config_file = self.config_dir / "config.json"

        # Verify NetworkManager availability
        if not self._is_network_manager_available():
            self.logger.warning(
                "NetworkManager is not available on this system")

        # Initialize with default configuration
        self.current_config = HotspotConfig()

        # Try to load saved config if available
        if self.config_file.exists():
            try:
                self.load_config()
                self.logger.debug(
                    f"Loaded configuration from {self.config_file}")
            except Exception as e:
                self.logger.error(f"Failed to load configuration: {e}")

    def _is_network_manager_available(self) -> bool:
        """
        Check if NetworkManager is available on the system.

        Returns:
            True if NetworkManager is installed and available
        """
        return shutil.which("nmcli") is not None

    async def _run_command_async(self, cmd: List[str]) -> CommandResult:
        """
        Run a command asynchronously and return the result.

        Args:
            cmd: List of command parts to execute

        Returns:
            CommandResult object containing the command output and status
        """
        self.logger.debug(f"Running command asynchronously: {' '.join(cmd)}")
        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                text=False  # Changed from True to False to match expected type
            )
            stdout_bytes, stderr_bytes = await process.communicate()

            # Decode bytes to strings
            stdout = stdout_bytes.decode('utf-8') if stdout_bytes else ""
            stderr = stderr_bytes.decode('utf-8') if stderr_bytes else ""

            success = process.returncode == 0

            if not success:
                self.logger.error(f"Command failed: {' '.join(cmd)}")
                self.logger.error(f"Error: {stderr}")

            return CommandResult(
                success=success,
                stdout=stdout,
                stderr=stderr,
                return_code=process.returncode if process.returncode is not None else -1,
                command=cmd
            )
        except Exception as e:
            self.logger.exception(f"Exception running command: {e}")
            return CommandResult(
                success=False,
                stderr=str(e),
                command=cmd
            )

    def _run_command(self, cmd: List[str]) -> CommandResult:
        """
        Run a command synchronously and return the result.

        Args:
            cmd: List of command parts to execute

        Returns:
            CommandResult object containing the command output and status
        """
        self.logger.debug(f"Running command: {' '.join(cmd)}")
        try:
            result = subprocess.run(
                cmd,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            success = result.returncode == 0

            if not success:
                self.logger.error(f"Command failed: {' '.join(cmd)}")
                self.logger.error(f"Error: {result.stderr}")

            return CommandResult(
                success=success,
                stdout=result.stdout,
                stderr=result.stderr,
                return_code=result.returncode,
                command=cmd
            )
        except Exception as e:
            self.logger.exception(f"Exception running command: {e}")
            return CommandResult(
                success=False,
                stderr=str(e),
                command=cmd
            )

    def save_config(self) -> bool:
        """
        Save the current configuration to disk.

        Returns:
            True if the configuration was successfully saved
        """
        try:
            # Create config directory if it doesn't exist
            self.config_dir.mkdir(parents=True, exist_ok=True)

            # Write config to file in JSON format
            with open(self.config_file, 'w') as f:
                json.dump(self.current_config.to_dict(), f, indent=2)
            return True
        except Exception as e:
            self.logger.error(f"Error saving configuration: {e}")
            return False

    def load_config(self) -> bool:
        """
        Load configuration from disk.

        Returns:
            True if the configuration was successfully loaded
        """
        try:
            with open(self.config_file, 'r') as f:
                config_dict = json.load(f)
            self.current_config = HotspotConfig.from_dict(config_dict)
            return True
        except Exception as e:
            self.logger.error(f"Error loading configuration: {e}")
            return False

    def update_config(self, **kwargs) -> None:
        """
        Update configuration with provided parameters.

        Args:
            **kwargs: Configuration parameters to update
        """
        # Update only parameters that exist in the config class
        for key, value in kwargs.items():
            if hasattr(self.current_config, key):
                setattr(self.current_config, key, value)
                self.logger.debug(f"Updated config: {key} = {value}")
            else:
                self.logger.warning(f"Unknown configuration parameter: {key}")

    def start(self, **kwargs) -> bool:
        """
        Start a WiFi hotspot with the current or provided configuration.

        Args:
            **kwargs: Configuration parameters to override for this operation

        Returns:
            True if the hotspot was successfully started
        """
        # Update config with any provided parameters
        if kwargs:
            self.update_config(**kwargs)

        # Validate configuration
        if self.current_config.authentication != AuthenticationType.NONE:
            if self.current_config.password is None or len(self.current_config.password) < 8:
                self.logger.error(
                    "Password is required and must be at least 8 characters")
                return False

        # Start hotspot with basic parameters
        cmd = [
            'nmcli', 'dev', 'wifi', 'hotspot',
            'ifname', self.current_config.interface,
            'ssid', self.current_config.name
        ]

        # Add password if authentication is enabled
        if self.current_config.authentication != AuthenticationType.NONE and self.current_config.password is not None:
            cmd.extend(['password', self.current_config.password])

        result = self._run_command(cmd)

        if not result.success:
            return False

        # Configure additional settings after basic setup succeeds
        self._configure_hotspot()

        self.logger.info(
            f"Hotspot '{self.current_config.name}' is now running")

        # Save the configuration for future use
        self.save_config()
        return True

    def _configure_hotspot(self) -> None:
        """
        Configure advanced hotspot settings after initial creation.

        This applies settings like authentication type, encryption, channel,
        and other parameters that can't be set during the initial hotspot creation.
        """
        # Set authentication method
        self._run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless-security.key-mgmt',
            self.current_config.authentication.value
        ])

        # Set encryption for data protection
        self._run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless-security.pairwise',
            self.current_config.encryption.value
        ])

        self._run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless-security.group',
            self.current_config.encryption.value
        ])

        # Set frequency band (2.4GHz, 5GHz, or both)
        self._run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless.band',
            self.current_config.band.value
        ])

        # Set channel for broadcasting
        self._run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless.channel',
            str(self.current_config.channel)
        ])

        # Set MAC address behavior for consistent identification
        self._run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless.cloned-mac-address', 'stable'
        ])

        self._run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless.mac-address-randomization', 'no'
        ])

        # Set hidden network status if configured
        if self.current_config.hidden:
            self._run_command([
                'nmcli', 'connection', 'modify', 'Hotspot',
                '802-11-wireless.hidden', 'yes'
            ])

    def stop(self) -> bool:
        """
        Stop the currently running hotspot.

        Returns:
            True if the hotspot was successfully stopped
        """
        result = self._run_command(['nmcli', 'connection', 'down', 'Hotspot'])
        if result.success:
            self.logger.info("Hotspot has been stopped")
        return result.success

    def get_status(self) -> Dict[str, Any]:
        """
        Get the current status of the hotspot.

        Returns:
            Dictionary containing status information including running state,
            interface, SSID, connected clients, uptime, and IP address
        """
        # Initialize status dictionary with default values
        status = {
            "running": False,
            "interface": None,
            "connection_name": None,
            "ssid": None,
            "clients": [],
            "uptime": None,
            "ip_address": None
        }

        # Check if hotspot is running by getting device status
        dev_status = self._run_command(['nmcli', 'dev', 'status'])
        if not dev_status.success:
            return status

        # Parse output to find hotspot interface
        for line in dev_status.stdout.splitlines()[1:]:  # Skip header line
            parts = [p.strip() for p in line.split()]
            if len(parts) >= 3 and parts[1] == "wifi" and "Hotspot" in line:
                status["running"] = True
                status["interface"] = parts[0]
                break

        if not status["running"]:
            return status

        # Get detailed connection information
        conn_details = self._run_command(
            ['nmcli', 'connection', 'show', 'Hotspot'])
        if conn_details.success:
            # Extract relevant details from connection info
            for line in conn_details.stdout.splitlines():
                if "802-11-wireless.ssid:" in line:
                    status["ssid"] = line.split(":", 1)[1].strip()
                elif "GENERAL.NAME:" in line:
                    status["connection_name"] = line.split(":", 1)[1].strip()
                elif "GENERAL.DEVICES:" in line:
                    status["interface"] = line.split(":", 1)[1].strip()
                elif "IP4.ADDRESS" in line:
                    # Extract IP address
                    ip_info = line.split(":", 1)[1].strip()
                    if "/" in ip_info:
                        status["ip_address"] = ip_info.split("/")[0]

        # Get uptime information
        uptime_cmd = self._run_command([
            'nmcli', '-t', '-f', 'GENERAL.STATE-TIMESTAMP',
            'connection', 'show', 'Hotspot'
        ])
        if uptime_cmd.success:
            # Extract timestamp and calculate uptime
            try:
                for line in uptime_cmd.stdout.splitlines():
                    if "GENERAL.STATE-TIMESTAMP:" in line:
                        timestamp = int(line.split(":", 1)[1].strip())
                        status["uptime"] = int(time.time() - timestamp / 1000)
                        break
            except (ValueError, IndexError):
                pass

        # Get connected clients information
        status["clients"] = self.get_connected_clients()

        return status

    def status(self) -> None:
        """
        Print the current status of the hotspot to the console.

        This is a user-friendly version of get_status() that formats
        the status information for display.
        """
        status = self.get_status()

        if status["running"]:
            print(f"Hotspot is running on interface {status['interface']}")
            print(f"SSID: {status['ssid']}")

            # Format uptime in a human-readable way
            if status["uptime"] is not None:
                hours, remainder = divmod(status["uptime"], 3600)
                minutes, seconds = divmod(remainder, 60)
                print(f"Uptime: {hours}h {minutes}m {seconds}s")

            print(f"IP Address: {status['ip_address']}")

            # Show connected clients
            if status["clients"]:
                print(f"\n**Connected clients** ({len(status['clients'])}):")
                for client in status["clients"]:
                    hostname = f" ({client.get('hostname')})" if client.get(
                        'hostname') else ""
                    print(
                        f"- {client['mac_address']} ({client['ip_address']}){hostname}")
            else:
                print("\nNo clients connected")
        else:
            print("**Hotspot is not running**")

    def list(self) -> List[Dict[str, str]]:
        """
        List all active network connections.

        Returns:
            List of dictionaries containing connection information
        """
        result = self._run_command(['nmcli', 'connection', 'show', '--active'])
        connections = []

        if result.success:
            lines = result.stdout.strip().split('\n')
            if len(lines) > 1:  # Skip the header line
                for line in lines[1:]:
                    parts = line.split()
                    if len(parts) >= 4:
                        connection = {
                            "name": parts[0],
                            "uuid": parts[1],
                            "type": parts[2],
                            "device": parts[3]
                        }
                        connections.append(connection)

            # Also print output for CLI usage
            print(result.stdout)

        return connections

    def set(self, **kwargs) -> bool:
        """
        Update the hotspot configuration.

        Args:
            **kwargs: Configuration parameters to update

        Returns:
            True if the configuration was successfully updated
        """
        self.update_config(**kwargs)

        # If hotspot is already running, apply changes immediately
        status = self.get_status()
        if status["running"]:
            self._configure_hotspot()
            self.logger.info(f"Updated running hotspot configuration")

        # Save the configuration for future use
        self.save_config()
        self.logger.info(f"Hotspot configuration updated and saved")
        return True

    def get_connected_clients(self) -> List[Dict[str, str]]:
        """
        Get information about clients connected to the hotspot.

        Uses multiple methods to gather client information, combining
        data from iw, arp, and DHCP leases.

        Returns:
            List of dictionaries containing client information
        """
        clients = []

        # Check if hotspot is running first
        status = self.get_status()
        if not status["running"]:
            return clients

        # METHOD 1: Use 'iw' command to list stations
        if status["interface"]:
            iw_cmd = self._run_command([
                'iw', 'dev', status["interface"], 'station', 'dump'
            ])

            if iw_cmd.success:
                # Parse iw output to extract client MAC addresses and connection times
                current_mac = None
                for line in iw_cmd.stdout.splitlines():
                    line = line.strip()
                    if line.startswith("Station"):
                        current_mac = line.split()[1]
                        clients.append({
                            "mac_address": current_mac,
                            "ip_address": "Unknown",
                            "connected_since": None
                        })
                    elif "connected time:" in line and current_mac:
                        # Extract connected time in seconds
                        try:
                            time_str = line.split(":", 1)[1].strip()
                            if "seconds" in time_str:
                                seconds = int(time_str.split()[0])
                                # Update the client that matches this MAC
                                for client in clients:
                                    if client["mac_address"] == current_mac:
                                        client["connected_since"] = int(
                                            time.time() - seconds)
                                        break
                        except (ValueError, IndexError):
                            pass

        # METHOD 2: Use the ARP table to match MACs with IP addresses
        arp_cmd = self._run_command(['arp', '-n'])
        if arp_cmd.success:
            for line in arp_cmd.stdout.splitlines()[1:]:  # Skip header
                parts = line.split()
                if len(parts) >= 3:
                    ip = parts[0]
                    mac = parts[2]
                    # Check if this MAC is in our clients list
                    for client in clients:
                        if client["mac_address"].lower() == mac.lower():
                            client["ip_address"] = ip
                            break

        # METHOD 3: Try to get hostnames from DHCP leases if available
        leases_file = Path("/var/lib/misc/dnsmasq.leases")
        if leases_file.exists():
            try:
                with open(leases_file, 'r') as f:
                    for line in f:
                        parts = line.split()
                        if len(parts) >= 5:
                            mac = parts[1]
                            ip = parts[2]
                            hostname = parts[3]
                            # Check if this MAC is in our clients list
                            for client in clients:
                                if client["mac_address"].lower() == mac.lower():
                                    client["ip_address"] = ip
                                    if hostname != "*":
                                        client["hostname"] = hostname
                                    break
            except Exception as e:
                self.logger.error(f"Error reading DHCP leases: {e}")

        return clients

    def get_network_interfaces(self) -> List[Dict[str, Any]]:
        """
        Get a list of available network interfaces.

        Returns:
            List of dictionaries with interface information
        """
        interfaces = []

        # Get list of interfaces using nmcli
        result = self._run_command(['nmcli', 'device', 'status'])
        if result.success:
            lines = result.stdout.strip().split('\n')
            if len(lines) > 1:  # Skip the header line
                for line in lines[1:]:
                    parts = line.split()
                    if len(parts) >= 3:
                        interface = {
                            "name": parts[0],
                            "type": parts[1],
                            "state": parts[2],
                            "connection": parts[3] if len(parts) > 3 else "Unknown"
                        }
                        interfaces.append(interface)

        return interfaces

    def get_available_channels(self, interface: Optional[str] = None) -> List[int]:
        """
        Get a list of available WiFi channels for the specified interface.

        Args:
            interface: Network interface to check (uses current config if None)

        Returns:
            List of available channel numbers
        """
        if interface is None:
            interface = self.current_config.interface

        channels = []

        # Get channel info using iwlist
        result = self._run_command(['iwlist', interface, 'channel'])
        if result.success:
            # Parse channel list from output
            channel_pattern = re.compile(r"Channel\s+(\d+)\s+:")
            for line in result.stdout.strip().split('\n'):
                match = channel_pattern.search(line)
                if match:
                    channels.append(int(match.group(1)))

        return channels

    def restart(self, **kwargs) -> bool:
        """
        Restart the hotspot with new configuration.

        Args:
            **kwargs: Configuration parameters to update

        Returns:
            True if the hotspot was successfully restarted
        """
        # Update config if parameters provided
        if kwargs:
            self.update_config(**kwargs)

        # First stop the hotspot
        if not self.stop():
            self.logger.error("Failed to stop hotspot for restart")
            return False

        # Brief pause to ensure interface is released
        time.sleep(1)

        # Start the hotspot with updated config
        return self.start()

    async def monitor_clients(self, interval: int = 5, callback: Optional[Callable[[List[Dict[str, Any]]], None]] = None) -> None:
        """
        Monitor clients connected to the hotspot in real-time.

        Args:
            interval: Time in seconds between checks
            callback: Optional function to call with client list on each update
        """
        try:
            previous_clients = set()

            while True:
                clients = self.get_connected_clients()
                current_clients = {client["mac_address"] for client in clients}

                # Detect new and disconnected clients
                new_clients = current_clients - previous_clients
                disconnected_clients = previous_clients - current_clients

                # Log connection changes
                for mac in new_clients:
                    self.logger.info(f"New client connected: {mac}")

                for mac in disconnected_clients:
                    self.logger.info(f"Client disconnected: {mac}")

                # Call the callback if provided
                if callback:
                    callback(clients)
                else:
                    # Default behavior: print client info
                    if clients:
                        print(f"\n{len(clients)} clients connected:")
                        for client in clients:
                            hostname = f" ({client['hostname']})" if 'hostname' in client and client['hostname'] else ""
                            print(
                                f"- {client['mac_address']} ({client.get('ip_address', 'Unknown IP')}){hostname}")
                    else:
                        print("\nNo clients connected")

                # Update previous clients list for next iteration
                previous_clients = current_clients

                await asyncio.sleep(interval)

        except asyncio.CancelledError:
            self.logger.info("Client monitoring stopped")
        except Exception as e:
            self.logger.error(f"Error monitoring clients: {e}")


# Function to create a pybind11 module
def create_pybind11_module():
    """
    Create the core functions and classes for pybind11 integration.

    Returns:
        A dictionary containing the classes and functions to expose via pybind11
    """
    return {
        "HotspotManager": HotspotManager,
        "HotspotConfig": HotspotConfig,
        "AuthenticationType": AuthenticationType,
        "EncryptionType": EncryptionType,
        "BandType": BandType,
        "CommandResult": CommandResult,
    }


def main():
    """
    Main entry point for command-line usage.

    Parses command-line arguments and executes the requested action.
    """
    parser = argparse.ArgumentParser(
        description='Advanced WiFi Hotspot Manager')
    subparsers = parser.add_subparsers(dest='action', help='Action to perform')

    # Start command
    start_parser = subparsers.add_parser('start', help='Start a WiFi hotspot')
    start_parser.add_argument('--name', help='Hotspot name')
    start_parser.add_argument('--password', help='Hotspot password')
    start_parser.add_argument('--authentication',
                              choices=[t.value for t in AuthenticationType],
                              help='Authentication type')
    start_parser.add_argument('--encryption',
                              choices=[t.value for t in EncryptionType],
                              help='Encryption type')
    start_parser.add_argument('--channel', type=int, help='Channel number')
    start_parser.add_argument('--interface', help='Network interface')
    start_parser.add_argument('--band',
                              choices=[b.value for b in BandType],
                              help='WiFi band')
    start_parser.add_argument('--hidden', action='store_true',
                              help='Make the hotspot hidden (not broadcast)')
    start_parser.add_argument(
        '--max-clients', type=int, help='Maximum number of clients')

    # Stop command
    subparsers.add_parser('stop', help='Stop the WiFi hotspot')

    # Status command
    subparsers.add_parser('status', help='Show hotspot status')

    # List command
    subparsers.add_parser('list', help='List active connections')

    # Config command
    config_parser = subparsers.add_parser(
        'config', help='Update hotspot configuration')
    config_parser.add_argument('--name', help='Hotspot name')
    config_parser.add_argument('--password', help='Hotspot password')
    config_parser.add_argument('--authentication',
                               choices=[t.value for t in AuthenticationType],
                               help='Authentication type')
    config_parser.add_argument('--encryption',
                               choices=[t.value for t in EncryptionType],
                               help='Encryption type')
    config_parser.add_argument('--channel', type=int, help='Channel number')
    config_parser.add_argument('--interface', help='Network interface')
    config_parser.add_argument('--band',
                               choices=[b.value for b in BandType],
                               help='WiFi band')
    config_parser.add_argument('--hidden', action='store_true',
                               help='Make the hotspot hidden (not broadcast)')
    config_parser.add_argument(
        '--max-clients', type=int, help='Maximum number of clients')

    # Restart command
    restart_parser = subparsers.add_parser(
        'restart', help='Restart the WiFi hotspot')
    restart_parser.add_argument('--name', help='Hotspot name')
    restart_parser.add_argument('--password', help='Hotspot password')
    restart_parser.add_argument('--authentication',
                                choices=[t.value for t in AuthenticationType],
                                help='Authentication type')
    restart_parser.add_argument('--encryption',
                                choices=[t.value for t in EncryptionType],
                                help='Encryption type')
    restart_parser.add_argument('--channel', type=int, help='Channel number')
    restart_parser.add_argument('--interface', help='Network interface')
    restart_parser.add_argument('--band',
                                choices=[b.value for b in BandType],
                                help='WiFi band')
    restart_parser.add_argument('--hidden', action='store_true',
                                help='Make the hotspot hidden (not broadcast)')
    restart_parser.add_argument(
        '--max-clients', type=int, help='Maximum number of clients')

    # Interfaces command
    subparsers.add_parser(
        'interfaces', help='List available network interfaces')

    # Clients command
    clients_parser = subparsers.add_parser(
        'clients', help='List connected clients')
    clients_parser.add_argument('--monitor', action='store_true',
                                help='Continuously monitor clients')
    clients_parser.add_argument('--interval', type=int, default=5,
                                help='Monitoring interval in seconds')

    # Channels command
    channels_parser = subparsers.add_parser(
        'channels', help='List available WiFi channels')
    channels_parser.add_argument(
        '--interface', help='Network interface to check')

    # Add global verbose flag
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Enable verbose logging')

    # Parse arguments
    args = parser.parse_args()

    # If no arguments were provided, show help
    if not args.action:
        parser.print_help()
        return 1

    # Create the hotspot manager
    manager = HotspotManager(
        log_level=logging.DEBUG if args.verbose else logging.INFO)

    # Process commands using pattern matching (Python 3.10+)
    match args.action:
        case 'start':
            # Collect parameters for start command
            params = {}
            for param in ['name', 'password', 'authentication', 'encryption',
                          'channel', 'interface', 'band', 'hidden', 'max_clients']:
                if hasattr(args, param) and getattr(args, param) is not None:
                    params[param] = getattr(args, param)

            # Convert string enum values to actual enums
            if 'authentication' in params:
                params['authentication'] = AuthenticationType(
                    params['authentication'])
            if 'encryption' in params:
                params['encryption'] = EncryptionType(params['encryption'])
            if 'band' in params:
                params['band'] = BandType(params['band'])

            success = manager.start(**params)
            return 0 if success else 1

        case 'stop':
            success = manager.stop()
            return 0 if success else 1

        case 'status':
            manager.status()
            return 0

        case 'list':
            manager.list()
            return 0

        case 'config':
            # Collect parameters for config command
            params = {}
            for param in ['name', 'password', 'authentication', 'encryption',
                          'channel', 'interface', 'band', 'hidden', 'max_clients']:
                if hasattr(args, param) and getattr(args, param) is not None:
                    params[param] = getattr(args, param)

            # Convert string enum values to actual enums
            if 'authentication' in params:
                params['authentication'] = AuthenticationType(
                    params['authentication'])
            if 'encryption' in params:
                params['encryption'] = EncryptionType(params['encryption'])
            if 'band' in params:
                params['band'] = BandType(params['band'])

            success = manager.set(**params)
            return 0 if success else 1

        case 'restart':
            # Collect parameters for restart command
            params = {}
            for param in ['name', 'password', 'authentication', 'encryption',
                          'channel', 'interface', 'band', 'hidden', 'max_clients']:
                if hasattr(args, param) and getattr(args, param) is not None:
                    params[param] = getattr(args, param)

            # Convert string enum values to actual enums
            if 'authentication' in params:
                params['authentication'] = AuthenticationType(
                    params['authentication'])
            if 'encryption' in params:
                params['encryption'] = EncryptionType(params['encryption'])
            if 'band' in params:
                params['band'] = BandType(params['band'])

            success = manager.restart(**params)
            return 0 if success else 1

        case 'interfaces':
            interfaces = manager.get_network_interfaces()
            if interfaces:
                print("**Available network interfaces:**")
                for interface in interfaces:
                    print(
                        f"- {interface['name']} ({interface['type']}): {interface['state']}")
            else:
                print("No network interfaces found")
            return 0

        case 'clients':
            if args.monitor:
                # Run asynchronously for monitoring
                try:
                    print("Monitoring clients... Press Ctrl+C to stop")
                    asyncio.run(manager.monitor_clients(
                        interval=args.interval))
                except KeyboardInterrupt:
                    print("\nMonitoring stopped")
            else:
                # Just show current clients
                clients = manager.get_connected_clients()
                if clients:
                    print(f"**{len(clients)} clients connected:**")
                    for client in clients:
                        ip = client.get('ip_address', 'Unknown IP')
                        hostname = client.get('hostname', '')
                        if hostname:
                            print(
                                f"- {client['mac_address']} ({ip}) - {hostname}")
                        else:
                            print(f"- {client['mac_address']} ({ip})")
                else:
                    print("No clients connected")
            return 0

        case 'channels':
            interface = args.interface or manager.current_config.interface
            channels = manager.get_available_channels(interface)
            if channels:
                print(f"**Available channels for {interface}:**")
                print(", ".join(map(str, channels)))
            else:
                print(f"No channel information available for {interface}")
            return 0

        case _:
            parser.print_help()
            return 1


if __name__ == "__main__":
    sys.exit(main())
