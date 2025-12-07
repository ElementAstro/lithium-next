"""
Lithium Bridge - Module Information System

This module provides comprehensive module metadata and documentation
for frontend discovery and management.
"""

from dataclasses import dataclass, field, asdict
from enum import Enum
from typing import Any, Dict, List, Optional
import json
from pathlib import Path


class ModuleCategory(str, Enum):
    """Module category classification."""

    SYSTEM = "system"  # System management tools
    NETWORK = "network"  # Network related tools
    SECURITY = "security"  # Security and certificate tools
    BUILD = "build"  # Build and compilation tools
    PACKAGE = "package"  # Package management tools
    DEVELOPMENT = "development"  # Development tools
    UTILITY = "utility"  # General utilities
    MONITORING = "monitoring"  # Monitoring and logging tools


class PlatformSupport(str, Enum):
    """Platform support levels."""

    WINDOWS = "windows"
    LINUX = "linux"
    MACOS = "macos"
    ALL = "all"


class ModuleStatus(str, Enum):
    """Module status."""

    STABLE = "stable"
    BETA = "beta"
    EXPERIMENTAL = "experimental"
    DEPRECATED = "deprecated"


@dataclass
class EndpointInfo:
    """Detailed endpoint information."""

    path: str
    method: str
    summary: str
    description: str = ""
    tags: List[str] = field(default_factory=list)
    parameters: List[Dict[str, Any]] = field(default_factory=list)
    request_body: Optional[Dict[str, Any]] = None
    responses: Dict[str, Any] = field(default_factory=dict)
    deprecated: bool = False
    requires_auth: bool = False
    rate_limit: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class CommandInfo:
    """Detailed command information."""

    command_id: str
    summary: str
    description: str = ""
    parameters: List[Dict[str, Any]] = field(default_factory=list)
    priority: int = 5
    timeout_ms: int = 30000
    async_supported: bool = True
    tags: List[str] = field(default_factory=list)
    deprecated: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class ModuleInfo:
    """Comprehensive module information for frontend."""

    # Basic info
    name: str
    display_name: str
    version: str
    description: str

    # Classification
    category: ModuleCategory
    tags: List[str] = field(default_factory=list)

    # Status and support
    status: ModuleStatus = ModuleStatus.STABLE
    platforms: List[PlatformSupport] = field(
        default_factory=lambda: [PlatformSupport.ALL]
    )
    min_python_version: str = "3.8"

    # Author info
    author: str = ""
    author_email: str = ""
    license: str = "GPL-3.0-or-later"
    homepage: str = ""
    repository: str = ""

    # Dependencies
    dependencies: List[str] = field(default_factory=list)
    optional_dependencies: List[str] = field(default_factory=list)
    system_requirements: List[str] = field(default_factory=list)

    # Exports
    endpoints: List[EndpointInfo] = field(default_factory=list)
    commands: List[CommandInfo] = field(default_factory=list)

    # Documentation
    documentation_url: str = ""
    changelog_url: str = ""
    examples: List[Dict[str, str]] = field(default_factory=list)

    # UI hints for frontend
    icon: str = ""  # Icon name or URL
    color: str = ""  # Theme color
    order: int = 0  # Display order
    hidden: bool = False  # Hide from UI

    # Capabilities
    capabilities: List[str] = field(default_factory=list)
    limitations: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        data = {
            "name": self.name,
            "display_name": self.display_name,
            "version": self.version,
            "description": self.description,
            "category": self.category.value,
            "tags": self.tags,
            "status": self.status.value,
            "platforms": [p.value for p in self.platforms],
            "min_python_version": self.min_python_version,
            "author": self.author,
            "author_email": self.author_email,
            "license": self.license,
            "homepage": self.homepage,
            "repository": self.repository,
            "dependencies": self.dependencies,
            "optional_dependencies": self.optional_dependencies,
            "system_requirements": self.system_requirements,
            "endpoints": [e.to_dict() for e in self.endpoints],
            "commands": [c.to_dict() for c in self.commands],
            "documentation_url": self.documentation_url,
            "changelog_url": self.changelog_url,
            "examples": self.examples,
            "icon": self.icon,
            "color": self.color,
            "order": self.order,
            "hidden": self.hidden,
            "capabilities": self.capabilities,
            "limitations": self.limitations,
            "stats": {
                "endpoint_count": len(self.endpoints),
                "command_count": len(self.commands),
            },
        }
        return data

    def to_json(self, indent: int = 2) -> str:
        """Convert to JSON string."""
        return json.dumps(self.to_dict(), indent=indent, ensure_ascii=False)

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "ModuleInfo":
        """Create from dictionary."""
        # Convert enums
        data["category"] = ModuleCategory(data.get("category", "utility"))
        data["status"] = ModuleStatus(data.get("status", "stable"))
        data["platforms"] = [PlatformSupport(p) for p in data.get("platforms", ["all"])]

        # Convert endpoints
        endpoints = []
        for ep in data.get("endpoints", []):
            endpoints.append(EndpointInfo(**ep))
        data["endpoints"] = endpoints

        # Convert commands
        commands = []
        for cmd in data.get("commands", []):
            commands.append(CommandInfo(**cmd))
        data["commands"] = commands

        return cls(**data)


