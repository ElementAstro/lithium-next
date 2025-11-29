#!/usr/bin/env python3
"""
Data models for WiFi Hotspot Manager.
Contains enum classes and dataclasses used throughout the application.
"""

import time
from enum import Enum
from dataclasses import dataclass, asdict, field
from typing import Optional, List, Dict, Any


class AuthenticationType(Enum):
    """
    Authentication types supported for WiFi hotspots.

    Each type represents a different security protocol that can be used
    to secure the hotspot connection.
    """

    WPA_PSK = "wpa-psk"  # WPA Personal
    WPA2_PSK = "wpa2-psk"  # WPA2 Personal
    WPA3_SAE = "wpa3-sae"  # WPA3 Personal with SAE
    NONE = "none"  # Open network (no authentication)


class EncryptionType(Enum):
    """
    Encryption algorithms for securing WiFi traffic.

    These encryption methods are used to protect data transmitted over
    the wireless network.
    """

    AES = "aes"  # Advanced Encryption Standard
    TKIP = "tkip"  # Temporal Key Integrity Protocol
    CCMP = "ccmp"  # Counter Mode with CBC-MAC Protocol (AES-based)


class BandType(Enum):
    """
    WiFi frequency bands that can be used for the hotspot.

    Different bands offer different ranges and speeds.
    """

    G_ONLY = "bg"  # 2.4 GHz band
    A_ONLY = "a"  # 5 GHz band
    DUAL = "any"  # Both bands


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
