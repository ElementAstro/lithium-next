#!/usr/bin/env python3
"""
Enhanced data models for WiFi Hotspot Manager with modern Python features.

This module provides type-safe, performance-optimized data models using the latest
Python features including Pydantic v2, StrEnum, and comprehensive validation.
"""

from __future__ import annotations

import time
from enum import StrEnum
from pathlib import Path
from typing import Any, Dict, List, Optional, Self, Union
from dataclasses import dataclass, field

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator
from loguru import logger


class AuthenticationType(StrEnum):
    """
    Authentication types supported for WiFi hotspots using StrEnum for better serialization.

    Each type represents a different security protocol that can be used
    to secure the hotspot connection.
    """
    WPA_PSK = "wpa-psk"    # WPA Personal
    WPA2_PSK = "wpa2-psk"  # WPA2 Personal  
    WPA3_SAE = "wpa3-sae"  # WPA3 Personal with SAE
    NONE = "none"          # Open network (no authentication)

    def __str__(self) -> str:
        """Return human-readable string representation."""
        return {
            self.WPA_PSK: "WPA Personal",
            self.WPA2_PSK: "WPA2 Personal", 
            self.WPA3_SAE: "WPA3 Personal (SAE)",
            self.NONE: "Open Network"
        }[self]

    @property
    def is_secure(self) -> bool:
        """Check if this authentication type provides security."""
        return self != AuthenticationType.NONE

    @property
    def requires_password(self) -> bool:
        """Check if this authentication type requires a password."""
        return self.is_secure


class EncryptionType(StrEnum):
    """
    Encryption algorithms for securing WiFi traffic using StrEnum.

    These encryption methods are used to protect data transmitted over
    the wireless network.
    """
    AES = "aes"    # Advanced Encryption Standard
    TKIP = "tkip"  # Temporal Key Integrity Protocol
    CCMP = "ccmp"  # Counter Mode with CBC-MAC Protocol (AES-based)

    def __str__(self) -> str:
        """Return human-readable string representation."""
        return {
            self.AES: "AES (Advanced Encryption Standard)",
            self.TKIP: "TKIP (Temporal Key Integrity Protocol)",
            self.CCMP: "CCMP (Counter Mode CBC-MAC Protocol)"
        }[self]

    @property
    def is_modern(self) -> bool:
        """Check if this is a modern encryption standard."""
        return self in {EncryptionType.AES, EncryptionType.CCMP}


class BandType(StrEnum):
    """
    WiFi frequency bands that can be used for the hotspot using StrEnum.

    Different bands offer different ranges and speeds.
    """
    G_ONLY = "bg"   # 2.4 GHz band
    A_ONLY = "a"    # 5 GHz band
    DUAL = "any"    # Both bands

    def __str__(self) -> str:
        """Return human-readable string representation."""
        return {
            self.G_ONLY: "2.4 GHz Only",
            self.A_ONLY: "5 GHz Only", 
            self.DUAL: "Dual Band (2.4/5 GHz)"
        }[self]

    @property
    def frequency_ghz(self) -> str:
        """Get frequency range in GHz."""
        return {
            self.G_ONLY: "2.4",
            self.A_ONLY: "5.0",
            self.DUAL: "2.4/5.0"
        }[self]


