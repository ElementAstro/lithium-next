"""
Lithium export declarations for nginx_manager module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from lithium_bridge import expose_command, expose_controller

from .manager import NginxManager


@expose_controller(
    endpoint="/api/nginx/status",
    method="GET",
    description="Get Nginx service status",
    tags=["nginx", "service"],
    version="1.0.0"
)
def get_nginx_status() -> dict:
    """
    Get the current status of Nginx service.

    Returns:
        Dictionary with Nginx status information
    """
    try:
        manager = NginxManager()
        status = manager.get_status()
        return {
            "running": status.get("running", False),
            "pid": status.get("pid"),
            "version": manager.get_version(),
            "success": True,
        }
    except Exception as e:
        return {
            "running": False,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/nginx/start",
    method="POST",
    description="Start Nginx service",
    tags=["nginx", "service"],
    version="1.0.0"
)
def start_nginx() -> dict:
    """
    Start the Nginx service.

    Returns:
        Dictionary with operation result
    """
    try:
        manager = NginxManager()
        result = manager.start_nginx()
        return {
            "action": "start",
            "success": result,
        }
    except Exception as e:
        return {
            "action": "start",
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/nginx/stop",
    method="POST",
    description="Stop Nginx service",
    tags=["nginx", "service"],
    version="1.0.0"
)
def stop_nginx() -> dict:
    """
    Stop the Nginx service.

    Returns:
        Dictionary with operation result
    """
    try:
        manager = NginxManager()
        result = manager.stop_nginx()
        return {
            "action": "stop",
            "success": result,
        }
    except Exception as e:
        return {
            "action": "stop",
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/nginx/reload",
    method="POST",
    description="Reload Nginx configuration",
    tags=["nginx", "service"],
    version="1.0.0"
)
def reload_nginx() -> dict:
    """
    Reload Nginx configuration without stopping the service.

    Returns:
        Dictionary with operation result
    """
    try:
        manager = NginxManager()
        result = manager.reload_nginx()
        return {
            "action": "reload",
            "success": result,
        }
    except Exception as e:
        return {
            "action": "reload",
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/nginx/config/check",
    method="GET",
    description="Check Nginx configuration syntax",
    tags=["nginx", "config"],
    version="1.0.0"
)
def check_config() -> dict:
    """
    Check Nginx configuration syntax.

    Returns:
        Dictionary with configuration check result
    """
    try:
        manager = NginxManager()
        valid, message = manager.check_config()
        return {
            "valid": valid,
            "message": message,
            "success": True,
        }
    except Exception as e:
        return {
            "valid": False,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/nginx/vhosts",
    method="GET",
    description="List virtual hosts",
    tags=["nginx", "vhost"],
    version="1.0.0"
)
def list_virtual_hosts() -> dict:
    """
    List all configured virtual hosts.

    Returns:
        Dictionary with virtual host list
    """
    try:
        manager = NginxManager()
        vhosts = manager.list_virtual_hosts()
        return {
            "vhosts": vhosts,
            "count": len(vhosts),
            "success": True,
        }
    except Exception as e:
        return {
            "vhosts": [],
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/nginx/vhost/create",
    method="POST",
    description="Create a virtual host",
    tags=["nginx", "vhost"],
    version="1.0.0"
)
def create_virtual_host(
    server_name: str,
    root_path: str,
    listen_port: int = 80,
    enable_ssl: bool = False,
) -> dict:
    """
    Create a new virtual host configuration.

    Args:
        server_name: Server name (domain)
        root_path: Document root path
        listen_port: Port to listen on
        enable_ssl: Enable SSL configuration

    Returns:
        Dictionary with creation result
    """
    try:
        manager = NginxManager()
        result = manager.create_virtual_host(
            server_name=server_name,
            root_path=root_path,
            listen_port=listen_port,
            enable_ssl=enable_ssl,
        )
        return {
            "server_name": server_name,
            "success": result,
        }
    except Exception as e:
        return {
            "server_name": server_name,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/nginx/health",
    method="GET",
    description="Perform health check",
    tags=["nginx", "monitoring"],
    version="1.0.0"
)
def health_check() -> dict:
    """
    Perform a comprehensive health check on Nginx.

    Returns:
        Dictionary with health check results
    """
    try:
        manager = NginxManager()
        health = manager.health_check()
        return {
            "healthy": health.get("healthy", False),
            "checks": health.get("checks", {}),
            "success": True,
        }
    except Exception as e:
        return {
            "healthy": False,
            "success": False,
            "error": str(e),
        }


@expose_command(
    command_id="nginx.status",
    description="Get Nginx status (command)",
    priority=5,
    timeout_ms=10000,
    tags=["nginx"]
)
def cmd_nginx_status() -> dict:
    """Get Nginx status via command dispatcher."""
    return get_nginx_status()


@expose_command(
    command_id="nginx.reload",
    description="Reload Nginx (command)",
    priority=5,
    timeout_ms=30000,
    tags=["nginx"]
)
def cmd_nginx_reload() -> dict:
    """Reload Nginx via command dispatcher."""
    return reload_nginx()


@expose_command(
    command_id="nginx.health",
    description="Nginx health check (command)",
    priority=5,
    timeout_ms=15000,
    tags=["nginx"]
)
def cmd_nginx_health() -> dict:
    """Nginx health check via command dispatcher."""
    return health_check()
