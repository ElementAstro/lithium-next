"""
Lithium export declarations for pacman_manager module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from lithium_bridge import expose_command, expose_controller

from .manager import PacmanManager


@expose_controller(
    endpoint="/api/pacman/install",
    method="POST",
    description="Install a package",
    tags=["pacman", "package"],
    version="1.0.0",
)
def install_package(package: str, aur: bool = False) -> dict:
    """
    Install a package using pacman.

    Args:
        package: Package name to install
        aur: Use AUR helper for installation

    Returns:
        Dictionary with installation result
    """
    try:
        manager = PacmanManager()
        if aur:
            result = manager.install_aur_package(package)
        else:
            result = manager.install_package(package)
        return {
            "package": package,
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "package": package,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/pacman/remove",
    method="POST",
    description="Remove a package",
    tags=["pacman", "package"],
    version="1.0.0",
)
def remove_package(package: str, cascade: bool = False) -> dict:
    """
    Remove a package using pacman.

    Args:
        package: Package name to remove
        cascade: Remove dependent packages

    Returns:
        Dictionary with removal result
    """
    try:
        manager = PacmanManager()
        result = manager.remove_package(package, cascade=cascade)
        return {
            "package": package,
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "package": package,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/pacman/search",
    method="GET",
    description="Search for packages",
    tags=["pacman", "package"],
    version="1.0.0",
)
def search_packages(query: str, aur: bool = False) -> dict:
    """
    Search for packages.

    Args:
        query: Search query
        aur: Include AUR packages

    Returns:
        Dictionary with search results
    """
    try:
        manager = PacmanManager()
        if aur:
            results = manager.search_aur_package(query)
        else:
            results = manager.search_package(query)
        return {
            "query": query,
            "results": [
                {
                    "name": p.name,
                    "version": p.version,
                    "description": p.description,
                }
                for p in results
            ],
            "count": len(results),
            "success": True,
        }
    except Exception as e:
        return {
            "query": query,
            "results": [],
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/pacman/info",
    method="GET",
    description="Get package information",
    tags=["pacman", "package"],
    version="1.0.0",
)
def get_package_info(package: str) -> dict:
    """
    Get detailed package information.

    Args:
        package: Package name

    Returns:
        Dictionary with package information
    """
    try:
        manager = PacmanManager()
        info = manager.show_package_info(package)
        return {
            "package": package,
            "info": {
                "name": info.name,
                "version": info.version,
                "description": info.description,
                "installed": info.installed,
                "size": info.size,
            },
            "success": True,
        }
    except Exception as e:
        return {
            "package": package,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/pacman/installed",
    method="GET",
    description="List installed packages",
    tags=["pacman", "package"],
    version="1.0.0",
)
def list_installed() -> dict:
    """
    List all installed packages.

    Returns:
        Dictionary with installed packages
    """
    try:
        manager = PacmanManager()
        packages = manager.list_installed_packages()
        return {
            "packages": [
                {
                    "name": p.name,
                    "version": p.version,
                }
                for p in packages
            ],
            "count": len(packages),
            "success": True,
        }
    except Exception as e:
        return {
            "packages": [],
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/pacman/upgrade",
    method="POST",
    description="Upgrade system packages",
    tags=["pacman", "system"],
    version="1.0.0",
)
def upgrade_system() -> dict:
    """
    Upgrade all system packages.

    Returns:
        Dictionary with upgrade result
    """
    try:
        manager = PacmanManager()
        result = manager.upgrade_system()
        return {
            "action": "upgrade",
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "action": "upgrade",
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/pacman/update",
    method="POST",
    description="Update package database",
    tags=["pacman", "system"],
    version="1.0.0",
)
def update_database() -> dict:
    """
    Update the package database.

    Returns:
        Dictionary with update result
    """
    try:
        manager = PacmanManager()
        result = manager.update_database()
        return {
            "action": "update",
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "action": "update",
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/pacman/clean",
    method="POST",
    description="Clean package cache",
    tags=["pacman", "maintenance"],
    version="1.0.0",
)
def clean_cache() -> dict:
    """
    Clean the package cache.

    Returns:
        Dictionary with clean result
    """
    try:
        manager = PacmanManager()
        result = manager.clear_cache()
        return {
            "action": "clean",
            "success": result.success,
            "message": result.message,
        }
    except Exception as e:
        return {
            "action": "clean",
            "success": False,
            "error": str(e),
        }


@expose_command(
    command_id="pacman.install",
    description="Install package (command)",
    priority=5,
    timeout_ms=300000,
    tags=["pacman"],
)
def cmd_install(package: str) -> dict:
    """Install package via command dispatcher."""
    return install_package(package)


@expose_command(
    command_id="pacman.search",
    description="Search packages (command)",
    priority=5,
    timeout_ms=30000,
    tags=["pacman"],
)
def cmd_search(query: str) -> dict:
    """Search packages via command dispatcher."""
    return search_packages(query)


@expose_command(
    command_id="pacman.upgrade",
    description="Upgrade system (command)",
    priority=3,
    timeout_ms=600000,
    tags=["pacman"],
)
def cmd_upgrade() -> dict:
    """Upgrade system via command dispatcher."""
    return upgrade_system()