class HotspotConfig(BaseModel):
    """
    Enhanced configuration parameters for a WiFi hotspot using Pydantic v2.

    This class provides type validation, serialization, and comprehensive
    configuration management for WiFi hotspot settings.
    """
    
    model_config = ConfigDict(
        # Enable strict validation and forbid extra fields
        extra='forbid',
        # Use enum values in serialization
        use_enum_values=True,
        # Validate assignment after initialization
        validate_assignment=True,
        # Allow field title customization
        populate_by_name=True,
        # JSON schema configuration
        json_schema_extra={
            "examples": [
                {
                    "name": "MySecureHotspot",
                    "password": "securepassword123",
                    "authentication": "wpa2-psk",
                    "encryption": "aes",
                    "channel": 6,
                    "max_clients": 10,
                    "interface": "wlan0",
                    "band": "bg",
                    "hidden": False
                }
            ]
        }
    )

    name: str = Field(
        default="MyHotspot",
        min_length=1,
        max_length=32,
        description="SSID (network name) for the hotspot",
        examples=["MyHotspot", "Office-WiFi"]
    )
    
    password: Optional[str] = Field(
        default=None,
        min_length=8,
        max_length=63,
        description="Password for securing the hotspot (required for secured networks)",
        examples=["securepassword123"]
    )
    
    authentication: AuthenticationType = Field(
        default=AuthenticationType.WPA2_PSK,
        description="Authentication method for the hotspot"
    )
    
    encryption: EncryptionType = Field(
        default=EncryptionType.AES,
        description="Encryption algorithm for the hotspot"
    )
    
    channel: int = Field(
        default=11,
        ge=1,
        le=14,
        description="WiFi channel (1-14 for 2.4GHz, auto-selected for 5GHz)"
    )
    
    max_clients: int = Field(
        default=10,
        ge=1,
        le=50,
        description="Maximum number of concurrent clients"
    )
    
    interface: str = Field(
        default="wlan0",
        pattern=r"^[a-zA-Z0-9]+$",
        description="Network interface to use for the hotspot",
        examples=["wlan0", "wlp3s0"]
    )
    
    band: BandType = Field(
        default=BandType.G_ONLY,
        description="Frequency band to use for the hotspot"
    )
    
    hidden: bool = Field(
        default=False,
        description="Whether to hide the network SSID"
    )

    @field_validator('name')
    @classmethod
    def validate_name(cls, v: str) -> str:
        """Validate SSID name format."""
        if not v.strip():
            raise ValueError("Hotspot name cannot be empty or whitespace only")
        # Remove leading/trailing whitespace
        v = v.strip()
        # Check for invalid characters
        if any(char in v for char in ['"', '\\']):
            raise ValueError("Hotspot name cannot contain quotes or backslashes")
        return v

    @field_validator('password')
    @classmethod  
    def validate_password(cls, v: Optional[str]) -> Optional[str]:
        """Validate password strength and format."""
        if v is None:
            return None
        
        if len(v) < 8:
            raise ValueError("Password must be at least 8 characters long")
        
        # Check for basic password strength
        if v.isdigit() or v.isalpha() or v.islower() or v.isupper():
            logger.warning(
                "Weak password detected. Consider using a mix of letters, numbers, and symbols"
            )
        
        return v

    @field_validator('channel')
    @classmethod
    def validate_channel(cls, v: int, info) -> int:
        """Validate WiFi channel based on band type."""
        # For 2.4GHz, channels 1-14 are valid (14 in some regions)
        # For 5GHz, channels are auto-selected by NetworkManager
        if 'band' in info.data:
            band = info.data['band']
            if band == BandType.G_ONLY and not (1 <= v <= 14):
                raise ValueError("2.4GHz channels must be between 1 and 14")
        return v

    @model_validator(mode='after')
    def validate_security_config(self) -> Self:
        """Validate that security configuration is consistent."""
        if self.authentication.requires_password and not self.password:
            raise ValueError(
                f"Password is required for {self.authentication} authentication"
            )
        
        if self.authentication == AuthenticationType.NONE and self.password:
            logger.warning("Password specified but authentication is set to 'none'")
            
        return self

    def to_dict(self) -> Dict[str, Any]:
        """Convert configuration to a dictionary for serialization."""
        return self.model_dump(mode='json')

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> HotspotConfig:
        """Create a configuration object from a dictionary with validation."""
        return cls.model_validate(data)

    @classmethod
    def from_file(cls, file_path: Union[str, Path]) -> HotspotConfig:
        """Load configuration from a JSON file."""
        import json
        
        path = Path(file_path)
        if not path.exists():
            raise FileNotFoundError(f"Configuration file not found: {path}")
        
        try:
            with path.open('r', encoding='utf-8') as f:
                data = json.load(f)
            return cls.from_dict(data)
        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid JSON in configuration file: {e}") from e

    def save_to_file(self, file_path: Union[str, Path]) -> None:
        """Save configuration to a JSON file."""
        import json
        
        path = Path(file_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        
        with path.open('w', encoding='utf-8') as f:
            json.dump(self.to_dict(), f, indent=2)
        
        logger.info(f"Configuration saved to {path}")

    def is_compatible_with_interface(self, interface: str) -> bool:
        """Check if configuration is compatible with a network interface."""
        # This is a placeholder - in a real implementation, you'd check
        # interface capabilities using system tools
        return interface.startswith(('wlan', 'wlp'))


@dataclass(frozen=True, slots=True)
class CommandResult:
    """
    Immutable result of a command execution with enhanced error context.

    Uses slots for memory efficiency and frozen=True for immutability.
    """
    success: bool
    stdout: str = ""
    stderr: str = ""
    return_code: int = 0
    command: List[str] = field(default_factory=list)
    execution_time: float = 0.0
    timestamp: float = field(default_factory=time.time)

    def __post_init__(self) -> None:
        """Validate command result data."""
        if self.execution_time < 0:
            raise ValueError("execution_time cannot be negative")

    @property
    def output(self) -> str:
        """Get combined output (stdout + stderr)."""
        return f"{self.stdout}\n{self.stderr}".strip()

    @property
    def failed(self) -> bool:
        """Check if the command failed."""
        return not self.success

    @property
    def command_str(self) -> str:
        """Get command as a single string."""
        return " ".join(self.command)

    def log_result(self, level: str = "DEBUG") -> None:
        """Log the command result with appropriate level."""
        log_func = getattr(logger, level.lower(), logger.debug)
        
        if self.success:
            log_func(f"Command succeeded: {self.command_str}")
        else:
            log_func(
                f"Command failed with code {self.return_code}: {self.command_str}",
                extra={
                    "stdout": self.stdout,
                    "stderr": self.stderr,
                    "execution_time": self.execution_time
                }
            )

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "success": self.success,
            "stdout": self.stdout,
            "stderr": self.stderr,
            "return_code": self.return_code,
            "command": self.command,
            "execution_time": self.execution_time,
            "timestamp": self.timestamp
        }


