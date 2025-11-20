#!/usr/bin/env python3
"""
Hotspot Manager module for WiFi Hotspot Manager.
Contains the HotspotManager class which is responsible for managing WiFi hotspots.
"""

import time
import json
import re
import shutil
import asyncio
from pathlib import Path
from typing import Optional, List, Dict, Any, Callable

from loguru import logger
from .models import (
    HotspotConfig, AuthenticationType, EncryptionType, BandType, ConnectedClient
)
from .command_utils import run_command, run_command_async


class HotspotManager:
    """
    Manages WiFi hotspots using NetworkManager.

    This class provides a comprehensive interface to create, modify, and monitor
    WiFi hotspots through NetworkManager's command-line tools.
    """

    def __init__(self, config_dir: Optional[Path] = None):
        """
        Initialize the HotspotManager.

        Args:
            config_dir: Directory to store configuration files. If None, uses ~/.config/hotspot-manager
        """
        # Set up configuration directory
        if config_dir is None:
            self.config_dir = Path.home() / ".config" / "hotspot-manager"
        else:
            self.config_dir = Path(config_dir)

        self.config_dir.mkdir(parents=True, exist_ok=True)
        self.config_file = self.config_dir / "config.json"

        # Verify NetworkManager availability
        if not self._is_network_manager_available():
            logger.warning("NetworkManager is not available on this system")

        # Initialize with default configuration
        self.current_config = HotspotConfig()

        # Try to load saved config if available
        if self.config_file.exists():
            try:
                self.load_config()
                logger.debug(f"Loaded configuration from {self.config_file}")
            except Exception as e:
                logger.error(f"Failed to load configuration: {e}")

    def _is_network_manager_available(self) -> bool:
        """
        Check if NetworkManager is available on the system.

        Returns:
            True if NetworkManager is installed and available
        """
        return shutil.which("nmcli") is not None

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
            logger.error(f"Error saving configuration: {e}")
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
            logger.error(f"Error loading configuration: {e}")
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
                logger.debug(f"Updated config: {key} = {value}")
            else:
                logger.warning(f"Unknown configuration parameter: {key}")

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
                logger.error(
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

        result = run_command(cmd)

        if not result.success:
            return False

        # Configure additional settings after basic setup succeeds
        self._configure_hotspot()

        logger.info(f"Hotspot '{self.current_config.name}' is now running")

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
        run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless-security.key-mgmt',
            self.current_config.authentication.value
        ])

        # Set encryption for data protection
        run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless-security.pairwise',
            self.current_config.encryption.value
        ])

        run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless-security.group',
            self.current_config.encryption.value
        ])

        # Set frequency band (2.4GHz, 5GHz, or both)
        run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless.band',
            self.current_config.band.value
        ])

        # Set channel for broadcasting
        run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless.channel',
            str(self.current_config.channel)
        ])

        # Set MAC address behavior for consistent identification
        run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless.cloned-mac-address', 'stable'
        ])

        run_command([
            'nmcli', 'connection', 'modify', 'Hotspot',
            '802-11-wireless.mac-address-randomization', 'no'
        ])

        # Set hidden network status if configured
        if self.current_config.hidden:
            run_command([
                'nmcli', 'connection', 'modify', 'Hotspot',
                '802-11-wireless.hidden', 'yes'
            ])

    def stop(self) -> bool:
        """
        Stop the currently running hotspot.

        Returns:
            True if the hotspot was successfully stopped
        """
        result = run_command(['nmcli', 'connection', 'down', 'Hotspot'])
        if result.success:
            logger.info("Hotspot has been stopped")
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
        dev_status = run_command(['nmcli', 'dev', 'status'])
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
        conn_details = run_command(['nmcli', 'connection', 'show', 'Hotspot'])
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
        uptime_cmd = run_command([
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
        result = run_command(['nmcli', 'connection', 'show', '--active'])
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
            logger.info(f"Updated running hotspot configuration")

        # Save the configuration for future use
        self.save_config()
        logger.info(f"Hotspot configuration updated and saved")
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
            iw_cmd = run_command([
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
        arp_cmd = run_command(['arp', '-n'])
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
                logger.error(f"Error reading DHCP leases: {e}")

        return clients

    def get_network_interfaces(self) -> List[Dict[str, Any]]:
        """
        Get a list of available network interfaces.

        Returns:
            List of dictionaries with interface information
        """
        interfaces = []

        # Get list of interfaces using nmcli
        result = run_command(['nmcli', 'device', 'status'])
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
        result = run_command(['iwlist', interface, 'channel'])
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
            logger.error("Failed to stop hotspot for restart")
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
                    logger.info(f"New client connected: {mac}")

                for mac in disconnected_clients:
                    logger.info(f"Client disconnected: {mac}")

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
            logger.info("Client monitoring stopped")
        except Exception as e:
            logger.error(f"Error monitoring clients: {e}")
