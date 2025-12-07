# Lithium Components

High-performance component management and dependency tracking library with modular architecture.

## Directory Structure

```text
src/components/
├── components.hpp          # Main aggregated header (entry point)
├── CMakeLists.txt          # Build configuration
├── README.md               # This document
│
├── core/                   # Core module
│   ├── core.hpp            # Core module aggregated header
│   ├── types.hpp           # Core types aggregated header
│   ├── dependency.hpp/.cpp # Dependency graph management
│   ├── loader.hpp/.cpp     # Module loader
│   ├── module.hpp          # Module information structure
│   ├── tracker.hpp/.cpp    # File tracker
│   └── version.hpp/.cpp    # Version management
│
├── manager/                # Component manager module
│   ├── manager_module.hpp  # Manager module aggregated header
│   ├── manager.hpp/.cpp    # Component manager implementation
│   ├── manager_impl.hpp/.cpp # Component manager internal implementation
│   ├── types.hpp           # Component type definitions
│   └── exceptions.hpp      # Exception definitions
│
├── debug/                  # Debug module
│   ├── debug.hpp           # Debug module aggregated header
│   ├── dump.hpp/.cpp       # Core dump analysis
│   ├── dynamic.hpp/.cpp    # Dynamic library parsing
│   └── elf.hpp/.cpp        # ELF file parsing (Linux only)
│
└── system/                 # System dependency module
    ├── system.hpp          # System module aggregated header
    ├── dependency_manager.hpp/.cpp
    ├── dependency_types.hpp/.cpp
    ├── dependency_exception.hpp
    ├── package_manager.hpp/.cpp
    └── platform_detector.hpp/.cpp
```

## 模块说明

### Core 模块 (`core/`)

核心功能组件，包含：

- **DependencyGraph**: 有向依赖图管理，支持循环检测、拓扑排序、版本冲突检测
- **ModuleLoader**: 动态模块加载和管理，支持异步加载和依赖验证
- **FileTracker**: 文件状态追踪，支持增量扫描和变更通知
- **Version**: 语义版本管理（SemVer），支持版本比较和范围检查
- **ModuleInfo**: 模块信息结构定义

### Debug 模块 (`debug/`)

调试和分析工具：

- **CoreDumpAnalyzer**: 核心转储文件分析
- **DynamicLibraryParser**: 动态库依赖解析
- **ElfParser**: ELF 文件解析 (仅 Linux)

### Manager 模块 (`manager/`)

组件生命周期管理：

- **ComponentManager**: 组件生命周期管理器，支持加载、启动、停止、暂停、恢复
- **ComponentEvent**: 组件事件枚举
- **ComponentState**: 组件状态枚举
- **ComponentOptions**: 组件配置选项

### System 模块 (`system/`)

系统依赖管理：

- **DependencyManager**: 系统依赖管理器
- **PackageManagerRegistry**: 包管理器注册表
- **PlatformDetector**: 平台检测器

## 使用方法

### 简单使用

```cpp
#include "components/components.hpp"

using namespace lithium;

// 创建模块加载器
auto loader = ModuleLoader::createShared();

// 加载模块
auto result = loader->loadModule("path/to/module.so", "module_name");
if (result) {
    // 获取模块
    auto module = loader->getModule("module_name");
}

// 创建组件管理器
auto manager = ComponentManager::createShared();

// 管理组件
manager->loadComponent(params);
manager->startComponent("component_name");
```

### 依赖图

```cpp
#include "components/core/dependency.hpp"

using namespace lithium;

DependencyGraph graph;
graph.addNode("package_a", Version{1, 0, 0});
graph.addNode("package_b", Version{2, 0, 0});
graph.addDependency("package_a", "package_b", Version{1, 5, 0});

if (graph.hasCycle()) {
    // 处理循环依赖
}

auto sorted = graph.topologicalSort();
```

### 文件追踪

```cpp
#include "components/core/tracker.hpp"

using namespace lithium;

std::vector<std::string> fileTypes = {".hpp", ".cpp"};
FileTracker tracker("/project/src", "state.json", fileTypes, true);

tracker.scan();
tracker.compare();
auto diff = tracker.getDifferences();
```

## Quick Start

For most use cases, include the main header:

```cpp
#include "components/components.hpp"

// All functionality is available under lithium namespace
using namespace lithium;

// Create ModuleLoader
auto loader = ModuleLoader::createShared();

// Create ComponentManager
auto manager = ComponentManager::createShared();

// Use system dependency management (in lithium::system namespace)
auto depManager = std::make_shared<system::DependencyManager>("config.json");
```

Or include specific module headers:

```cpp
// Core module only
#include "components/core/core.hpp"

// Manager module only
#include "components/manager/manager_module.hpp"

// Debug module only
#include "components/debug/debug.hpp"

// System module only
#include "components/system/system.hpp"
```

## Building

项目使用 CMake 构建系统：

```bash
cd build
cmake ..
cmake --build . --target lithium_components
```

## 依赖

- **atom**: 基础工具库
- **lithium_config**: 配置管理库
- **spdlog/loguru**: 日志库
- **yaml-cpp**: YAML 解析库
- **tinyxml2**: XML 解析库
- **nlohmann_json**: JSON 解析库

## 许可证

GPL3 License - Copyright (C) 2023-2024 Max Qian
