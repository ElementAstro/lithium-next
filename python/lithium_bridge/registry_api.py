"""
Lithium Bridge - Registry API

This module provides HTTP API endpoints for frontend to access
module registry information.
"""

from pathlib import Path
from typing import Optional

from .exporter import expose_controller, expose_command
from .module_info import ModuleRegistry, ModuleCategory, PlatformSupport


# Global registry instance
_registry: Optional[ModuleRegistry] = None


def get_registry() -> ModuleRegistry:
    """Get or load the module registry."""
    global _registry
    if _registry is None:
        registry_path = Path(__file__).parent.parent / "tools" / "module_registry.json"
        if registry_path.exists():
            _registry = ModuleRegistry.load(registry_path)
        else:
            _registry = ModuleRegistry()
    return _registry


def reload_registry() -> ModuleRegistry:
    """Force reload the registry."""
    global _registry
    _registry = None
    return get_registry()


# ============================================================================
# HTTP API Endpoints
# ============================================================================


@expose_controller(
    endpoint="/api/registry/info",
    method="GET",
    description="获取模块注册表基本信息",
    tags=["registry", "frontend"],
    version="1.0.0",
)
def get_registry_info() -> dict:
    """
    获取模块注册表的基本信息。

    Returns:
        注册表元数据，包括名称、版本、模块数量等
    """
    registry = get_registry()
    return {
        "name": registry.name,
        "version": registry.version,
        "description": registry.description,
        "api_version": registry.api_version,
        "base_url": registry.base_url,
        "stats": {
            "module_count": len(registry.modules),
            "total_endpoints": sum(len(m.endpoints) for m in registry.modules),
            "total_commands": sum(len(m.commands) for m in registry.modules),
        },
        "success": True,
    }


@expose_controller(
    endpoint="/api/registry/categories",
    method="GET",
    description="获取所有模块分类",
    tags=["registry", "frontend"],
    version="1.0.0",
)
def get_categories() -> dict:
    """
    获取所有模块分类信息。

    Returns:
        分类列表，包括ID、名称、图标、颜色等
    """
    registry = get_registry()
    categories = registry.categories or registry._default_categories()

    # 统计每个分类的模块数
    category_stats = {}
    for m in registry.modules:
        cat = m.category.value
        category_stats[cat] = category_stats.get(cat, 0) + 1

    for cat in categories:
        cat["module_count"] = category_stats.get(cat["id"], 0)

    return {
        "categories": categories,
        "count": len(categories),
        "success": True,
    }


@expose_controller(
    endpoint="/api/registry/modules",
    method="GET",
    description="获取所有模块列表",
    tags=["registry", "frontend"],
    version="1.0.0",
)
def get_modules(
    category: str = None,
    platform: str = None,
    status: str = None,
    include_hidden: bool = False,
) -> dict:
    """
    获取模块列表，支持过滤。

    Args:
        category: 按分类过滤
        platform: 按平台过滤
        status: 按状态过滤
        include_hidden: 是否包含隐藏模块

    Returns:
        模块列表
    """
    registry = get_registry()
    modules = registry.modules

    # 过滤隐藏模块
    if not include_hidden:
        modules = [m for m in modules if not m.hidden]

    # 按分类过滤
    if category:
        try:
            cat = ModuleCategory(category)
            modules = [m for m in modules if m.category == cat]
        except ValueError:
            pass

    # 按平台过滤
    if platform:
        try:
            plat = PlatformSupport(platform)
            modules = [
                m
                for m in modules
                if PlatformSupport.ALL in m.platforms or plat in m.platforms
            ]
        except ValueError:
            pass

    # 按状态过滤
    if status:
        modules = [m for m in modules if m.status.value == status]

    # 按order排序
    modules = sorted(modules, key=lambda m: m.order)

    return {
        "modules": [
            {
                "name": m.name,
                "display_name": m.display_name,
                "version": m.version,
                "description": m.description,
                "category": m.category.value,
                "status": m.status.value,
                "platforms": [p.value for p in m.platforms],
                "icon": m.icon,
                "color": m.color,
                "tags": m.tags,
                "endpoint_count": len(m.endpoints),
                "command_count": len(m.commands),
            }
            for m in modules
        ],
        "count": len(modules),
        "success": True,
    }


@expose_controller(
    endpoint="/api/registry/module",
    method="GET",
    description="获取单个模块详细信息",
    tags=["registry", "frontend"],
    version="1.0.0",
)
def get_module_detail(name: str) -> dict:
    """
    获取单个模块的详细信息。

    Args:
        name: 模块名称

    Returns:
        模块详细信息
    """
    registry = get_registry()
    module = registry.get_module(name)

    if module is None:
        return {
            "success": False,
            "error": f"Module '{name}' not found",
        }

    return {
        "module": module.to_dict(),
        "success": True,
    }