class ConnectedClient(BaseModel):
    """
    Enhanced information about a client connected to the hotspot using Pydantic.
    """
    
    model_config = ConfigDict(
        extra='forbid',
        validate_assignment=True,
        str_strip_whitespace=True
    )

    mac_address: str = Field(
        pattern=r"^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$",
        description="MAC address of the connected client"
    )
    
    ip_address: Optional[str] = Field(
        default=None,
        pattern=r"^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$",
        description="IP address assigned to the client"
    )
    
    hostname: Optional[str] = Field(
        default=None,
        max_length=253,
        description="Hostname of the connected client"
    )
    
    connected_since: Optional[float] = Field(
        default=None,
        description="Timestamp when client connected"
    )
    
    data_transferred: int = Field(
        default=0,
        ge=0,
        description="Total bytes transferred by this client"
    )
    
    signal_strength: Optional[int] = Field(
        default=None,
        ge=-100,
        le=0,
        description="Signal strength in dBm"
    )

    @field_validator('mac_address')
    @classmethod
    def normalize_mac_address(cls, v: str) -> str:
        """Normalize MAC address format to lowercase with colons."""
        # Convert to lowercase and replace any separators with colons
        v = v.lower().replace('-', ':').replace('.', ':')
        return v

    @property
    def connection_duration(self) -> float:
        """Calculate how long the client has been connected in seconds."""
        if self.connected_since is None:
            return 0.0
        return time.time() - self.connected_since

    @property
    def connection_duration_str(self) -> str:
        """Get human-readable connection duration."""
        duration = self.connection_duration
        if duration < 60:
            return f"{duration:.0f}s"
        elif duration < 3600:
            return f"{duration/60:.0f}m"
        else:
            return f"{duration/3600:.1f}h"

    @property
    def is_active(self) -> bool:
        """Check if client is considered active (connected recently)."""
        return self.connection_duration < 300  # 5 minutes threshold

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary with additional computed fields."""
        data = self.model_dump()
        data.update({
            "connection_duration": self.connection_duration,
            "connection_duration_str": self.connection_duration_str,
            "is_active": self.is_active
        })
        return data


@dataclass(frozen=True, slots=True)
class NetworkInterface:
    """Information about a network interface that can be used for hotspots."""
    
    name: str
    type: str  # e.g., "wifi", "ethernet"
    state: str  # e.g., "connected", "disconnected", "unavailable"
    driver: Optional[str] = None
    capabilities: List[str] = field(default_factory=list)
    
    @property
    def is_wifi(self) -> bool:
        """Check if this is a WiFi interface."""
        return self.type.lower() == "wifi"
    
    @property
    def is_available(self) -> bool:
        """Check if interface is available for hotspot use."""
        return self.state.lower() in {"disconnected", "unmanaged"}
    
    @property
    def supports_ap_mode(self) -> bool:
        """Check if interface supports Access Point mode."""
        return "ap" in [cap.lower() for cap in self.capabilities]


class HotspotException(Exception):
    """Base exception for hotspot-related errors."""
    
    def __init__(self, message: str, *, error_code: Optional[str] = None, **kwargs: Any):
        super().__init__(message)
        self.error_code = error_code
        self.context = kwargs
        
        # Log the exception with context
        logger.error(
            f"HotspotException: {message}",
            extra={
                "error_code": error_code,
                "context": kwargs
            }
        )


class ConfigurationError(HotspotException):
    """Raised when there's an error in hotspot configuration."""
    pass


class NetworkManagerError(HotspotException):
    """Raised when there's an error communicating with NetworkManager."""
    pass


class InterfaceError(HotspotException):
    """Raised when there's an error with the network interface."""
    pass