@dataclass
class ModuleRegistry:
    """Registry of all modules with metadata."""

    name: str = "lithium-tools"
    version: str = "1.0.0"
    description: str = "Lithium Python tools collection"
    modules: List[ModuleInfo] = field(default_factory=list)

    # API info
    api_version: str = "1.0"
    base_url: str = "/api"

    # Frontend hints
    default_category: str = "utility"
    categories: List[Dict[str, str]] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "api_version": self.api_version,
            "base_url": self.base_url,
            "default_category": self.default_category,
            "categories": self.categories or self._default_categories(),
            "modules": [m.to_dict() for m in self.modules],
            "stats": {
                "module_count": len(self.modules),
                "total_endpoints": sum(len(m.endpoints) for m in self.modules),
                "total_commands": sum(len(m.commands) for m in self.modules),
            },
        }

    def _default_categories(self) -> List[Dict[str, str]]:
        """Get default category definitions."""
        return [
            {
                "id": "system",
                "name": "系统管理",
                "icon": "settings",
                "color": "#4CAF50",
            },
            {"id": "network", "name": "网络工具", "icon": "wifi", "color": "#2196F3"},
            {
                "id": "security",
                "name": "安全工具",
                "icon": "shield",
                "color": "#F44336",
            },
            {"id": "build", "name": "构建工具", "icon": "build", "color": "#FF9800"},
            {"id": "package", "name": "包管理", "icon": "package", "color": "#9C27B0"},
            {
                "id": "development",
                "name": "开发工具",
                "icon": "code",
                "color": "#00BCD4",
            },
            {"id": "utility", "name": "实用工具", "icon": "tool", "color": "#607D8B"},
            {
                "id": "monitoring",
                "name": "监控工具",
                "icon": "chart",
                "color": "#795548",
            },
        ]

    def to_json(self, indent: int = 2) -> str:
        """Convert to JSON string."""
        return json.dumps(self.to_dict(), indent=indent, ensure_ascii=False)

    def save(self, path: Path) -> None:
        """Save registry to file."""
        with open(path, "w", encoding="utf-8") as f:
            f.write(self.to_json())

    @classmethod
    def load(cls, path: Path) -> "ModuleRegistry":
        """Load registry from file."""
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)

        modules = []
        for m in data.get("modules", []):
            modules.append(ModuleInfo.from_dict(m))

        return cls(
            name=data.get("name", "lithium-tools"),
            version=data.get("version", "1.0.0"),
            description=data.get("description", ""),
            modules=modules,
            api_version=data.get("api_version", "1.0"),
            base_url=data.get("base_url", "/api"),
            default_category=data.get("default_category", "utility"),
            categories=data.get("categories", []),
        )

    def get_module(self, name: str) -> Optional[ModuleInfo]:
        """Get module by name."""
        for m in self.modules:
            if m.name == name:
                return m
        return None

    def get_modules_by_category(self, category: ModuleCategory) -> List[ModuleInfo]:
        """Get modules by category."""
        return [m for m in self.modules if m.category == category]

    def get_modules_by_platform(self, platform: PlatformSupport) -> List[ModuleInfo]:
        """Get modules supporting a platform."""
        return [
            m
            for m in self.modules
            if PlatformSupport.ALL in m.platforms or platform in m.platforms
        ]

    def search_modules(self, query: str) -> List[ModuleInfo]:
        """Search modules by name, description, or tags."""
        query = query.lower()
        results = []
        for m in self.modules:
            if (
                query in m.name.lower()
                or query in m.description.lower()
                or query in m.display_name.lower()
                or any(query in t.lower() for t in m.tags)
            ):
                results.append(m)
        return results

    def get_all_endpoints(self) -> List[Dict[str, Any]]:
        """Get all endpoints from all modules."""
        endpoints = []
        for m in self.modules:
            for ep in m.endpoints:
                ep_dict = ep.to_dict()
                ep_dict["module"] = m.name
                ep_dict["module_display_name"] = m.display_name
                endpoints.append(ep_dict)
        return endpoints

    def get_all_commands(self) -> List[Dict[str, Any]]:
        """Get all commands from all modules."""
        commands = []
        for m in self.modules:
            for cmd in m.commands:
                cmd_dict = cmd.to_dict()
                cmd_dict["module"] = m.name
                cmd_dict["module_display_name"] = m.display_name
                commands.append(cmd_dict)
        return commands


def create_module_info(
    name: str, display_name: str, description: str, category: ModuleCategory, **kwargs
) -> ModuleInfo:
    """Helper function to create ModuleInfo."""
    return ModuleInfo(
        name=name,
        display_name=display_name,
        version=kwargs.get("version", "1.0.0"),
        description=description,
        category=category,
        **{k: v for k, v in kwargs.items() if k != "version"},
    )