@expose_controller(
    endpoint="/api/registry/endpoints",
    method="GET",
    description="获取所有API端点",
    tags=["registry", "frontend"],
    version="1.0.0",
)
def get_all_endpoints(
    module: str = None,
    tag: str = None,
) -> dict:
    """
    获取所有API端点列表。

    Args:
        module: 按模块过滤
        tag: 按标签过滤

    Returns:
        端点列表
    """
    registry = get_registry()
    endpoints = registry.get_all_endpoints()

    # 按模块过滤
    if module:
        endpoints = [e for e in endpoints if e.get("module") == module]

    # 按标签过滤
    if tag:
        endpoints = [e for e in endpoints if tag in e.get("tags", [])]

    return {
        "endpoints": endpoints,
        "count": len(endpoints),
        "success": True,
    }


@expose_controller(
    endpoint="/api/registry/commands",
    method="GET",
    description="获取所有命令",
    tags=["registry", "frontend"],
    version="1.0.0",
)
def get_all_commands(
    module: str = None,
    tag: str = None,
) -> dict:
    """
    获取所有命令列表。

    Args:
        module: 按模块过滤
        tag: 按标签过滤

    Returns:
        命令列表
    """
    registry = get_registry()
    commands = registry.get_all_commands()

    # 按模块过滤
    if module:
        commands = [c for c in commands if c.get("module") == module]

    # 按标签过滤
    if tag:
        commands = [c for c in commands if tag in c.get("tags", [])]

    return {
        "commands": commands,
        "count": len(commands),
        "success": True,
    }


@expose_controller(
    endpoint="/api/registry/search",
    method="GET",
    description="搜索模块",
    tags=["registry", "frontend"],
    version="1.0.0",
)
def search_modules(query: str) -> dict:
    """
    搜索模块。

    Args:
        query: 搜索关键词

    Returns:
        匹配的模块列表
    """
    registry = get_registry()
    results = registry.search_modules(query)

    return {
        "query": query,
        "results": [
            {
                "name": m.name,
                "display_name": m.display_name,
                "description": m.description,
                "category": m.category.value,
                "icon": m.icon,
                "color": m.color,
            }
            for m in results
        ],
        "count": len(results),
        "success": True,
    }


@expose_controller(
    endpoint="/api/registry/openapi",
    method="GET",
    description="获取OpenAPI规范",
    tags=["registry", "frontend"],
    version="1.0.0",
)
def get_openapi_spec() -> dict:
    """
    获取OpenAPI规范文档。

    Returns:
        OpenAPI 3.0规范
    """
    registry = get_registry()

    spec = {
        "openapi": "3.0.0",
        "info": {
            "title": registry.name,
            "version": registry.version,
            "description": registry.description,
        },
        "servers": [{"url": registry.base_url, "description": "API Server"}],
        "paths": {},
        "tags": [],
    }

    # 添加标签
    seen_tags = set()
    for m in registry.modules:
        for ep in m.endpoints:
            for tag in ep.tags:
                if tag not in seen_tags:
                    seen_tags.add(tag)
                    spec["tags"].append({"name": tag})

    # 添加路径
    for m in registry.modules:
        for ep in m.endpoints:
            path = ep.path
            method = ep.method.lower()

            if path not in spec["paths"]:
                spec["paths"][path] = {}

            spec["paths"][path][method] = {
                "summary": ep.summary,
                "description": ep.description,
                "tags": ep.tags,
                "operationId": f"{m.name}_{ep.path.replace('/', '_').strip('_')}",
                "responses": {"200": {"description": "Successful response"}},
            }

            # 添加参数
            if ep.parameters:
                params = []
                for p in ep.parameters:
                    params.append(
                        {
                            "name": p.get("name"),
                            "in": "query" if method == "get" else "body",
                            "required": p.get("required", False),
                            "schema": {"type": p.get("type", "string")},
                            "description": p.get("description", ""),
                        }
                    )
                if method == "get":
                    spec["paths"][path][method]["parameters"] = params

    return spec


@expose_controller(
    endpoint="/api/registry/reload",
    method="POST",
    description="重新加载注册表",
    tags=["registry", "admin"],
    version="1.0.0",
)
def reload_registry_endpoint() -> dict:
    """
    重新加载模块注册表。

    Returns:
        重载结果
    """
    try:
        registry = reload_registry()
        return {
            "success": True,
            "message": "Registry reloaded",
            "module_count": len(registry.modules),
        }
    except Exception as e:
        return {
            "success": False,
            "error": str(e),
        }


# ============================================================================
# Commands
# ============================================================================


@expose_command(
    command_id="registry.info",
    description="获取注册表信息",
    priority=5,
    timeout_ms=5000,
    tags=["registry"],
)
def cmd_registry_info() -> dict:
    """获取注册表信息。"""
    return get_registry_info()


@expose_command(
    command_id="registry.modules",
    description="获取模块列表",
    priority=5,
    timeout_ms=5000,
    tags=["registry"],
)
def cmd_get_modules() -> dict:
    """获取模块列表。"""
    return get_modules()


@expose_command(
    command_id="registry.search",
    description="搜索模块",
    priority=5,
    timeout_ms=5000,
    tags=["registry"],
)
def cmd_search(query: str) -> dict:
    """搜索模块。"""
    return search_modules(query)
